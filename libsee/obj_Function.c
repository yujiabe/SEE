/*
 * Copyright (c) 2003
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by David Leonard and 
 *      contributors.
 * 4. Neither the name of Mr Leonard nor the names of the contributors
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
/* $Id$ */

#if HAVE_CONFIG_H
# include <see/config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
#endif

#include <see/mem.h>
#include <see/value.h>
#include <see/native.h>
#include <see/no.h>
#include <see/cfunction.h>
#include <see/string.h>
#include <see/input.h>
#include <see/error.h>
#include <see/interpreter.h>
#include <see/try.h>

#include "array.h"
#include "function.h"
#include "context.h"
#include "parse.h"
#include "stringdefs.h"
#include "init.h"

/*
 * Function objects.
 *
 * This module declares four object classess:
 *   - the 'Function constructor' (a.k.a Function)
 *   - the 'Function prototype' (a.k.a. Function.prototype)
 *   - the 'function instance' (what FunctionDeclarations generate)
 *   - the 'arguments object' (translates indicies into local vars)
 */

struct function_inst *tofunction(struct SEE_interpreter *, struct SEE_object *);
static void function_inst_init(struct function_inst *,
	struct SEE_interpreter *, struct function *, struct SEE_scope *);

static void function_construct(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);
static void function_inst_construct(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);
static void function_inst_call(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);
static int function_inst_hasinstance(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_value *);

static void function_inst_get(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *, 
	struct SEE_value *);
static void function_inst_put(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *, 
	struct SEE_value *, int);
static int  function_inst_canput(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *);
static int  function_inst_hasproperty(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *);
static int  function_inst_delete(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *);
static struct SEE_enum *function_inst_enumerator(struct SEE_interpreter *,
	struct SEE_object *);

static void function_proto_toString(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);
static void function_proto_apply(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);
static void function_proto_call(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);

static void arguments_get(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *, 
	struct SEE_value *);
static void arguments_put(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *, 
	struct SEE_value *, int);
static int  arguments_canput(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *);
static int  arguments_hasproperty(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *);
static int  arguments_delete(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_string *);
void arguments_defaultvalue(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_value *, struct SEE_value *);

static struct arguments *arguments_create(struct SEE_interpreter*, 
	struct function *, struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **);

/* object class for Function constructor */
static struct SEE_objectclass function_const_class = {
	STR(FunctionConstructor),		/* Class */
	SEE_native_get,				/* Get */
	SEE_native_put,				/* Put */
	SEE_native_canput,			/* CanPut */
	SEE_native_hasproperty,			/* HasProperty */
	SEE_native_delete,			/* Delete */
	SEE_native_defaultvalue,		/* DefaultValue */
	SEE_native_enumerator,			/* Enumerator */
	function_construct,			/* Construct */
	function_construct			/* Call */
};

/* object class for function instances */
static struct SEE_objectclass function_inst_class = {
	STR(Function),				/* Class */
	function_inst_get,			/* Get */
	function_inst_put,			/* Put */
	function_inst_canput,			/* CanPut */
	function_inst_hasproperty,		/* HasProperty */
	function_inst_delete,			/* Delete */
	SEE_native_defaultvalue,		/* DefaultValue */
	function_inst_enumerator,		/* Enumerator */
	function_inst_construct,		/* Construct */
	function_inst_call,			/* Call */
	function_inst_hasinstance		/* HasInstance */
};

/* object class for 'arguments' instances */
static struct SEE_objectclass arguments_class = {
	STR(Arguments),				/* Class */
	arguments_get,				/* Get */
	arguments_put,				/* Put */
	arguments_canput,			/* CanPut */
	arguments_hasproperty,			/* HasProperty */
	arguments_delete,			/* Delete */
	arguments_defaultvalue,			/* DefaultValue */
	SEE_native_enumerator			/* Enumerator */
};

