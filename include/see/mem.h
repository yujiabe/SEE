/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_mem_
#define _SEE_h_mem_

#include <see/type.h>

struct SEE_interpreter;

void *	SEE_malloc(struct SEE_interpreter *i, unsigned int sz);
void  	SEE_free(struct SEE_interpreter *i, void *ptr);

/* Convenience function */
#define SEE_NEW(i, t)		(t *)SEE_malloc(i, sizeof (t))
#define SEE_NEW_ARRAY(i, t, n)	(t *)SEE_malloc(i, (n) * sizeof (t))

void *(*SEE_mem_malloc_hook)(struct SEE_interpreter *i, unsigned int sz);
void  (*SEE_mem_free_hook)(struct SEE_interpreter *i, void *ptr);
void  (*SEE_mem_exhausted_hook)(struct SEE_interpreter *i) SEE_dead;

#endif /* _SEE_h_mem_ */
