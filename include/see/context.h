/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_context_
#define _SEE_h_context_

struct SEE_string;
struct SEE_object;
struct SEE_value;
struct SEE_scope;
struct SEE_interpreter;

/*
 * Execution context. 
 * (This structure is only of interest to debugger writers.)
 * -- 10
 */
struct SEE_context {
	struct SEE_interpreter *interpreter;
	struct SEE_object *activation;
	struct SEE_object *variable;
	int varattr;			/* default attrs for new vars */
	struct SEE_object *thisobj;
	struct SEE_scope *scope;
};

void SEE_eval(struct SEE_context *context, struct SEE_object *thisobj,
	int argc, struct SEE_value **argv, struct SEE_value *res);

#endif /* _SEE_h_context_ */
