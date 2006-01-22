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
 * gc_malloc - Allocates memory that will be scanned for pointers.
 * gc_malloc_atomic - Allocates memory with content that wont be scanned.
 * gc_malloc_finalizer - Allocates scannable with finalizer function
 *                        that will be called when collected.
 * gc_atexit - Runs all finalizers and frees all objects.
 * gc_collect - Perform a collection now, instead of waiting until 
 *			malloc() returns NULL.
 * gc_add_root - Add foreign memory to scan.
 * gc_remove_root - Remove memory previously added with gc_add_root().
 */

#ifdef TEST
# include <stdio.h>
# include <unistd.h>
#endif

#include <stdlib.h>
#include "simple_gc.h"

struct root {
	char *base;
	unsigned int extent;
	struct root *next;
};

struct allocation {
	struct allocation *side[2];
	unsigned int state;
	unsigned int extent;	/* sizeof of struct + allocation length */
	void (*finalizer)(void *);
	/* char data[]; */
};
#define STATE_BALANCE   3		/* AVL balance property of the node */
#define STATE_MARKED	4		/* Marked during scan() */
#define STATE_ATOMIC	8		/* Guaranteed not to contain pointers */

#define NORMAL	0

#define IS_MARKED(a)	 ((a)->state & STATE_MARKED)
#define SET_MARK(a)	 (a)->state |= STATE_MARKED
#define CLEAR_MARK(a)	 (a)->state &= ~STATE_MARKED

#define IS_ATOMIC(a)	 ((a)->state & STATE_ATOMIC)

#define BALANCE_OF(a)	 ((a)->state & STATE_BALANCE) 
#define SET_BALANCE(a,b) (a)->state = ((a)->state & ~STATE_BALANCE) | (b)
#define LEFT	 0
#define RIGHT	 1
#define BALANCED 2

/* The allocation data immediately follows the allocation */
#define ALLOCATION_BASE(a) ((char *)(a) + sizeof (struct allocation))
#define ALLOCATION_LENGTH(a) ((a)->extent - sizeof (struct allocation))
#define ALLOCATION_END(a) ((char *)(a) + (a)->extent) 

#define ALIGN(a)	((a) & ~(sizeof (void *) - 1))

static void sweep(void);
static void sweep_sub(struct allocation *);
static void run_finalizers(void);
static void scan(char *base, unsigned int len);
static void allocation_insert(struct allocation *);
static void allocation_free(struct allocation *);
static void *allocate(unsigned int sz, int state, void (*finalizer)(void *));

static char *stack_top;				/* top of stack */
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
	    if (BALANCE_OF(*n) != BALANCED)
	    	last_imbalance = n;
	    n = &(*n)->side[new_node < *n];
	}
	*n = new_node;

	if (last_imbalance) {
	    n = last_imbalance;
	    if (BALANCE_OF(*n) == BALANCED) {
	    	/* nothing to do, root node is balanced */
	    } else {
	    	unsigned int dir = new_node < *n;
		struct allocation **n1 = &(*n)->side[dir];
	        if (BALANCE_OF(*n) != dir) {
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

/* Scans the stack, BSS and user roots and then sweeps the allocation tree */
void
gc_collect() 
{
	char *stack_bottom;
	struct root *root;
	extern char _edata, _end;

	/*
	 * Phase 1: Scan and mark
	 */

	if (stack_top) {
	    /* Scan the stack of an intel process */
	    __asm__("mov %%ebp, %0" : "=g"(stack_bottom));
	    scan(stack_bottom, stack_top - stack_bottom);
	}

	/*
	 * Scan the BSS of an a.out executable. These
	 * _e* variables are defined on Unix systems with 
	 * a.out 
	 */
	scan(&_edata, &_end - &_edata);

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
	     (char *)p < base + len; 
	     p++)
	{
#if 0 /* ifdef TEST */
	    static unsigned int count;
	    if (count++ % 1024)
	    	putchar('.');
#endif
	    if (*p >= allocation_min && *p < allocation_max)
	    {
	    	struct allocation *a = allocation_root;
	        while (a) 
			if (*p < (char *)a)
				a = a->side[RIGHT];
			else if (*p >= ALLOCATION_END(a))
				a = a->side[LEFT];
			else {
			    /* Discovered a pointer into the data */
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

/* Creates a new garbage-collectable allocation and adds to tree */
static void *
allocate(len, init_state, finalizer)
	unsigned int len;
	int init_state;
	void (*finalizer)(void *);
{
	struct allocation *newa;

	/* Don't allocate empty objects */
	if (len == 0) {
	    if (finalizer)
	    	(*finalizer)(NULL);
	    return NULL;
	}

	/* Collect when we have allocated twice as much as last time */
	if (allocation_total_size >= collect_at) {
	    gc_collect();
	    collect_at *= 2;
	}

	/* Call system malloc, or collect if it is exhausted */
	newa = (struct allocation *)malloc(sizeof *newa + len);
	if (!newa) {
	    gc_collect();
	    newa = (struct allocation *)malloc(sizeof *newa + len);
	    if (!newa)
	        return NULL;
	}

	/* Fill in the object's header and insert it */
	newa->extent = sizeof *newa + len;
	newa->state = init_state;
	newa->finalizer = finalizer;
	allocation_insert(newa);

	/* Accounting */
	allocation_total_size += ALLOCATION_LENGTH(newa);
	allocation_total_count++;
	if (!allocation_min || ALLOCATION_BASE(newa) < allocation_min)
		allocation_min = ALLOCATION_BASE(newa);
	if (ALLOCATION_END(newa) >= allocation_max)
		allocation_max = ALLOCATION_END(newa);

	return ALLOCATION_BASE(newa);
}

/* Called when an allocation is deemed unreachable */
static void
allocation_free(a)
	struct allocation *a;
{
	free(a);
}

void
gc_add_root(base, extent)
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

void
gc_remove_root(base)
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

void *
gc_malloc(len)
	unsigned int len;
{
	return allocate(len, 0, NULL);
}

void *
gc_malloc_atomic(len)
	unsigned int len;
{
	return allocate(len, STATE_ATOMIC, NULL);
}

void *
gc_malloc_finalizer(len, finalizer)
	unsigned int len;
	void (*finalizer)(void *);
{
	return allocate(len, 0, finalizer);
}

void
gc_atexit()
{
	sweep();
	run_finalizers();
	sweep();
	run_finalizers();
}

#ifdef TEST

static void myfinalizer(void *);
static void info(void);

static void
info()
{
	printf("[%u bytes in %u objects]\n\n",
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


	/* Record the current stack base for scanning the stack */
	__asm__("mov %%ebp, %0" : "=g"(stack_top));


	setbuf(stdout, NULL);
	atexit(info);
	atexit(gc_atexit);


	printf("calling: p = allocate(100)\n");
	p = gc_malloc(100);
	printf("  -> p = %p\n", p);
	info();

	printf("calling collect\n");
	gc_collect();
	info();

	printf("setting p = NULL\n");
	p = NULL;
	gc_collect();
	info();

#define COUNT 1024*8
#define SIZE  1024
	printf("allocating %u objects of size %u:\n", COUNT, SIZE);
	for (i = 0; i < COUNT; i++) {
	    printf("\r\t%7u", i);
	    gc_malloc(SIZE);
	}
        printf(" done\n");
	info();

	printf("calling collect\n");
	start = now();
	gc_collect();
	printf(" collect took %f seconds\n", now() - start);
	info();

	printf("calling malloc_finalizer(300, myfinalizer)\n");
	p2 = gc_malloc_finalizer(300, myfinalizer);
	info();

	exit(0);
}

#endif /* TEST */
