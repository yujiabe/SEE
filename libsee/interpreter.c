/*
 * Copyright (c) 2003
 *      David Leonard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Mr Leonard nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID LEONARD AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID LEONARD OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id$ */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
# include <stdlib.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <see/mem.h>
#include <see/native.h>
#include <see/interpreter.h>
#include <see/cfunction.h>
#include <see/intern.h>

#include "context.h"
#include "init.h"

static void interpreter_abort(struct SEE_interpreter *i, const char *) SEE_dead;

/**
 * The
 * .Fn SEE_interpreter_init
 * function (re)initialises the interpreter structure pointed to by
 * .Fa interp
 * using default compatibility flags.
 */
void
SEE_interpreter_init(interp)
	struct SEE_interpreter *interp;
{
	SEE_interpreter_init_compat(interp, 
		SEE_COMPAT_262_3B |
		SEE_COMPAT_EXT1);
}

/**
 * The
 * .Fn SEE_interpreter_init_compat
 * function (re)initialises the interpreter structure pointed to by
 * .Fa interp ,
 * using the bitwise-OR of flags in
 * .Fa compat_flags .
 * The compatibility flags are documented in the
 * .Pa COMPATIBILITY
 * file.
 */
void
SEE_interpreter_init_compat(interp, compat_flags)
	struct SEE_interpreter *interp;
	int compat_flags;
{
	if (SEE_mem_malloc_hook == NULL)
		(*SEE_abort)(interp, "SEE_mem_malloc_hook is NULL");

	interp->try_context = NULL;
	interp->try_location = NULL;

	interp->compatibility = compat_flags;
	interp->random_seed = (unsigned int)interp ^ (unsigned int)time(0);
	interp->trace = NULL;
	interp->traceback = NULL;
	interp->locale = NULL;
	interp->recursion_limit = -1;

	/* Allocate object storage first, since dependencies are complex */
	SEE_Array_alloc(interp);
	SEE_Boolean_alloc(interp);
	SEE_Date_alloc(interp);
	SEE_Error_alloc(interp);
	SEE_Function_alloc(interp);
	SEE_Global_alloc(interp);
	SEE_Math_alloc(interp);
	SEE_Number_alloc(interp);
	SEE_Object_alloc(interp);
	SEE_RegExp_alloc(interp);
	SEE_String_alloc(interp);

	SEE_intern_init(interp);

	/* Initialise the objects; order *shouldn't* matter */
	SEE_Array_init(interp);
	SEE_Boolean_init(interp);
	SEE_Date_init(interp);
	SEE_Error_init(interp);
	SEE_Global_init(interp);
	SEE_Math_init(interp);
	SEE_Number_init(interp);
	SEE_Object_init(interp);
	SEE_RegExp_init(interp);
	SEE_String_init(interp);

	/* Function init needs to be called last since it uses the parser */
	SEE_Function_init(interp);
}

/**
 * The
 * .Fv SEE_abort
 * global variable points to a non-returning function that is called
 * when the interpreter detects a fatal error. It defaults to a function
 * that writes a message to stderr and then calls
 * .Xr abort 3 .
 * It should be set by the application if more graceful handling is
 * required.
 * Note that the first parameter to the function may be NULL
 */
void (*SEE_abort)(struct SEE_interpreter *i, const char *msg) SEE_dead =
	interpreter_abort;

/*
 * The default abort handler: we just print a fatal error message,
 * and die. Calling abort() will usually leave a core image that 
 * may be analysed for som port mortem debugging.
 */
static void
interpreter_abort(i, msg)
	struct SEE_interpreter *i;		/* may be NULL */
	const char *msg;
{
	if (msg)
	    fprintf(stderr, "fatal error: %s\n", msg);
	abort();
}
