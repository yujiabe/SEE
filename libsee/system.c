/*
 * Copyright (c) 2005
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

#if HAVE_TIME
# if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  if HAVE_SYS_TIME_H
#   include <sys/time.h>
#  else
#   include <time.h>
#  endif
# endif
#endif

#if HAVE_GC_H
# include <gc.h>
#else
# if HAVE_GC_MALLOC
extern void *GC_malloc(int);
# endif
# if HAVE_GC_MALLOC_ATOMIC
extern void *GC_malloc_atomic(int);
# endif
# if HAVE_GC_FREE
extern void GC_free(void *);
# endif
#endif

#include <see/system.h>
#include <see/interpreter.h>

#include "dprint.h"

/* Prototypes */
static void simple_abort(struct SEE_interpreter *, const char *) SEE_dead;
static unsigned int simple_random_seed(void);
#if HAVE_GC_MALLOC
static void *simple_gc_malloc(struct SEE_interpreter *, SEE_size_t);
static void *simple_gc_malloc_string(struct SEE_interpreter *, SEE_size_t);
static void simple_gc_free(struct SEE_interpreter *, void *);
#else
static void *simple_malloc(struct SEE_interpreter *, SEE_size_t);
static void simple_free(struct SEE_interpreter *, void *);
#endif
static void simple_mem_exhausted(struct SEE_interpreter *) SEE_dead;

/*
 * System defaults. This structure should be not be modified after
 * interpreters are created.
 */
struct SEE_system SEE_system = {
	NULL,				/* default_locale */
	-1,				/* default_recursion_limit */
	NULL,				/* default_trace */

	SEE_COMPAT_262_3B  		/* default_compat_flags */
	|SEE_COMPAT_EXT1,

	simple_random_seed,		/* random_seed */

	simple_abort,			/* abort */

#if HAVE_GC_MALLOC
	simple_gc_malloc,		/* malloc */
	simple_gc_malloc_string,	/* malloc_string */
	simple_gc_free,			/* free */
#else
	simple_malloc,			/* malloc */
	simple_malloc,			/* malloc_string */
	simple_free,			/* free */
#endif
	simple_mem_exhausted		/* mem_exhausted */
};

/*
 * A simple abort handler: we just try to print the error message,
 * and then die. Calling abort() will usually leave a core image that 
 * may be analysed for some port-mortem debugging.
 */
static void
simple_abort(interp, msg)
	struct SEE_interpreter *interp;		/* may be NULL */
	const char *msg;
{

#if STDC_HEADERS
	if (msg)
	    fprintf(stderr, "fatal error: %s\n", msg);
#endif

#if HAVE_ABORT
	abort();
#else
# ifndef NDEBUG
	dprintf("fatal error: %s\n", msg);
# endif
	exit(1);
#endif
}

/*
 * A simple random number seed generator. It is not thread safe.
 */
static unsigned int
simple_random_seed()
{
	static unsigned int counter = 0;
	unsigned int r;

	r = counter++;
#if HAVE_TIME
	r += (unsigned int)time(0);
#endif
	return r;
}

/*
 * A simple memory exhausted handler.
 */
static void
simple_mem_exhausted(interp)
	struct SEE_interpreter *interp;
{
	SEE_ABORT(interp, "memory exhausted");
}


#if HAVE_GC_MALLOC
/*
 * Memory allocator using Boehm GC
 */
static void *
simple_gc_malloc(interp, size)
	struct SEE_interpreter *interp;
	SEE_size_t size;
{
	return GC_malloc(size);
}

/*
 * Non-pointer memory allocator using Boehm GC
 */
static void *
simple_gc_malloc_string(interp, size)
	struct SEE_interpreter *interp;
	SEE_size_t size;
{
# if HAVE_GC_MALLOC_ATOMIC
	return GC_malloc_atomic(size);
# else
	return GC_malloc(size);
# endif
}

/*
 * Non-pointer memory allocator using Boehm GC
 */
static void
simple_gc_free(interp, ptr)
	struct SEE_interpreter *interp;
	void *ptr;
{
# if HAVE_GC_FREE
	GC_free(ptr);
# endif
}

#else /* !HAVE_GC_MALLOC */


/*
 * Memory allocator using system malloc().
 * Note: most mallocs do not get freed! 
 * System strongly assumes a garbage collector.
 * This is a stub function.
 */
static void *
simple_malloc(interp, size)
	struct SEE_interpreter *interp;
	SEE_size_t size;
{
#ifndef NDEBUG
	static int warning_printed = 0;

	if (!warning_printed) {
		warning_printed++;
		dprintf("WARNING: SEE is using non-release malloc\n(A garbage collector library is highly recommended.)");
	}

#endif
	return malloc(size);
}

/*
 * Memory deallocator using system free().
 */
static void
simple_free(interp, ptr)
	struct SEE_interpreter *interp;
	void *ptr;
{
	free(ptr);
}

#endif /* !HAVE_GC_MALLOC */

