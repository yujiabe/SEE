/*
 * Copyright (c) 2006
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

/*
 * A simple mark-and-sweep GC that uses an AVL-tree and the system malloc.
 * Currently contains intel/unix-specific specific parts, that could
 * be replaced for other systems.
 *
 * sgc_malloc - Allocates memory that will be scanned for pointers.
 * sgc_malloc_atomic - Allocates memory with content that wont be scanned.
 * sgc_malloc_finalizer - Allocates scannable with finalizer function
 *                        that will be called when collected.
 * sgc_atexit - Runs all finalizers and frees all objects.
 * sgc_collect - Perform a collection now, instead of waiting until 
 *			malloc() returns NULL.
 * sgc_add_root - Add foreign memory to scan.
 * sgc_remove_root - Remove memory previously added with sgc_add_root().
 *
 * TODO:
 *   stack and BSS detection for different platforms
 */

#ifdef TEST
# include <stdio.h>
# include <unistd.h>
#endif

#if STDC_INCLUDES
# include <stdlib.h>
#endif

#include <see/try.h>

#include "simple_gc.h"

/* Linked list of roots. A root is a memory segment that is always reachable */
struct root {
	char *base;
	unsigned int extent;
	struct root *next;
};

/* Binary tree of allocations. Allocations are collectable memory segments */
struct allocation {
	struct allocation *side[2];
	unsigned int state;	/* mark state and AVL flags */
	unsigned int extent;	/* sizeof of struct + allocation length */
	void (*finalizer)(void *);
	/* char data[]; */
};

/* State flags */
#define STATE_AVL       3		/* AVL balance property of the node */
#define  LEFT	           0
#define  RIGHT	           1
#define  BALANCED          2
#define STATE_MARKED	4		/* Marked during scan() */
#define STATE_ATOMIC	8		/* Guaranteed not to contain pointers */

#define IS_MARKED(a)	 ((a)->state & STATE_MARKED)
#define SET_MARK(a)	 (a)->state |= STATE_MARKED
#define CLEAR_MARK(a)	 (a)->state &= ~STATE_MARKED
#define IS_ATOMIC(a)	 ((a)->state & STATE_ATOMIC)
#define GET_BALANCE(a)	 ((a)->state & STATE_AVL) 
#define SET_BALANCE(a,b) (a)->state = ((a)->state & ~STATE_AVL) | (b)

/* The allocation data immediately follows the allocation */
#define ALLOCATION_BASE(a) ((char *)(a) + sizeof (struct allocation))
#define ALLOCATION_LENGTH(a) ((a)->extent - sizeof (struct allocation))
#define ALLOCATION_END(a) ((char *)(a) + (a)->extent) 

#define ALIGN(a)	((a) & ~(sizeof (void *) - 1))

static void sweep(void);
static void sweep_sub(struct allocation *);
static void run_finalizers(void);
static void machdep_scan(void);
static void scan(char *base, unsigned int len);
static void allocation_insert(struct allocation *);
static void allocation_free(struct allocation *);
static void *allocate(unsigned int sz, int state, void (*finalizer)(void *));

static struct allocation *allocation_root;	/* allocated objects */
static struct allocation *allocation_finalizing; /* objects pending finalizer */
static char *allocation_min;
static char *allocation_max;
static unsigned int allocation_total_size;
static unsigned int allocation_total_count;
static unsigned int collect_at = 1024;		/* collect at this size */
static struct root *user_roots;

/*
 * Inserts a new allocation into the AVL binary tree. Takes O(log(n))
 * but is not recursive, and uses constant stack space.
 */
static void
allocation_insert(new_node)
	struct allocation *new_node;
{
	struct allocation **root = &allocation_root;
	struct allocation **last_imbalance = NULL;
	struct allocation **n;

	SET_BALANCE(new_node, BALANCED);
	new_node->side[RIGHT] = NULL;
	new_node->side[LEFT] = NULL;

