/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_interpreter_
#define _SEE_h_interpreter_

#include "config.h"
#include "type.h"

struct SEE_object;
struct SEE_try_context;
struct SEE_throw_location;
struct SEE_scope;

/*
 * This is the place to stick things that, once created
 * for an interpreter instance, should be kept around
 * for easy access. (And cannot be replaced by scripts)
 */

struct SEE_interpreter {
	void *	host_data;		/* reserved for host app's use */
	int	compatibility;		/* compatibility flags (read-only?) */

	/* Built-in objects */
	struct SEE_object *Global;
	struct SEE_object *Object;
	struct SEE_object *Object_prototype;
	struct SEE_object *Error;
	struct SEE_object *EvalError;
	struct SEE_object *RangeError;
	struct SEE_object *ReferenceError;
	struct SEE_object *SyntaxError;
	struct SEE_object *TypeError;
	struct SEE_object *URIError;
	struct SEE_object *String;
	struct SEE_object *String_prototype;
	struct SEE_object *Function;
	struct SEE_object *Function_prototype;
	struct SEE_object *Array;
	struct SEE_object *Array_prototype;
	struct SEE_object *Number;
	struct SEE_object *Number_prototype;
	struct SEE_object *Boolean;
	struct SEE_object *Boolean_prototype;
	struct SEE_object *Math;
	struct SEE_object *RegExp;
	struct SEE_object *RegExp_prototype;
	struct SEE_object *Date;
	struct SEE_object *Date_prototype;
	struct SEE_object *Global_eval;
	struct SEE_scope  *Global_scope;

	/* Current 'try-catch' context */
	volatile struct SEE_try_context * try_context;
	struct SEE_throw_location * try_location;

	/* Call traceback */
	struct SEE_traceback {
		struct SEE_throw_location *call_location;
		struct SEE_object *callee;
		int call_type;
		struct SEE_traceback *prev;
	} *traceback;

	void *intern_tab;		/* interned string table */
	unsigned int random_seed;	/* used by Math.random() */
	const char *locale;		/* current locale (may be NULL) */
	int recursion_limit;		/* -1 means don't care */

	void (*trace)(struct SEE_interpreter *, struct SEE_throw_location *);
};

/* Compatibility flags */
#define SEE_COMPAT_262_3B	0x0001	/* ECMA-262 3rd ed. Annex B */
#define SEE_COMPAT_EXT1		0x0002	/* see1.0 (non-ecma) extensions */
#define SEE_COMPAT_UNDEFDEF	0x0004	/* No ReferenceError on undefined var */
#define SEE_COMPAT_UTF_UNSAFE	0x0008	/* accept 'valid but insecure' UTF */
#define SEE_COMPAT_SGMLCOM	0x0010	/* treat '<!--' as a '//' comment */

/* traceback call_type */
#define SEE_CALLTYPE_CALL	1
#define SEE_CALLTYPE_CONSTRUCT	2

void SEE_interpreter_init(struct SEE_interpreter *i);
void SEE_interpreter_init_compat(struct SEE_interpreter *i, int compat_flags);

/*
 * This function is called when the interpreter encounters a
 * fatal condition. It should not return. It defaults to calling abort().
 */
extern void (*SEE_abort)(struct SEE_interpreter *i, const char *msg) SEE_dead;

#endif _SEE_h_interpreter_
