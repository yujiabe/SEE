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
# include <math.h>
#endif

#include <see/mem.h>
#include <see/value.h>
#include <see/string.h>
#include <see/object.h>
#include <see/native.h>
#include <see/cfunction.h>
#include <see/error.h>
#include <see/interpreter.h>

#include "regex.h"
#include "stringdefs.h"
#include "init.h"

/*
 * 15.10 The RegExp object.
 */

/* structure of regexp instances */
struct regexp_object {
	struct SEE_native native;
	struct SEE_string *source;
	unsigned char flags;
	struct regex *regex;
};

static struct regexp_object *toregexp(struct SEE_interpreter *,
	struct SEE_object *);

static void regexp_construct(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *, int,
	struct SEE_value **, struct SEE_value *);
static void regexp_call(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *, int,
	struct SEE_value **, struct SEE_value *);

static void regexp_proto_test(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);
static void regexp_proto_exec(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);
static void regexp_proto_toString(struct SEE_interpreter *,
	struct SEE_object *, struct SEE_object *,
	int, struct SEE_value **, struct SEE_value *);

/* object class for RegExp constructor */
static struct SEE_objectclass regexp_const_class = {
	STR(RegExpConstructor),		/* Class */
	SEE_native_get,			/* Get */
	SEE_native_put,			/* Put */
	SEE_native_canput,		/* CanPut */
	SEE_native_hasproperty,		/* HasProperty */
	SEE_native_delete,		/* Delete */
	SEE_native_defaultvalue,	/* DefaultValue */
	SEE_native_enumerator,		/* DefaultValue */
	regexp_construct,		/* Construct */
	regexp_call			/* Call */
};

/* object class for RegExp.prototype */
static struct SEE_objectclass regexp_proto_class = {
	STR(Object),			/* Class 15.10.6 */
	SEE_native_get,			/* Get */
	SEE_native_put,			/* Put */
	SEE_native_canput,		/* CanPut */
	SEE_native_hasproperty,		/* HasProperty */
	SEE_native_delete,		/* Delete */
	SEE_native_defaultvalue,	/* DefaultValue */
	SEE_native_enumerator		/* enumerator */
};

/* object class for regexp instances */
static struct SEE_objectclass regexp_inst_class = {
	STR(RegExp),			/* Class */
	SEE_native_get,			/* Get */
	SEE_native_put,			/* Put */
	SEE_native_canput,		/* CanPut */
	SEE_native_hasproperty,		/* HasProperty */
	SEE_native_delete,		/* Delete */
	SEE_native_defaultvalue,	/* DefaultValue */
	SEE_native_enumerator		/* enumerator */
};

void
SEE_RegExp_alloc(interp)
	struct SEE_interpreter *interp;
{
	interp->RegExp = 
	    (struct SEE_object *)SEE_NEW(interp, struct SEE_native);
	interp->RegExp_prototype = 
	    (struct SEE_object *)SEE_NEW(interp, struct SEE_native);
}