	/*
	 * Search the tree, looking for the leaf to
	 * attach the new_node too. At the same time,
	 * we take note of the closest imbalanced
	 * ancestor.
	 */
	n = root;
	while (*n) {
	    if (GET_BALANCE(*n) != BALANCED)
	    	last_imbalance = n;
	    n = &(*n)->side[new_node < *n];
	}
	*n = new_node;

	if (last_imbalance) {
	    n = last_imbalance;
	    if (GET_BALANCE(*n) == BALANCED) {
	    	/* nothing to do, root node is balanced */
	    } else {
	    	unsigned int dir = new_node < *n;
		struct allocation **n1 = &(*n)->side[dir];
	        if (GET_BALANCE(*n) != dir) {
		    /* counter-imbalance is corrected to balanced */
		    SET_BALANCE(*n, BALANCED);
		    n = n1;
		} else {
		    unsigned int dir1 = new_node < *n1;
		    struct allocation **n2 = &(*n1)->side[dir1];
		    if (dir == dir1) {
			/* aggravated imbalance corrected by single rotation */
		        struct allocation **n2o = &(*n1)->side[!dir1];
		    	struct allocation *t = *n;
			*n = *n1;
			*n1 = *n2o;
			*n2o = t;
			n = n2;
			SET_BALANCE(t, BALANCED);
		    } else {
			/* aggravated imbalance corrected by double rotation */
		        struct allocation *p = (*n2)->side[dir1];
		        struct allocation *q = (*n2)->side[dir];
			unsigned int dir2 = new_node < *n2;
			(*n2)->side[dir1] = *n;
			(*n2)->side[dir] = *n1;
			if (*n2 == new_node) {
			    SET_BALANCE(*n, BALANCED);
			    SET_BALANCE(*n1, BALANCED);
			} else if (dir2 == dir1) {
			    SET_BALANCE(*n, BALANCED);
			    SET_BALANCE(*n1, dir);
			} else {
			    SET_BALANCE(*n, dir1);
			    SET_BALANCE(*n1, BALANCED);
			}
			*n = *n2;
			*n1 = p;
			*n2 = q;
			n = dir2 == dir1 ? n1 : n2;
		    }
		}
	    }
	} else
	    n = root;

	/* 
	 * Correct the remainder chain of balanced nodes by
	 * making them all imbalanced towards the new node
	 */
	while (*n && *n != new_node) {
	    SET_BALANCE(*n, new_node < *n);
	    n = &(*n)->side[new_node < *n];
	}


}

/*
 * Machine-dependent root scan.
 */
#if __unix__
static void
machdep_scan()
{
	/* 1. Stack */
#if __i386__ && __GNUC__
	/* Intel; assumes stack top never changes (ie no threads) */
	char *bottom;
	static char *top = NULL;
	__asm__("mov %%ebp, %0" : "=g"(bottom));
	if (!top) {
		char *sp = bottom;
		while (*(char **)sp > sp)
			sp = *(char **)sp;
		top = sp;
	}
	scan(bottom, top - bottom);
#else
  /* TODO - stack finders for other architectures */
 # warning "Unable to add stack to root set on this architecture"
#endif

	/* 2. Static variables */
#if __unix__ || unix
# if __linux__ || sun
#  define __fini _fini
# endif
# if hppa || __hppa__
#  define _init	__data_start
#  define __fini _edata
# endif
# define data_start	_init
# define data_end	__fini
# define bss_start	_edata
# define bss_end	_end
	{
	    extern char data_start[], data_end[], bss_start[], bss_end[];
	    scan(data_start, data_end - data_start);
	    scan(bss_start, bss_end - bss_start);
	}
#else /* not unix */
 # warning "Unable to add static storage to root set on this system"
#endif 

    /* Dynamically loaded libraries */
    /* TODO - detected shared libraries? */
}
#endif /* unix */

