/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_mem_
#define _SEE_h_mem_

#include <see/type.h>

struct SEE_interpreter;

void *	SEE_malloc(struct SEE_interpreter *i, SEE_size_t sz);
void *	SEE_malloc_string(struct SEE_interpreter *i, SEE_size_t sz);
void  	SEE_free(struct SEE_interpreter *i, void **memp);

#ifndef NDEBUG
/* Debugging variants */
void *	_SEE_malloc_debug(struct SEE_interpreter *i, SEE_size_t sz, 
		const char *file, int line, const char *arg);
void *	_SEE_malloc_string_debug(struct SEE_interpreter *i, SEE_size_t sz, 
		const char *file, int line, const char *arg);
void 	_SEE_free_debug(struct SEE_interpreter *i, void **memp,
		const char *file, int line, const char *arg);
#define SEE_malloc(i,s) \
		_SEE_malloc_debug(i,s,__FILE__,__LINE__,#s)
#define SEE_malloc_string(i,s) \
		_SEE_malloc_string_debug(i,s,__FILE__,__LINE__,#s)
#define SEE_free(i,p) \
		_SEE_free_debug(i,p,__FILE__,__LINE__,#p)
#endif

/* Convenience macros */
#define SEE_NEW(i, t)		(t *)SEE_malloc(i, sizeof (t))
#define SEE_NEW_ARRAY(i, t, n)	(t *)SEE_malloc(i, (n) * sizeof (t))
#define SEE_NEW_STRING_ARRAY(i, t, n) \
				(t *)SEE_malloc_string(i, (n) * sizeof (t))

#endif /* _SEE_h_mem_ */
