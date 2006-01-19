#ifndef _h_simple_gc_
#define _h_simple_gc_

void  gc_collect(void);
void *gc_malloc(unsigned int sz);
void *gc_malloc_atomic(unsigned int sz);
void *gc_malloc_finalizer(unsigned int sz, void (*finalizer)(void *));
void  gc_atexit(void);
void  gc_add_root(void *base, unsigned int sz);
void  gc_remove_root(void *base);

#endif /* _h_simple_gc_ */
