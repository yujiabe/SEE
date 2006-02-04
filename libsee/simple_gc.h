#ifndef _h_simple_gc_
#define _h_simple_gc_

void  sgc_collect(void);
void *sgc_malloc(unsigned int sz);
void *sgc_malloc_atomic(unsigned int sz);
void *sgc_malloc_finalizer(unsigned int sz, void (*finalizer)(void *));
void  sgc_atexit(void);
void  sgc_add_root(void *base, unsigned int sz);
void  sgc_remove_root(void *base);

#endif /* _h_simple_gc_ */
