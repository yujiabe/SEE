/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_try_
#define _h_try_

/*
 * Exception handling (try/catch)
 *
 * Usage example:
 *
 *	SEE_try_context_t c;
 *
 *	SEE_TRY(interp, c) {
 *		-- do code here that may call SEE_THROW()
 *		-- the try block breaks as soon as SEE_THROW happens
 *		-- NEVER 'return' or 'break' from inside a SEE_TRY!
 *		-- use TRY_BREAK or 'continue' to exit the SEE_TRY block.
 *	}
 *	-- 'finally' code gets run here always
 *	if (SEE_CAUGHT(c)) {
 *		-- this code run if a SEE_THROW was called during SEE_TRY 
 *		-- the exception thrown is SEE_CAUGHT(c)
 *		-- do a SEE_THROW(interp, SEE_CAUGHT(c)) if you can't handle it
 *	}
 *
 * If you only want a finally, and don't want to catch anything, use:
 *
 *	SEE_TRY(interp, c) {
 *		...
 *	}
 *	-- 'finally' code goes here
 *	SEE_DEFAULT_CATCH(interp, c);
 *
 * The signatures for the macros are:
 *
 *   struct SEE_value * SEE_CAUGHT(SEE_try_context_t);
 *   void               SEE_THROW(struct SEE_interpreter *, struct SEE_valuet *);
 *   void               SEE_DEFAULT_CATCH(SEE_interpreter *, SEE_try_context_t);
 *
 */

#include <setjmp.h>
#include "type.h"
#include "value.h"

/*
 * Macros
 */

#define SEE_TRY(interp, c) 					\
	    for ((c).previous = (interp)->try_context,		\
		 (interp)->try_context = &(c),			\
		 (c).interpreter = (interp),			\
		 SEE_SET_NULL(&(c).thrown),			\
		 (c).done = 0;					\
		 !(c).done && (_setjmp(				\
		    ((struct SEE_try_context *)&(c))->env) 	\
		   ? ((c).interpreter->try_context = 		\
		     (c).previous, 0)   			\
		   : 1);					\
		 (c).done = 1,					\
		 (c).interpreter->try_context = (c).previous)

#define TRY_BREAK						\
	    continue

#define SEE_CAUGHT(c)						\
	    ((struct SEE_value *)((c).done ? 0 : &(c).thrown))

#define SEE_THROW(interp, v) SEE__THROW(interp, v, __FILE__, __LINE__)

#define SEE__THROW(interp, v, _file, _line)			\
	    do {						\
		if (!(interp)->try_context)			\
			SEE_throw_abort(interp, __FILE__, 	\
				__LINE__);			\
		SEE_VALUE_COPY((struct SEE_value *)		\
			&(interp)->try_context->thrown, 	\
			(struct SEE_value *)v);			\
		(interp)->try_context->throw_file = _file;	\
		(interp)->try_context->throw_line = _line;	\
		SEE_throw();	/* debugger hook */		\
		_longjmp(((struct SEE_try_context *)		\
		      (interp)->try_context)->env, 1);		\
		/* NOTREACHED */				\
	    } while (0)

/* convenience macro */
#define SEE_DEFAULT_CATCH(interp, c)				\
	    do {						\
		if (!(c).done) 					\
		    SEE__THROW(interp, &(c).thrown,		\
			    (c).throw_file, (c).throw_line);	\
	    } while (0)

/*------------------------------------------------------------
 * private functions and externs used by the above macros
 */

struct SEE_throw_location {
	struct SEE_string *filename;		/* source location */
	int lineno;
};

struct SEE_try_context {
	struct SEE_interpreter *interpreter;
	volatile struct SEE_try_context *previous; /* try chain */
	struct SEE_value thrown;		/* what was thrown during try */
	int done;				/* true if try completed */
	jmp_buf env;				/* setjmp storage */
	const char *throw_file;			/* (debugging) */
	int throw_line;				/* (debugging) */
};

typedef struct SEE_try_context volatile SEE_try_context_t; 

struct SEE_interpreter;
void	SEE_throw_abort(struct SEE_interpreter *, const char *, int) SEE_dead;
struct SEE_string *SEE_location_string(struct SEE_interpreter *i,
		struct SEE_throw_location *loc);

/* Debugger hook for exceptions */
#ifndef NDEBUG
void	SEE_throw(void);
#else
# define SEE_throw() /* nothing */
#endif

#endif /* _h_try_ */
