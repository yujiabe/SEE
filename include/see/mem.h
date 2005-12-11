/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_mem_
#define _SEE_h_mem_

#include <see/type.h>

struct SEE_interpreter;

/* Calls function at SEE_mem_malloc_hook */
void *	SEE_malloc(struct SEE_interpreter *i, unsigned int sz);

/* Calls function at SEE_mem_malloc_string_hook, or if null, SEE_mem_malloc */
void *	SEE_malloc_string(struct SEE_interpreter *i, unsigned int sz);

/* Calls function at SEE_mem_free_hook */
void  	SEE_free(struct SEE_interpreter *i, void *ptr);

/* Convenience macros */
#define SEE_NEW(i, t)		(t *)SEE_malloc(i, sizeof (t))
#define SEE_NEW_ARRAY(i, t, n)	(t *)SEE_malloc(i, (n) * sizeof (t))
#define SEE_NEW_STRING_ARRAY(i, t, n)	(t *)SEE_malloc_string(i, (n) * sizeof (t))

/* Allocator hooks. See usage document */
extern void *(*SEE_mem_malloc_hook)(struct SEE_interpreter *i, unsigned int sz);
extern void *(*SEE_mem_malloc_string_hook)(struct SEE_interpreter *i, unsigned int sz);
extern void  (*SEE_mem_free_hook)(struct SEE_interpreter *i, void *ptr);
extern void  (*SEE_mem_exhausted_hook)(struct SEE_interpreter *i) SEE_dead;

#endif /* _SEE_h_mem_ */