/* object class for objects created by instance constructs (13.2.2) */
static struct SEE_objectclass inst_inst_class = {
	STR(Object),				/* Class 13.2.2(2) */
	SEE_native_get,				/* Get */
	SEE_native_put,				/* Put */
	SEE_native_canput,			/* CanPut */
	SEE_native_hasproperty,			/* HasProperty */
	SEE_native_delete,			/* Delete */
	SEE_native_defaultvalue,		/* DefaultValue */
	SEE_native_enumerator			/* Enumerator */
};

/* structure of function instances */
struct function_inst {
	struct SEE_object object;
	struct function  *function;
	struct SEE_scope *scope;
};

/* structure of the 'arguments' objects */
struct arguments {
	struct SEE_native  native;
	struct function   *function;
	struct SEE_object *activation;
	int argc;
};

void
SEE_Function_alloc(interp)
	struct SEE_interpreter *interp;
{
	interp->Function =
		(struct SEE_object *)SEE_NEW(interp, struct SEE_native);
	interp->Function_prototype =
		(struct SEE_object *)SEE_NEW(interp, struct function_inst);
}

void
SEE_Function_init(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_object *Function;
	struct SEE_object *Function_prototype;
	struct SEE_value v;
	struct function *f;

	Function = interp->Function;
	Function_prototype = interp->Function_prototype;

	/* Function.prototype is itself a function instance! (15.3.4) */
	f = SEE_parse_function(interp, NULL, NULL, NULL);
	function_inst_init((struct function_inst *)Function_prototype,
		interp, f, interp->Global_scope);
	Function_prototype->Prototype = interp->Object_prototype; /* 15.3.4 */
	f->common->object.Prototype = interp->Object_prototype;	  /* 15.3.4 */

	if (interp->compatibility & SEE_COMPAT_EXT1) {
		/*
		 * Delete the "prototype" property of Function.prototype.
		 */
		SEE_SET_UNDEFINED(&v);
		SEE_OBJECT_PUT(interp, (struct SEE_object *)f->common, 
			STR(prototype), &v, SEE_ATTR_READONLY);
		SEE_OBJECT_DELETE(interp, (struct SEE_object *)f->common, 
			STR(prototype));
	}

#define PUTFUNC(name, len)\
	SEE_SET_OBJECT(&v, SEE_cfunction_make(interp, 			\
		function_proto_##name, STR(name), len));		\
	SEE_OBJECT_PUT(interp, Function_prototype, STR(name), 		\
		&v, SEE_ATTR_DEFAULT);

	PUTFUNC(toString, 1)					/* 15.3.4.2 */
	PUTFUNC(apply, 1)					/* 15.3.4.3 */
	PUTFUNC(call, 1)					/* 15.3.4.4 */

	/* Function.prototype.constructor = Function */
	SEE_SET_OBJECT(&v, Function);
	SEE_OBJECT_PUT(interp, Function_prototype, STR(constructor), &v,
		SEE_ATTR_DEFAULT);
								/* 15.3.4.1 */

	Function->objectclass = &function_const_class;
	Function->Prototype = Function_prototype;		/* 15.3.3 */

	SEE_SET_NUMBER(&v, 1);
	SEE_OBJECT_PUT(interp, Function, STR(length), &v, 
		SEE_ATTR_LENGTH);				/* 15.3.3 */

	SEE_SET_OBJECT(&v, Function_prototype);
	SEE_OBJECT_PUT(interp, Function, STR(prototype), &v, 	/* 15.3.3.1 */
		SEE_ATTR_DONTENUM | SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY);
};


/* Convert an object to a function instance, or raise a TypeError */
struct function_inst *
tofunction(interp, o)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
{
	if (!o || o->objectclass != &function_inst_class)
		SEE_error_throw_string(interp, interp->TypeError, 
		   STR(not_function));
	SEE_ASSERT(interp, ((struct function_inst *)o)->function != NULL);
	return (struct function_inst *)o;
}

static void
function_inst_init(fi, interp, f, scope)
	struct function_inst *fi;
	struct SEE_interpreter *interp;
	struct function *f;
	struct SEE_scope *scope;
{
	fi->object.objectclass = &function_inst_class;
	fi->object.Prototype = interp->Function_prototype;
	fi->function = f;
	fi->scope = scope;
}

/* 13.2 create (or pull from cache) a function instance with given scope */
struct SEE_object *
SEE_function_inst_create(interp, f, scope)
	struct SEE_interpreter *interp;
	struct function *f;
	struct SEE_scope *scope;
{
	struct function_inst *fi;

	/*
	 * We cached the first created Function instance of a 
	 * function strcuture, and return it if the scopes are
	 * observationally equivalent.
	 */
	if (f->cache) {
		/* Does the cached object have the same scope? */
		fi = (struct function_inst *)f->cache;
		if (SEE_context_scope_eq(fi->scope, scope))
			return f->cache;
	}

	fi = SEE_NEW(interp, struct function_inst);
	function_inst_init(fi, interp, f, scope);

	if (!f->cache)
		f->cache = (struct SEE_object *)fi;

	return (struct SEE_object *)fi;
}

/* 
 * Helper function for the SEE_OBJECT_JOINED() macro.
 * Returns true iff
 *   - the two objects are both function instances; AND
 *   - they are both 'joined' together (share the same body+common).
 *
 * See 13.1.2, 11.9.3 (step 13) and 11.9.6 (step 13)
 */
int
SEE_function_is_joined(a, b)
	struct SEE_object *a, *b;
{
	struct function_inst *fa = (struct function_inst *)a;
	struct function_inst *fb = (struct function_inst *)b;

	return a->objectclass == &function_inst_class &&
	       b->objectclass == &function_inst_class &&
	       fa->function == fb->function;
}

/* 15.3.2.1 new Function(...) */
static void
function_construct(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_string *P, *body;
	struct SEE_value r9, r13;
	struct SEE_input *paraminp, *bodyinp;
	int k;

	P = SEE_string_new(interp, 0);
	for (k = 0; k < argc - 1; k++) {
	    if (k)
		SEE_string_addch(P, ',');
	    SEE_ToString(interp, argv[k], &r9);
	    SEE_string_append(P, r9.u.string);
	}
	if (argc) {
	    SEE_ToString(interp, argv[argc - 1], &r13);
	    body = r13.u.string;
	} else
	    body = STR(empty_string);

	paraminp = SEE_input_string(interp, P);
	bodyinp = SEE_input_string(interp, body);
	SEE_SET_OBJECT(res, SEE_Function_new(interp, NULL, paraminp, bodyinp));
	SEE_INPUT_CLOSE(bodyinp);
	SEE_INPUT_CLOSE(paraminp);
}

struct SEE_object *
SEE_Function_new(interp, name, paraminp, bodyinp)
	struct SEE_interpreter *interp;
	struct SEE_string *name;
        struct SEE_input *paraminp, *bodyinp;
{
	struct function *f;

	f = SEE_parse_function(interp, name, paraminp, bodyinp);
	return SEE_function_inst_create(interp, f, interp->Global_scope);
}

/* 15.3.5.3 */
static int
function_inst_hasinstance(interp, f, vval)
	struct SEE_interpreter *interp;
	struct SEE_object *f;
	struct SEE_value *vval;
{
	struct SEE_object *v, *o;
	struct SEE_value oval;

	if (vval->type != SEE_OBJECT)
		return 0;
	v = vval->u.object;

	SEE_OBJECT_GET(interp, f, STR(prototype), &oval);
	if (oval.type != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError, 
			STR(internal_error)); /* XXX prototype not an object */
	o = oval.u.object;

	for (;;) {
		v = v->Prototype;
		if (!v)
			return 0;
		if (SEE_OBJECT_JOINED(v, o))
			return 1;
	}
}

/* 13.2.1 call a function */
static void
function_inst_call(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct context context;
	struct function_inst *fi;
	struct SEE_object *activation;
	struct arguments *arguments;
	struct SEE_value v;
	struct SEE_scope *innerscope;
	SEE_try_context_t ctxt;
	struct SEE_value old_arguments;
	int old_arguments_saved = 0;
	int old_arguments_attr = 0;

	fi = tofunction(interp, self);

	/* Bypass empty functions */
	if ((interp->compatibility & SEE_COMPAT_EXT1) &&
	    fi->function->is_empty)
	{
		SEE_SET_UNDEFINED(res);
		return;
	}

	/* 10.1.6 Create an activation object */
	activation = SEE_Object_new(interp);

	/* 10.1.6 Install an 'arguments' property */
	arguments = arguments_create(interp, fi->function, 
		activation, self, argc, argv);
	SEE_SET_OBJECT(&v, (struct SEE_object *)arguments);
	SEE_OBJECT_PUT(interp, activation, STR(arguments), &v, SEE_ATTR_DONTDELETE);

	/* 10.2.3 build the right scope chain now */
	innerscope = SEE_NEW(interp, struct SEE_scope);
	innerscope->obj = activation;
	innerscope->next = fi->scope;

	/* 10.2 enter a new execution context */
	context.interpreter = interp;
	context.activation = activation;
	context.variable = activation;		/* see 10.2.3 */
	context.varattr = SEE_ATTR_DONTDELETE;
	context.thisobj = thisobj ? thisobj : interp->Global;
	context.scope = innerscope;

	/* 10.1.3 Install the actual args into the activation object */
	SEE_function_put_args(&context, fi->function, argc, argv);


	/* 
	 * Compatibility: set f.arguments to the arguments object too,
	 * saving the old value (It gets restored later)
	 */
	if (interp->compatibility & SEE_COMPAT_EXT1) {
	    struct SEE_object *common = (struct SEE_object *)fi->function->common;
	    if (SEE_OBJECT_HASPROPERTY(interp, common, STR(arguments))) {
		SEE_OBJECT_GET(interp, common, STR(arguments), &old_arguments);
		old_arguments_attr = SEE_native_getownattr(interp, common,
			STR(arguments));
		old_arguments_saved = 1;
	    }
	    SEE_SET_OBJECT(&v, (struct SEE_object *)arguments);
	    SEE_OBJECT_PUT(interp, common, STR(arguments), &v, 
		SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY | SEE_ATTR_DONTENUM);
	}

	/* Run it (adds vars and func decls to context.variable) */
	SEE_TRY(interp, ctxt) {
		SEE_eval_functionbody(fi->function, &context, &v);
	}

	/* Restore f.arguments */
	if (interp->compatibility & SEE_COMPAT_EXT1) {
	    struct SEE_object *common = (struct SEE_object *)fi->function->common;
	    if (old_arguments_saved)
		SEE_OBJECT_PUT(interp, common, STR(arguments),
		    &old_arguments, old_arguments_attr);
	    else {
		/* XXX kludge to allow us to delete old arguments */
		SEE_SET_UNDEFINED(&v);
		SEE_OBJECT_PUT(interp, common, STR(arguments), &v, 
		    SEE_ATTR_READONLY);
		SEE_OBJECT_DELETE(interp, common, STR(arguments));
	    }
	}

	SEE_DEFAULT_CATCH(interp, ctxt);

	if (v.u.completion.type == SEE_NORMAL)
		SEE_SET_UNDEFINED(res);
	else if (v.u.completion.type == SEE_RETURN)
		SEE_VALUE_COPY(res, v.u.completion.value);
	else
		SEE_error_throw_string(interp, interp->Error, 
			STR(internal_error));
}

/* 13.2.2 */
static void
function_inst_construct(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_object *r1;
	struct SEE_value r3;

	r1 = SEE_native_new(interp);
	r1->objectclass = &inst_inst_class;

	SEE_OBJECT_GET(interp, self, STR(prototype), &r3);
	if (r3.type == SEE_OBJECT)
	    r1->Prototype = r3.u.object;
	else
	    r1->Prototype = interp->Object_prototype;
	SEE_OBJECT_CALL(interp, self, r1, argc, argv, res);
	if (res->type != SEE_OBJECT)
		SEE_SET_OBJECT(res, r1);
}

/* Function.prototype.toString (15.3.4.2) */
static void
function_proto_toString(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct function_inst *fi;
	struct function *f;
	struct SEE_string *s;
	int i;

	if (interp->compatibility & SEE_COMPAT_EXT1) {
	    /*
	     * spec bug: built-in-functions really should supply their own 
	     * toString() method for representation,  but the standard 
	     * makes Function.prototype.toString() have to handle it. 
	     * It is allowed to return an "implementation-dependent"
	     * string, BUT it has to have the syntax of a FunctionDeclaration!
	     * i.e. a built-in function must be expressible in the language!
	     * My solution is to return a void function that has a comment
	     * inside it, explaining.
	     */
	    extern struct SEE_objectclass SEE_cfunction_class;
	    if (thisobj && thisobj->objectclass == &SEE_cfunction_class) {
		    void SEE_cfunction_toString(struct SEE_interpreter *,
			    struct SEE_object *, struct SEE_object *,
			    int, struct SEE_value **, struct SEE_value *);
		    SEE_cfunction_toString(interp, self, thisobj, argc, argv,
			res);
		    return;
	    }

	    /*
	     * Built-in constructors have a similar representation problem.
	     * (This is pretty unsatisfactory.)
	     */
	    if (thisobj && thisobj->objectclass != &function_inst_class &&
		thisobj->objectclass->Construct != NULL)
	    {
		    s = SEE_string_sprintf(interp, 
			"function () { /* constructor %p */ }", 
			thisobj->objectclass->Construct);
		    SEE_SET_STRING(res, s);
		    return;
	    }
	}

	fi = tofunction(interp, thisobj);
	f = fi->function;

	s = SEE_string_new(interp, 0);
	SEE_string_append(s, STR(function));
	SEE_string_addch(s, ' ');
	if (f->name) 
		SEE_string_append(s, f->name);
	SEE_string_addch(s, '(');			
	for (i = 0; i < f->nparams; i++) {
		if (i) {
			SEE_string_addch(s, ',');
			SEE_string_addch(s, ' ');
		}
		SEE_string_append(s, f->params[i]);
	}
	/* XXX should get printing to work properly */
	SEE_string_addch(s, ')');
	SEE_string_addch(s, ' ');
	SEE_string_addch(s, '{');
	SEE_string_append(s, SEE_functionbody_string(interp, f));
	SEE_string_addch(s, '\n');
	SEE_string_addch(s, '}');
	SEE_string_addch(s, '\n');

	SEE_SET_STRING(res, s);
}

/* Function.prototype.apply (15.3.4.3) */
static void
function_proto_apply(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value **the_argv, *the_args, v;
	struct SEE_object *thisarg;
	struct SEE_string *s = NULL;
	int i, the_argc;

	if (!SEE_OBJECT_HAS_CALL(thisobj))
		SEE_error_throw_string(interp, interp->TypeError, 
			STR(not_callable));

	if (argc < 1 || argv[0]->type == SEE_UNDEFINED ||
			argv[0]->type == SEE_NULL)
		thisarg = interp->Global;
	else {
		SEE_ToObject(interp, argv[0], &v);
		thisarg = v.u.object;
	}

	if (argc < 2 || argv[1]->type == SEE_UNDEFINED ||
			argv[1]->type == SEE_NULL)
	{
	    the_argc = 0;
	    the_args = NULL;
	} else if (argv[1]->type == SEE_OBJECT && 
	    (argv[1]->u.object->objectclass == &arguments_class ||
	     SEE_is_Array(argv[1]->u.object)))
	{
	    struct SEE_object *a = argv[1]->u.object;

	    SEE_OBJECT_GET(interp, a, STR(length), &v);
	    the_argc = SEE_ToUint32(interp, &v);
	    the_args = SEE_ALLOCA(the_argc, struct SEE_value);

	    for (i = 0; i < the_argc; i++) {
		if (i == 0) s = SEE_string_new(interp, 0);
		s->length = 0;
		SEE_string_append_int(s, i);
		SEE_OBJECT_GET(interp, (struct SEE_object *)a, s, &the_args[i]);
	    }
	} else
		SEE_error_throw_string(interp, interp->TypeError, 
		   STR(apply_not_array));

	the_argv = SEE_ALLOCA(the_argc, struct SEE_value *);
	for (i = 0; i < the_argc; i++)
	    the_argv[i] = &the_args[i];
	SEE_OBJECT_CALL(interp, thisobj, thisarg, the_argc, the_argv, res);
}

/* Function.prototype.call (15.3.4.4) */
static void
function_proto_call(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value thisv;

	if (!SEE_OBJECT_HAS_CALL(thisobj))
		SEE_error_throw_string(interp, interp->TypeError, 
			STR(bad_arg));
	if (argc < 1 || argv[0]->type == SEE_NULL ||
	    argv[0]->type == SEE_UNDEFINED)
		SEE_SET_OBJECT(&thisv, interp->Global);
	else
		SEE_ToObject(interp, argv[0], &thisv);

	SEE_OBJECT_CALL(interp, thisobj, thisv.u.object, 
		argc == 0 ? 0 : argc - 1, argc == 0 ? NULL : argv + 1, res);
}

/*------------------------------------------------------------
 * The arguments object
 * 10.1.8
 */

/* 
 * Helper function that converts an integer name into the
 * formal parameter name. Returns NULL if the string s is not
 * a valid argument index.
 */
static struct SEE_string *
arg_rename(a, s)
	struct arguments *a;
	struct SEE_string *s;
{
	int value;
	int i;

	if (s->length == 0)
		return NULL;
	value = 0;
	for (i = 0; i < s->length; i++) {
		if (s->data[i] >= '0' && s->data[i] <= '9')
			value = 10 * value + s->data[i] - '0';
		else
			return NULL;
	}
	if (i < a->function->nparams)
		return a->function->params[i];
	return NULL;
}

static void
arguments_get(interp, o, p, res)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
	struct SEE_value *res;
{
	struct arguments *a = (struct arguments *)o;
	struct SEE_string *name = arg_rename(a, p);

	if (name)
		SEE_OBJECT_GET(interp, a->activation, name, res);
	else
		SEE_native_get(interp, o, p, res);
}

static void
arguments_put(interp, o, p, val, attr)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
	struct SEE_value *val;
	int attr;
{
	struct arguments *a = (struct arguments *)o;
	struct SEE_string *name = arg_rename(a, p);

	if (name)
		SEE_OBJECT_PUT(interp, a->activation, name, val, attr);
	else
		SEE_native_put(interp, o, p, val, attr);
}

static int
arguments_canput(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	struct arguments *a = (struct arguments *)o;
	struct SEE_string *name = arg_rename(a, p);

	if (name)
		return SEE_OBJECT_CANPUT(interp, a->activation, name);
	else
		return SEE_native_canput(interp, o, p);
}

static int
arguments_hasproperty(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	struct arguments *a = (struct arguments *)o;
	struct SEE_string *name = arg_rename(a, p);

	if (name)
		return SEE_OBJECT_HASPROPERTY(interp, a->activation, name);
	else
		return SEE_native_hasproperty(interp, o, p);
}

static int
arguments_delete(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	struct arguments *a = (struct arguments *)o;
	struct SEE_string *name = arg_rename(a, p);

	if (name)
		return SEE_OBJECT_DELETE(interp, a->activation, name);
	else
		return SEE_native_delete(interp, o, p);
}

void
arguments_defaultvalue(interp, o, hint, res)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_value *hint;
	struct SEE_value *res;
{
	struct arguments *a = (struct arguments *)o;
	struct SEE_string *s = SEE_string_new(interp, 0);
	struct SEE_string *snum = NULL;
	struct SEE_value v, vs;
	int i, argc;


	if (interp->compatibility & SEE_COMPAT_EXT1) {
	    argc = a->argc;
	    if (argc < a->function->nparams)
		argc = a->function->nparams;
	    SEE_string_addch(s, '[');
	    for (i = 0; i < argc; i++) {
		if (i) {
		    SEE_string_addch(s, ',');
		    SEE_string_addch(s, ' ');
		}
		if (i < a->function->nparams) {
		    SEE_string_append(s, a->function->params[i]);
		    SEE_string_addch(s, '=');
		    SEE_OBJECT_GET(interp, a->activation, 
			a->function->params[i], &v);
		} else {
		    if (!snum) snum = SEE_string_new(interp, 0);
		    snum->length = 0;
		    SEE_string_append_int(snum, i);
		    SEE_string_append(s, snum);
		    SEE_string_addch(s, '=');
		    SEE_native_get(interp, o, snum, &v);
		}
		SEE_ToString(interp, &v, &vs);
		SEE_string_append(s, vs.u.string);
	    }
	    SEE_string_addch(s, ']');
	    SEE_SET_STRING(res, s);
	} else
	    SEE_no_defaultvalue(interp, o, hint, res);
}

/* 10.1.8 Create an arguments structure */
static struct arguments *
arguments_create(interp, f, activation, callee, argc, argv)
	struct SEE_interpreter *interp;
	struct function *f;
	struct SEE_object *activation, *callee;
	int argc;
	struct SEE_value **argv;
{
	struct arguments *arguments;
	struct SEE_value v;
	struct SEE_string *s;
	int i;

	arguments = SEE_NEW(interp, struct arguments);
	SEE_native_init(&arguments->native, interp, &arguments_class,
		interp->Object_prototype);

	arguments->activation = activation;
	arguments->function = f;
	arguments->argc = argc;

	SEE_SET_OBJECT(&v, callee);
	SEE_OBJECT_PUT(interp, (struct SEE_object *)arguments, STR(callee), &v,
		SEE_ATTR_DONTENUM);

	SEE_SET_NUMBER(&v, argc);
	SEE_OBJECT_PUT(interp, (struct SEE_object *)arguments, STR(length), &v,
		SEE_ATTR_DONTENUM);

	/* Copy the extraneous arguments */
	if (argc > f->nparams) {
	    s = SEE_string_new(interp, 4);
	    for (i = f->nparams; i < argc; i++) {
		s->length = 0;
		SEE_string_append_int(s, i);
		SEE_OBJECT_PUT(interp, (struct SEE_object *)arguments, s, argv[i], 0);
	    }
	}
	return arguments;
}

/*------------------------------------------------------------
 * function instance methods
 *  - these simply pass the operations down to the 'common' object
 */

/*
 * NOTE: since these are always calls to SEE_native_*, we could
 *       change the macros to direct calls, and save one pointer lookup.
 *       However, it seems pretty rare to make changes to function objects
 *       and this interface separation improves modularity.
 */

static void
function_inst_get(interp, o, p, res)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
	struct SEE_value *res;
{
	struct SEE_object *common;

	common = (struct SEE_object *)tofunction(interp, o)->function->common;
	SEE_OBJECT_GET(interp, common, p, res);
}

static void
function_inst_put(interp, o, p, val, attr)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
	struct SEE_value *val;
	int attr;
{
	struct SEE_object *common;

	common = (struct SEE_object *)tofunction(interp, o)->function->common;
	SEE_OBJECT_PUT(interp, common, p, val, attr);
}

static int
function_inst_canput(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	struct SEE_object *common;

	common = (struct SEE_object *)tofunction(interp, o)->function->common;
	return SEE_OBJECT_CANPUT(interp, common, p);
}

static int
function_inst_hasproperty(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	struct SEE_object *common;

	common = (struct SEE_object *)tofunction(interp, o)->function->common;
	return SEE_OBJECT_HASPROPERTY(interp, common, p);
}

static int
function_inst_delete(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	struct SEE_object *common;

	common = (struct SEE_object *)tofunction(interp, o)->function->common;
	return SEE_OBJECT_DELETE(interp, common, p);
}

static struct SEE_enum *
function_inst_enumerator(interp, o)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
{
	struct SEE_object *common;

	common = (struct SEE_object *)tofunction(interp, o)->function->common;
	return SEE_OBJECT_ENUMERATOR(interp, common);
}

