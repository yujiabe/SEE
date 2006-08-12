/* Copyright (c) 2006, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_code_
#define _SEE_h_code_

struct SEE_interpreter;
struct SEE_value;

/*
 * A code stream for executing ECMAScript programs.
 * Code stream classes provide methods for generating code
 * in a single pass (with branch patching) and methods for
 * excuting the code. 
 */

struct SEE_code;

/* Call operators that take an argc as a parameter */
enum SEE_code_call_op {
	SEE_CODE_CALL,			/* ref arg0..argn | val */
	SEE_CODE_CONSTRUCT		/* obj arg0..argn | obj */
};

/* Generator operations; all parameters from the stack */
enum SEE_code_gen_op {
	SEE_CODE_DUP,			/* val | val val */
	SEE_CODE_POP,			/* val | - */
	SEE_CODE_THIS,			/* - | obj */
	SEE_CODE_REF,			/* obj str | ref */
	SEE_CODE_LOOKUP,		/* str | ref */
	SEE_CODE_PUT,			/* obj str val | - */
	SEE_CODE_GET,			/* obj str | val */
	SEE_CODE_DELETE,		/* ref | bool */
	SEE_CODE_TYPEOF,		/* ref | str */

	SEE_CODE_ADD,			/* num num | num  ;  str val | str */
	SEE_CODE_SUB,			/* num num | num */
	SEE_CODE_MUL,			/* num num | num */
	SEE_CODE_DIV,			/* num num | num */
	SEE_CODE_MOD,			/* num num | num */
	SEE_CODE_NEG,			/* num | num */
	SEE_CODE_INV,			/* val | num */
	SEE_CODE_NOT,			/* val | bool */

	SEE_CODE_GETVALUE,		/* ref | val */
	SEE_CODE_PUTVALUE,		/* ref val | - */

	SEE_CODE_TOOBJECT,		/* val | obj */
	SEE_CODE_TONUMBER,		/* val | num */
	SEE_CODE_TOBOOLEAN,		/* val | bool */
	SEE_CODE_TOSTRING,		/* val | str */
};

enum SEE_code_branch_op {
	SEE_CODE_BRANCH_ALWAYS
};

/* A branch target address in a code stream */
typedef void *SEE_code_addr_t;

/* Method table for the code stream */
struct SEE_code_class {
	const char *name;		/* Name of the code stream class */

	void	(*gen_value)(struct SEE_code *, const struct SEE_value *);
	void	(*gen_op)(struct SEE_code *, enum SEE_code_gen_op op);
	void	(*gen_roll)(struct SEE_code *, unsigned char);
	void	(*gen_call)(struct SEE_code *, enum SEE_code_call_op op, 
				int argc);
	void	(*gen_loc)(struct SEE_code *, struct SEE_throw_location *);
	void	(*gen_branch)(struct SEE_code *, enum SEE_code_branch_op op,
				SEE_code_addr_t *);
	void	(*patch)(struct SEE_code *, SEE_code_addr_t *);
};

/* Public fields of the code context superclass */
struct SEE_code {
	struct SEE_code_class *code_class;
	struct SEE_interpreter *interpreter;
};


/* 
 * Notes on optimizations:
 *   DUP,ROLL(2),PUTVALUE,POP -> PUTVALUE
 */

#endif /* _SEE_h_code_ */
