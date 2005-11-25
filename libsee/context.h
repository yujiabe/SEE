/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_context_
#define _SEE_h_context_

struct SEE_string;
struct SEE_object;
struct SEE_value;
struct SEE_interpreter;

/*
 * Execution context
 * -- 10
 */
struct SEE_context {
	struct SEE_interpreter *interpreter;
	struct SEE_object *activation;
	struct SEE_object *variable;
	int varattr;
	struct SEE_object *thisobj;
	struct SEE_scope {
		struct SEE_scope *next;
		struct SEE_object *obj;
	} *scope;
};

void SEE_context_lookup(struct SEE_context *context,
	struct SEE_string *name, struct SEE_value *res);
int SEE_context_scope_eq(struct SEE_scope *scope1, struct SEE_scope *scope2);

#endif /* _SEE_h_context_ */
