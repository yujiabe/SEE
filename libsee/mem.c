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
# include <stdlib.h>
#endif

#include <see/mem.h>
#include <see/interpreter.h>

/*
 * This module provides an interface abstraction for the memory
 * allocator. If configured properly, the default allocator is
 * the Boehm-GC collector. An application could see if a default
 * exists by testing (SEE_mem_malloc_hook != NULL).
 */

static void memory_exhausted(struct SEE_interpreter *) SEE_dead;

#if defined(HAVE_LIBGC)
static void *gc_malloc(struct SEE_interpreter *, unsigned int);
# define INITIAL_MALLOC		gc_malloc
# define INITIAL_FREE		NULL
#else
# define INITIAL_MALLOC		NULL
# define INITIAL_FREE		NULL
#endif

void* (*SEE_mem_malloc_hook)(struct SEE_interpreter *, unsigned int)
		= INITIAL_MALLOC;
void (*SEE_mem_free_hook)(struct SEE_interpreter *, void *) 
		= INITIAL_FREE;
void (*SEE_mem_exhausted_hook)(struct SEE_interpreter *) SEE_dead
		= memory_exhausted;

/*------------------------------------------------------------
 * Simple ehaustion strategy
 */

static void
memory_exhausted(interp)
	struct SEE_interpreter *interp;
{
	/* Call the interpreter's abort mechanism */
	(*SEE_abort)(interp);
}

#if defined HAVE_LIBGC
/*------------------------------------------------------------
 * Boehm-GC wrapper
 */

#include "gc.h"

static void *
gc_malloc(interp, size)
	struct SEE_interpreter *interp;
	unsigned int size;
{
	return GC_malloc(size);
}
#endif /* HAVE_LIBGC */

/*------------------------------------------------------------
 * wrappers around memory allocators that check for failur
 */

/*
 * Called to allocate size bytes of garbage-collected storage
 */
void *
SEE_malloc(interp, size)
	struct SEE_interpreter *interp;
	unsigned int size;
{
	void *data;

	if (size == 0)
		return NULL;
	data = (*SEE_mem_malloc_hook)(interp, size);
	if (data == NULL) 
		(*SEE_mem_exhausted_hook)(interp);
	return data;
}

/*
 * Called when we *know* that previously allocated storage
 * can be released. NOT RECOMMENDED TO BE USED.
 * (Much better is to allocate temp storage on the stack with SEE_ALLOCA().)
 */
void
SEE_free(interp, ptr)
	struct SEE_interpreter *interp;
	void *ptr;
{
	if (SEE_mem_free_hook)
		(*SEE_mem_free_hook)(interp, ptr);
}