/* Scans the stack, BSS and user roots and then sweeps the allocation tree */
void
sgc_collect() 
{
	struct root *root;
	_SEE_JMPBUF jmpbuf;
#if TEST
	unsigned int osize = allocation_total_size;
	unsigned int ocount = allocation_total_count;
fprintf(stderr, "[collecting...");
#endif

	/*
	 * Phase 1: Scan and mark
	 */

	/* Scan the unspilt registers */
	_SEE_SETJMP(jmpbuf);
        scan((char *)&jmpbuf, sizeof jmpbuf);

	/* Scan the stack, data and BSS */
	machdep_scan();

	/* Scan the user-supplied roots */
	for (root = user_roots; root; root = root->next)
		scan(root->base, root->extent);

	/*
	 * Phase 2: Sweep and release
	 */

	/* Rebuild the allocation tree with marked nodes only */
	sweep();

	/* Deal with objects left on the finalizing list */
	run_finalizers();

#if TEST
fprintf(stderr, "freed %d objects %d bytes]\n", 
	ocount - allocation_total_count,
	osize - allocation_total_size);
#endif
}

/*
 * Scans the allocation tree, removing nodes that aren't marked, and
 * removing the mark from those that are. Runs in O(n.log(n)).
 * Calling this without a scan frees everything allocated.
 */
static void
sweep()
{
	struct allocation *old_root = allocation_root;
	
	allocation_root = NULL;
	if (old_root)
	    sweep_sub(old_root);
}

/* Note that this function recurses to a depth of approx 1.5 * log(n). */
static void
sweep_sub(a)
	struct allocation *a;
{
	struct allocation *side[2];

	side[0] = a->side[0];
	side[1] = a->side[1];

	if (!IS_MARKED(a)) {
	    allocation_total_size -= ALLOCATION_LENGTH(a);
	    allocation_total_count--;
	    if (a->finalizer) {
	    	/* Add to the list of finalizers to run later */
	        a->side[0] = allocation_finalizing;
	        allocation_finalizing = a;
	    } else
	        allocation_free(a);
	} else {
	    CLEAR_MARK(a);
	    allocation_insert(a);
	}
	if (side[0])
	    sweep_sub(side[0]);
	if (side[1])
	    sweep_sub(side[1]);
}

/* Finalize the allocations on the allocation_finalizing list then free them */
static void
run_finalizers()
{
	struct allocation *a, *anext;

	a = allocation_finalizing;
	allocation_finalizing = NULL;

	while (a) {
	    anext = a->side[0];
	    (*a->finalizer)((void *)ALLOCATION_BASE(a));
	    allocation_free(a);
	    a = anext;
	}
}

/* Scans a segment of memory and recursively marks/scans reached allocations */
static void
scan(base, len)
	char *base;
	unsigned int len;
{
	char **p;

	for (p = (char **)ALIGN((unsigned int)base); 
	     (char *)p + sizeof *p < base + len; 
	     p++)
	{
	    if (*p >= allocation_min && *p < allocation_max)
	    {
	    	/* Looks like a pointer into an allocation; search O(log(n)) */
	    	struct allocation *a = allocation_root;
	        while (a) 
			if (*p < (char *)a)
				a = a->side[RIGHT];
			else if (*p >= ALLOCATION_END(a))
				a = a->side[LEFT];
			else {
			    /* Found the allocation pointed to */
			    if (!IS_MARKED(a) && *p >= ALLOCATION_BASE(a)) {
				SET_MARK(a);
				if (!IS_ATOMIC(a))
				    scan(ALLOCATION_BASE(a), 
				         ALLOCATION_LENGTH(a));
			    }
			    break;
			}
	     }
	}
}

