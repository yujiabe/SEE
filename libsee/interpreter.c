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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by David Leonard and 
 *      contributors.
 * 4. Neither the name of Mr Leonard nor the names of the contributors
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
# include <see/config.h>
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

/* 
 * The interpreter context.
 */

/* obj_Array.c */
void SEE_Array_alloc(struct SEE_interpreter *);
void SEE_Array_init(struct SEE_interpreter *);

/* obj_Boolean.c */
void SEE_Boolean_alloc(struct SEE_interpreter *);
void SEE_Boolean_init(struct SEE_interpreter *);

/* obj_Date.c */
void SEE_Date_alloc(struct SEE_interpreter *);
void SEE_Date_init(struct SEE_interpreter *);

/* obj_Error.c */
void SEE_Error_alloc(struct SEE_interpreter *);
void SEE_Error_init(struct SEE_interpreter *);

/* obj_Function.c */
void SEE_Function_alloc(struct SEE_interpreter *);
void SEE_Function_init(struct SEE_interpreter *);

/* obj_Global.c */
void SEE_Global_alloc(struct SEE_interpreter *);
void SEE_Global_init(struct SEE_interpreter *);

/* obj_Math.c */
void SEE_Math_alloc(struct SEE_interpreter *);
void SEE_Math_init(struct SEE_interpreter *);

/* obj_Number.c */
void SEE_Number_alloc(struct SEE_interpreter *);
void SEE_Number_init(struct SEE_interpreter *);

/* obj_Object.c */
void SEE_Object_alloc(struct SEE_interpreter *);
void SEE_Object_init(struct SEE_interpreter *);

/* obj_RegExp.c */
void SEE_RegExp_alloc(struct SEE_interpreter *);
void SEE_RegExp_init(struct SEE_interpreter *);

/* obj_String.c */
void SEE_String_alloc(struct SEE_interpreter *);
void SEE_String_init(struct SEE_interpreter *);

static void interpreter_abort(struct SEE_interpreter *i) SEE_dead;

void (*SEE_abort)(struct SEE_interpreter *i) SEE_dead = interpreter_abort;

void
SEE_interpreter_init(interp)
	struct SEE_interpreter *interp;
{
	if (SEE_mem_malloc_hook == NULL) {
		fprintf(stderr, "SEE_mem_malloc_hook: not configured\n");
		(*SEE_abort)(interp);
	}

	interp->try_context = NULL;
	interp->try_location = NULL;

	interp->compatibility = SEE_COMPAT_262_3B 
			      | SEE_COMPAT_EXT1;
	interp->random_seed = (unsigned int)interp ^ (unsigned int)time(0);
	interp->trace = NULL;
	interp->traceback = NULL;

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

/* Handle a fatal interpreter fault. */
static void
interpreter_abort(i)
	struct SEE_interpreter *i;		/* may be NULL */
{
	abort();
}