void
SEE_RegExp_init(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_object *RegExp;		/* struct SEE_native */
	struct SEE_object *RegExp_prototype;	/* struct SEE_native */
	struct SEE_value v;

	RegExp = interp->RegExp;
	RegExp_prototype = interp->RegExp_prototype;

	SEE_native_init((struct SEE_native *)RegExp, interp,
		&regexp_const_class, interp->Function_prototype);

	SEE_SET_NUMBER(&v, 2);
	SEE_OBJECT_PUT(interp, RegExp, STR(length), &v, SEE_ATTR_LENGTH); /* 15.10.5 */

	/* 15.10.6 Regexp.prototype */
	SEE_native_init((struct SEE_native *)RegExp_prototype, interp,
		&regexp_proto_class, interp->Object_prototype); 

	SEE_SET_OBJECT(&v, RegExp_prototype);			/* 15.10.5.1 */
	SEE_OBJECT_PUT(interp, RegExp, STR(prototype), &v,
		SEE_ATTR_DONTENUM | SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY);

	/* 15.10.6.1 RegExp.prototype.constructor */
	SEE_SET_OBJECT(&v, RegExp);
	SEE_OBJECT_PUT(interp, RegExp_prototype, STR(constructor), &v, 
		SEE_ATTR_DEFAULT);

#define PUTFUNC(name, len) \
	SEE_SET_OBJECT(&v, SEE_cfunction_make(interp, 		\
		regexp_proto_##name, STR(name), len));		\
	SEE_OBJECT_PUT(interp, RegExp_prototype, STR(name), &v,	\
		SEE_ATTR_DEFAULT);

	PUTFUNC(exec, 1)			/* 15.10.6.2 */
	PUTFUNC(test, 1)			/* 15.10.6.3 */
	PUTFUNC(toString, 0)			/* 15.10.6.4 */
}

static struct regexp_object *
toregexp(interp, o)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
{
	if (o->objectclass != &regexp_inst_class)
		SEE_error_throw_string(interp, interp->TypeError, 
		   STR(not_regexp));
	return (struct regexp_object *)o;
}

/* 15.10.4.1 */
static void
regexp_construct(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv;
	struct SEE_value *res;
{
	struct regexp_object *ro;
	struct SEE_value v;
	int i;

	ro = SEE_NEW(interp, struct regexp_object);
	SEE_native_init(&ro->native, interp, &regexp_inst_class,
		interp->RegExp_prototype);

	if (argc > 0 && 
	    argv[0]->type == SEE_OBJECT &&
	    argv[0]->u.object->objectclass == &regexp_inst_class)
	{
	    struct regexp_object *rs = 
	        (struct regexp_object *)argv[0]->u.object;
	    if (!(argc < 2 || argv[1]->type == SEE_UNDEFINED))
		SEE_error_throw_string(interp, interp->TypeError, 
		   STR(regexp_bad_string));
	    ro->source = rs->source;
	    ro->flags = rs->flags;
	} else {
	    if (argc < 1 || argv[0]->type == SEE_UNDEFINED)
		ro->source = STR(empty_string);
	    else {
		SEE_ToString(interp, argv[0], &v);
		ro->source = v.u.string;
	    }
	    ro->flags = 0;
	    if (argc > 1) {
		SEE_ToString(interp, argv[1], &v);
		for (i = 0; i < v.u.string->length; i++)
		    switch (v.u.string->data[i]) {
		    case 'g':  ro->flags |= FLAG_GLOBAL; break;
		    case 'i':  ro->flags |= FLAG_IGNORECASE; break;
		    case 'm':  ro->flags |= FLAG_MULTILINE; break;
		    default: break; /* ignore unknown flags */
		    }
	    }
	}

	ro->regex = SEE_regex_parse(interp, ro->source, ro->flags);

	/* 15.10.7.1 */
	SEE_SET_STRING(&v, ro->source);
	SEE_OBJECT_PUT(interp, (struct SEE_object *)ro, STR(source), &v,
		SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY | SEE_ATTR_DONTENUM);

	/* 15.10.7.2 */
	SEE_SET_BOOLEAN(&v, ro->flags & FLAG_GLOBAL);
	SEE_OBJECT_PUT(interp, (struct SEE_object *)ro, STR(global), &v,
		SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY | SEE_ATTR_DONTENUM);

	/* 15.10.7.3 */
	SEE_SET_BOOLEAN(&v, ro->flags & FLAG_IGNORECASE);
	SEE_OBJECT_PUT(interp, (struct SEE_object *)ro, STR(ignoreCase), &v,
		SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY | SEE_ATTR_DONTENUM);

	/* 15.10.7.4 */
	SEE_SET_BOOLEAN(&v, ro->flags & FLAG_MULTILINE);
	SEE_OBJECT_PUT(interp, (struct SEE_object *)ro, STR(multiline), &v,
		SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY | SEE_ATTR_DONTENUM);

	/* 15.10.7.5 */
	SEE_SET_NUMBER(&v, 0); /* 15.10.4.1 lastIndex = 0 */
	SEE_OBJECT_PUT(interp, (struct SEE_object *)ro, STR(lastIndex), &v,
		SEE_ATTR_DONTDELETE | SEE_ATTR_DONTENUM);

	SEE_SET_OBJECT(res, (struct SEE_object *)ro);
}

/* 15.10.3.1 */
static void
regexp_call(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv;
	struct SEE_value *res;
{
	if (argc > 1 && 
	    argv[0]->type == SEE_OBJECT &&
	    argv[0]->u.object->objectclass == &regexp_inst_class &&
	    (argc < 2 || argv[1]->type == SEE_UNDEFINED))
		SEE_SET_OBJECT(res, argv[0]->u.object);
	else
		SEE_OBJECT_CONSTRUCT(interp, self, thisobj, argc, argv, res);
}

/* 15.10.6.2 RegExp.prototype.exec() */
static void
regexp_proto_exec(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct regexp_object *ro;
	struct SEE_string *S;
	struct SEE_value v, iv, **elv, *el;
	unsigned int i, ncaptures;
	struct capture *captures;
	struct SEE_object *a;

	ro = toregexp(interp, thisobj);

	if (argc < 1)
	    SEE_error_throw_string(interp, interp->RangeError, STR(bad_argc));

	SEE_ToString(interp, argv[0], &v);
	S = v.u.string;

	SEE_OBJECT_GET(interp, thisobj, STR(lastIndex), &v);
	SEE_ToNumber(interp, &v, &iv);
	if (!(ro->flags & FLAG_GLOBAL))
		SEE_SET_NUMBER(&iv, 0);
	if (SEE_NUMBER_ISINF(&iv) || iv.u.number < 0 || iv.u.number > S->length) {
		SEE_SET_NUMBER(&v, 0);
		SEE_OBJECT_PUT(interp, thisobj, STR(lastIndex), &v, 0); 
		SEE_SET_NULL(res);
		return;
	}
	i = iv.u.number;	/* XXX what happens if this exceeds maxint? */

	ncaptures = SEE_regex_count_captures(ro->regex);
	SEE_ASSERT(interp, ncaptures > 0);
	captures = SEE_ALLOCA(ncaptures, struct capture);
	while (!SEE_regex_match(interp, ro->regex, S, i, captures)) {
		i++;
		if (i > S->length) {
		    SEE_SET_NUMBER(&v, 0);
		    SEE_OBJECT_PUT(interp, thisobj, STR(lastIndex), &v, 0); 
		    SEE_SET_NULL(res);
		    return;
		}
	}

	if ((ro->flags & FLAG_GLOBAL)) {
		SEE_SET_NUMBER(&v, captures[0].end);
		SEE_OBJECT_PUT(interp, thisobj, STR(lastIndex), &v, 0);
	}

	el = SEE_ALLOCA(ncaptures, struct SEE_value);
	elv = SEE_ALLOCA(ncaptures, struct SEE_value *);
	for (i = 0; i < ncaptures; i++) {
	    if (CAPTURE_IS_UNDEFINED(captures[i]))
		SEE_SET_UNDEFINED(&el[i]);
	    else 
		SEE_SET_STRING(&el[i], SEE_string_substr(interp, S, 
		    captures[i].start, captures[i].end - captures[i].start));
	    elv[i] = &el[i];
	}

	SEE_OBJECT_CONSTRUCT(interp, interp->Array, interp->Array,
		ncaptures, elv, &v);
	a = v.u.object;

	SEE_SET_NUMBER(&v, captures[0].start);
	SEE_OBJECT_PUT(interp, a, STR(index), &v, 0);

	SEE_SET_STRING(&v, S);
	SEE_OBJECT_PUT(interp, a, STR(input), &v, 0);

	SEE_SET_OBJECT(res, a);
}

int
SEE_is_RegExp(o)
	struct SEE_object *o;
{
	return o->objectclass == &regexp_inst_class;
}

int
SEE_RegExp_count_captures(interp, obj)
	struct SEE_interpreter *interp;
	struct SEE_object *obj;
{
	struct regexp_object *ro;

	ro = toregexp(interp, obj);
	return SEE_regex_count_captures(ro->regex);
}

int
SEE_RegExp_match(interp, obj, text, start, captures)
	struct SEE_interpreter *interp;
	struct SEE_object *obj;
	struct SEE_string *text;
	unsigned int start;
	struct capture *captures;
{
	struct regexp_object *ro;

	ro = toregexp(interp, obj);
	return SEE_regex_match(interp, ro->regex, text, start, captures);
}

/* 15.10.6.3 RegExp.prototype.test() */
static void
regexp_proto_test(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v;

	/*
	 * spec bug: the spec says this method is supposed to be
	 *
	 * "equivalent to the expression
	 *    RegExp.prototype.exec(string) != null."
	 *
	 * However, that expression would always give
	 * a type error exception, since RegExp.prototype
	 * does not have [[Class]] = "RegExp". The
	 * interpretation here is that we use the 'exec'
	 * method of the original RegExp.prototype object
	 * and then check for a null result in a fast
	 * way (see steps 1 and 3 of s11.9.3).
	 */
	regexp_proto_exec(interp, self, thisobj, argc, argv, &v);
	SEE_SET_BOOLEAN(res, v.type == SEE_NULL);
}

/* 15.10.6.4 RegExp.prototype.toString() */
static void
regexp_proto_toString(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct regexp_object *ro;
	struct SEE_string *s;
	int i;

	/*
	 * XXX (spec bug?) 15.10.6 says RegExp.prototype's [[Class]] is Object, and that
	 * methods where thisobj's [[Class]] is not RegExp, it has to throw a TypeError.
	 * Sadly, mozilla's test cases often want to print the RegExp.prototype.
	 * This special case is handled here. (Technically, this is a spec violation!)
	 */
	if (thisobj == interp->RegExp_prototype) {
		s = SEE_string_new(interp, 0);
		SEE_string_append(s, STR(RegExp));
		SEE_string_addch(s, '.');
		SEE_string_append(s, STR(prototype));
		SEE_SET_STRING(res, s);
		return;
	}

	ro = toregexp(interp, thisobj);
	s = SEE_string_new(interp, 0);
	SEE_string_addch(s, '/');
	for (i = 0; i < ro->source->length; i++) {
	    SEE_char_t c = ro->source->data[i];
	    if (c == '/')
		SEE_string_addch(s, '\\');    /* escape all forward slashes */
	    if (c == '\\') {
		SEE_string_addch(s, '\\');    /* leave escaped chars alone */
		if (++i < ro->source->length)
		    c = ro->source->data[i];
		else
		    break;		/* regex ends with an escape? */
	    }
	    SEE_string_addch(s, c);
	}
	SEE_string_addch(s, '/');
	if (ro->flags & FLAG_GLOBAL)
		SEE_string_addch(s, 'g');
	if (ro->flags & FLAG_IGNORECASE)
		SEE_string_addch(s, 'i');
	if (ro->flags & FLAG_MULTILINE)
		SEE_string_addch(s, 'm');
	SEE_SET_STRING(res, s);
}