/* Creates a new allocation */
static void *
allocate(len, init_state, finalizer)
	unsigned int len;
	int init_state;
	void (*finalizer)(void *);
{
	struct allocation *newa;

	/* Don't allocate empty objects */
	if (len == 0) {
	    /* NB: finalizer is NOT run */
	    return NULL;
	}

	/* Collect when we have allocated twice as much as last time */
	if (allocation_total_size >= collect_at) {
	    sgc_collect();
	    collect_at = collect_at * 3 / 2;
	    /* if (collect_at < 1024) collect_at = 1024; */
	}

	/* Call system malloc; if exhausted, collect and retry */
	newa = (struct allocation *)malloc(sizeof *newa + len);
	if (!newa) {
	    sgc_collect();
	    newa = (struct allocation *)malloc(sizeof *newa + len);
	    if (!newa)
	        return NULL;
	}

	/* Fill in the allocation header and insert into allocation tree */
	newa->extent = sizeof *newa + len;
	newa->state = init_state;
	newa->finalizer = finalizer;
	allocation_insert(newa); /* O(log(n)) */

	/* Accounting */
	allocation_total_size += ALLOCATION_LENGTH(newa);
	allocation_total_count++;
	if (!allocation_min || ALLOCATION_BASE(newa) < allocation_min)
		allocation_min = ALLOCATION_BASE(newa);
	if (ALLOCATION_END(newa) >= allocation_max)
		allocation_max = ALLOCATION_END(newa);

	return ALLOCATION_BASE(newa);
}

/* Called when an allocation is deemed unreachable after sweeping */
static void
allocation_free(a)
	struct allocation *a;
{
	free(a);
}

/* Adds a memory segment to the list of roots scanned during collect */
void
sgc_add_root(base, extent)
	void *base;
	unsigned int extent;
{
	struct root *root;
	
	if (!extent)
	    return;
	root = (struct root *)malloc(sizeof (struct root));
	if (root) {
	    root->base = (char *)base;
	    root->extent = extent;
	    root->next = user_roots;
	    user_roots = root;
	}
}

/* Removes a previously added memory segment */
void
sgc_remove_root(base)
	void *base;
{
	struct root **r;

	for (r = &user_roots; *r; r = &(*r)->next)
	    if ((*r)->base == (char *)base) {
	    	struct root *root = *r;
		*r = root->next;
		free(root);
		return;
	    }
}

/* Allocates collectable memory */
void *
sgc_malloc(len)
	unsigned int len;
{
	return allocate(len, 0, NULL);
}

/* Allocates collectable memory caller guarantees never to contain pointers */
void *
sgc_malloc_atomic(len)
	unsigned int len;
{
	return allocate(len, STATE_ATOMIC, NULL);
}

/* Allocates collectable memory attaching a finalizer callback function */
void *
sgc_malloc_finalizer(len, finalizer)
	unsigned int len;
	void (*finalizer)(void *);
{
	return allocate(len, 0, finalizer);
}

/* Marks all objects as unreachable and runs finalizers */
void
sgc_atexit()
{
	sweep();
	run_finalizers();
	sweep();
	run_finalizers();
}

/*------------------------------------------------------------
 * Test stubs
 */
#ifdef TEST

static void myfinalizer(void *);
static void info(void);

static void
info()
{
	printf("[in use: %u bytes in %u objects]\n\n",
		allocation_total_size, allocation_total_count);
}

static void 
myfinalizer(p)
	void *p;
{
	printf("Finalizer called for %p!\n", p);
}

static double
now()
{
	struct timeval t;

	gettimeofday(&t, NULL);
	return (double)t.tv_sec + (double)t.tv_usec * 1e-6;
}

int
main()
{
	void *p, *p2;
	unsigned long i;
	double start, t;

	setbuf(stdout, NULL);
	atexit(info);
	atexit(sgc_atexit);

	printf("calling: p = allocate(100)\n");
	p = sgc_malloc(100);
	printf("  -> p = %p\n", p);
	info();

	printf("calling collect\n");
	sgc_collect();
	info();

	printf("setting p = NULL\n");
	p = NULL;
	sgc_collect();
	info();

#define COUNT 1024*256
#define SIZE  1024*2
	printf("allocating %u objects of size %u:\n", COUNT, SIZE);
	for (i = 0; i < COUNT; i++) {
/*	    printf("\r\t%7u", i); */
	    sgc_malloc(SIZE);
	}
/*	printf(" done\n"); */
	info();

	printf("calling collect\n");
	start = now();
	sgc_collect();
	printf(" collect took %f seconds\n", now() - start);
	info();

	printf("calling malloc_finalizer(300, myfinalizer)\n");
	p2 = sgc_malloc_finalizer(300, myfinalizer);
	info();

	exit(0);
}

#endif /* TEST */
