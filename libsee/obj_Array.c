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
# include <config.h>
#endif

#include <see/mem.h>
#include <see/value.h>
#include <see/string.h>
#include <see/object.h>
#include <see/native.h>
#include <see/cfunction.h>
#include <see/error.h>
#include <see/interpreter.h>
#include <see/debug.h>

#include "stringdefs.h"
#include "array.h"
#include "parse.h"
#include "init.h"

#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) < (b) ? (b) : (a))

/*
 * The Array object.
 * 15.4 
 */

/* structure of array instances */
struct array_object {
	struct SEE_native native;
	SEE_uint32_t length;
};

static void intstr_p(struct SEE_string *, SEE_uint32_t);
static void intstr(struct SEE_interpreter *,struct SEE_string **, SEE_uint32_t);
static void array_init(struct array_object *, struct SEE_interpreter *,
	SEE_uint32_t);
static void array_construct(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static struct array_object *toarray(struct SEE_interpreter *, 
	struct SEE_object *);

static void array_proto_toString(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_toLocaleString(struct SEE_interpreter *, 
	struct SEE_object *, struct SEE_object *, int, struct SEE_value **, 
	struct SEE_value *);
static void array_proto_concat(struct SEE_interpreter *, struct SEE_object *, 
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_join(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_pop(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_push(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_reverse(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_shift(struct SEE_interpreter *, struct SEE_object *, 
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_slice(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_sort(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_splice(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void array_proto_unshift(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

static void array_get(struct SEE_interpreter *, struct SEE_object *, 
	struct SEE_string *, struct SEE_value *);
static void array_put(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_string *, struct SEE_value *, int);
static int  array_hasproperty(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_string *);
static int  array_delete(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_string *);
static struct SEE_enum *array_enumerator(struct SEE_interpreter *,
	struct SEE_object *);

/* object class for Array constructor */
static struct SEE_objectclass array_const_class = {
	STR(ArrayConstructor),		/* Class */
	SEE_native_get,			/* Get */
	SEE_native_put,			/* Put */
	SEE_native_canput,		/* CanPut */
	SEE_native_hasproperty,		/* HasProperty */
	SEE_native_delete,		/* Delete */
	SEE_native_defaultvalue,	/* DefaultValue */
	SEE_native_enumerator,		/* DefaultValue */
	array_construct,
	array_construct,
};

/* object class for array instances */
static struct SEE_objectclass array_inst_class = {
	STR(Array),			/* Class */
	array_get,			/* Get */
	array_put,			/* Put */
	SEE_native_canput,		/* CanPut */
	array_hasproperty,		/* HasProperty */
	array_delete,			/* Delete */
	SEE_native_defaultvalue,	/* DefaultValue */
	SEE_native_enumerator		/* enumerator */
};

void
SEE_Array_alloc(interp)
	struct SEE_interpreter *interp;
{
	interp->Array = 
		(struct SEE_object *)SEE_NEW(interp, struct SEE_native);
	interp->Array_prototype = 
		(struct SEE_object *)SEE_NEW(interp, struct array_object);
}

void
SEE_Array_init(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_object *Array;		/* struct SEE_native */
	struct SEE_object *Array_prototype;	/* struct array_object */
	struct SEE_value v;

	Array = interp->Array;
	Array_prototype = interp->Array_prototype;

	SEE_native_init((struct SEE_native *)Array, interp, &array_const_class,
		interp->Function_prototype);

	/* 15.4.3 Array.length = 1 */
	SEE_SET_NUMBER(&v, 1);
	SEE_OBJECT_PUT(interp, Array, STR(length), &v, SEE_ATTR_LENGTH);

	/* 15.4.3.1 Array.prototype */
	SEE_SET_OBJECT(&v, Array_prototype);
	SEE_OBJECT_PUT(interp, Array, STR(prototype), &v,
		SEE_ATTR_DONTENUM | SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY); 

	/* 15.4.4 Array.prototype.length = 0 */
	array_init((struct array_object *)Array_prototype, interp, 0);

	/* 14.4.4 Array.prototype.[[Prototype]] = Object.prototype */
	Array_prototype->Prototype = interp->Object_prototype;

	/* 15.4.4.1 Array.prototype.constructor = Array */
	SEE_SET_OBJECT(&v, Array);
	SEE_OBJECT_PUT(interp, Array_prototype, STR(constructor), &v, SEE_ATTR_DEFAULT);

#define PUTFUNC(name, len) 						  \
	SEE_SET_OBJECT(&v, SEE_cfunction_make(interp, array_proto_##name, \
		STR(name), len)); 					  \
	SEE_OBJECT_PUT(interp, Array_prototype, STR(name), &v, 		  \
		SEE_ATTR_DEFAULT);

	PUTFUNC(toString, 0)			/* 15.4.4.2 */
	PUTFUNC(toLocaleString, 0)		/* 15.4.4.3 */
	PUTFUNC(concat, 1)			/* 15.4.4.4 */
	PUTFUNC(join, 1)			/* 15.4.4.5 */
	PUTFUNC(pop, 0)				/* 15.4.4.6 */
	PUTFUNC(push, 1)			/* 15.4.4.7 */
	PUTFUNC(reverse, 0)			/* 15.4.4.8 */
	PUTFUNC(shift, 0)			/* 15.4.4.9 */
	PUTFUNC(slice, 2)			/* 15.4.4.10 */
	PUTFUNC(sort, 1)			/* 15.4.4.11 */
	PUTFUNC(splice, 2)			/* 15.4.4.12 */
	PUTFUNC(unshift, 1)			/* 15.4.4.13 */
}

#define MAX_ARRAY_INDEX	(4294967295UL)

/*
 * Helper function that returns true if the string
 * is an integer property less than 2^32-1 (4294967295), and stores
 * the integer value in the given pointer. Don't allow leading zeroes.
 */
int
SEE_to_array_index(s, ip)
	struct SEE_string *s;
	SEE_uint32_t *ip;
{
	SEE_uint32_t n = 0;
	int i, digit;

	if (s->length == 0)
		return 0;
	/* Don't allow leading zeros */
	if (s->data[0] == '0' && s->length > 1)
		return 0;
	for (i = 0; i < s->length; i++) {
	    if (s->data[i] < '0' || s->data[i] > '9')
		return 0;
	    digit = s->data[i] - '0';
	    if (n > (MAX_ARRAY_INDEX / 10) ||
	        (n == (MAX_ARRAY_INDEX / 10) && 
		 digit >= (MAX_ARRAY_INDEX % 10)))
		    return 0;
	    n = n * 10 + digit;
	}
	*ip = n;
	return 1;
}

/*
 * Helper functions for quickly building a string from an integer.
 * Only called from intstr().
 */
static void
intstr_p(s, i)
	struct SEE_string *s;
	SEE_uint32_t i;
{
	if (i >= 10)
		intstr_p(s, i / 10);
	SEE_string_addch(s, '0' + (i % 10));
}

/*
 * If sp is null, allocates a new empty string.
 * Clears the string *sp and puts unsigned integer i into it.
 */
static void
intstr(interp, sp, i)
	struct SEE_interpreter *interp;
	struct SEE_string **sp;
	SEE_uint32_t i;
{
	if (!*sp)
		*sp = SEE_string_new(interp, 0);
	else
		(*sp)->length = 0;
	intstr_p(*sp, i);
}

/*
 * Convert the object to a native array, or raise an error
 */
static struct array_object *
toarray(interp, o)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
{
	if (!SEE_is_Array(o))
	    SEE_error_throw_string(interp, interp->TypeError, 
		STR(not_array));
	return (struct array_object *)o;
}

int
SEE_is_Array(o)
	struct SEE_object *o;
{
	return o && o->objectclass == &array_inst_class;
}

/* Fast native array push() */
void
SEE_Array_push(interp, o, v)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_value *v;
{
	struct array_object *a;
	struct SEE_string *s = NULL;

	a = toarray(interp, o);
	intstr(interp, &s, a->length);
	SEE_native_put(interp, o, s, v, 0);
	a->length++;
}

SEE_uint32_t
SEE_Array_length(interp, o)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
{
	struct array_object *a = toarray(interp, o);
	return a->length;
}

/* helper function to build an array instance */
static void
array_init(ao, interp, length)
	struct array_object *ao;
	struct SEE_interpreter *interp;
	unsigned int length;
{
	SEE_native_init(&ao->native, interp, &array_inst_class, 
	    interp->Array_prototype);
	ao->length = length;
}

/* 15.4.4.2 */
static void
array_construct(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv;
	struct SEE_value *res;
{
	struct array_object *ao;
	int i;
	SEE_uint32_t length;
	struct SEE_string *s = NULL;

	if (argc == 1 && SEE_VALUE_GET_TYPE(argv[0]) == SEE_NUMBER) {
	    length = SEE_ToUint32(interp, argv[0]);
	    if (argv[0]->u.number != length)
		SEE_error_throw_string(interp, interp->RangeError, 
		   STR(array_badlen));
	    ao = SEE_NEW(interp, struct array_object);
	    array_init(ao, interp, length);
	} else {
	    ao = SEE_NEW(interp, struct array_object);
	    array_init(ao, interp, argc);
	    for (i = 0; i < argc; i++) {
		intstr(interp, &s, i);
		SEE_native_put(interp, (struct SEE_object *)&ao->native, 
			s, argv[i], 0);
	    }
	}
	SEE_SET_OBJECT(res, (struct SEE_object *)ao);
}

/* 15.4.4.2 */
static void
array_proto_toString(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	(void)toarray(interp, thisobj);
	array_proto_join(interp, self, thisobj, 0, NULL, res);
}

/* 15.4.4.3 */
static void
array_proto_toLocaleString(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v, r6, r7;
	struct SEE_string *separator, *s, *n = NULL;
	SEE_uint32_t length, i;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	length = SEE_ToUint32(interp, &v);

	separator = STR(comma);		/* TODO: should be locale-dependent */

	if (length == 0) {
	    SEE_SET_STRING(res, STR(empty_string));
	    return;
	}

	s = SEE_string_new(interp, 0);
	if (length) {
	    for (i = 0; i < length; i++) {
		if (i)
		    SEE_string_append(s, separator);
		intstr(interp, &n, i);
		SEE_OBJECT_GET(interp, thisobj, n, &r6);
		if (!(SEE_VALUE_GET_TYPE(&r6) == SEE_UNDEFINED || SEE_VALUE_GET_TYPE(&r6) == SEE_NULL)) {
		    SEE_ToObject(interp, &r6, &r7);
		    SEE_OBJECT_GET(interp, r7.u.object, STR(toLocaleString),&v);
		    if (SEE_VALUE_GET_TYPE(&v) != SEE_OBJECT ||
				!SEE_OBJECT_HAS_CALL(v.u.object))
			SEE_error_throw_string(interp, interp->TypeError,
			    STR(toLocaleString_notfunc));
		    SEE_OBJECT_CALL(interp, v.u.object, r7.u.object, 
			0, NULL, &v);
		    if (SEE_VALUE_GET_TYPE(&v) != SEE_STRING)
			SEE_error_throw_string(interp, interp->TypeError,
			    STR(toLocaleString_notstring));
		    SEE_string_append(s, v.u.string);
		}
	    }
	}
	SEE_SET_STRING(res, s);
}

/* 15.4.4.4 */
static void
array_proto_concat(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v, *E, thisv;
	struct SEE_object *A;
	SEE_uint32_t n, k;
	int i;
	struct SEE_string *ns = NULL;

	SEE_OBJECT_CONSTRUCT(interp, interp->Array, interp->Array, 0, NULL, &v);
	A = v.u.object;
	n = 0;
	SEE_SET_OBJECT(&thisv, thisobj);
	E = &thisv;
	i = 0;
	for(;;) {
	    if (SEE_VALUE_GET_TYPE(E) == SEE_OBJECT && SEE_is_Array(E->u.object)) {
		struct array_object *Ea = (struct array_object *)E->u.object;
		for (k = 0; k < Ea->length; k++) {
		    intstr(interp, &ns, k);
		    if (SEE_OBJECT_HASPROPERTY(interp, E->u.object, ns)) {
			SEE_OBJECT_GET(interp, E->u.object, ns, &v);
			intstr(interp, &ns, n);
			SEE_OBJECT_PUT(interp, A, ns, &v, 0);
		    }
		    n++;
		}
	    } else {
		intstr(interp, &ns, n);
		SEE_OBJECT_PUT(interp, A, ns, E, 0);
		n++;
	    }
	    if (i >= argc) break;
	    E = argv[i++];
	}
	SEE_SET_NUMBER(&v, n); SEE_OBJECT_PUT(interp, A, STR(length), &v, 0);
	SEE_SET_OBJECT(res, A);
}

/* 15.4.4.5 */
static void
array_proto_join(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v, r6, r7;
	struct SEE_string *separator, *s, *n = NULL;
	SEE_uint32_t length, i;
	int use_comma;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	length = SEE_ToUint32(interp, &v);

	if (interp->compatibility & SEE_COMPAT_ARRAYJOIN1) 
		use_comma = (argc == 0);
	else 
		/* strict E262-3 behaviour: */
		use_comma = (argc == 0 || SEE_VALUE_GET_TYPE(argv[0]) == SEE_UNDEFINED);

	if (use_comma)
		separator = STR(comma);
	else {
		SEE_ToString(interp, argv[0], &v);
		separator = v.u.string;
	}

	s = SEE_string_new(interp, 0);
	if (length) {
	    for (i = 0; i < length; i++) {
		if (i)
		    SEE_string_append(s, separator);
		intstr(interp, &n, i);
		SEE_OBJECT_GET(interp, thisobj, n, &r6);
		if (!(SEE_VALUE_GET_TYPE(&r6) == SEE_UNDEFINED || SEE_VALUE_GET_TYPE(&r6) == SEE_NULL)) {
		    SEE_ToString(interp, &r6, &r7);
		    SEE_string_append(s, r7.u.string);
		}
	    }
	}
	SEE_SET_STRING(res, s);
}

/* 15.4.4.6 */
static void
array_proto_pop(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v;
	SEE_uint32_t i;
	struct SEE_string *s = NULL;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	i = SEE_ToUint32(interp, &v);
	if (i == 0) {
		SEE_SET_NUMBER(&v, i);
		SEE_OBJECT_PUT(interp, thisobj, STR(length), &v, 0);
		SEE_SET_UNDEFINED(res);
		return;
	}
	intstr(interp, &s, i - 1);
	SEE_OBJECT_GET(interp, thisobj, s, res);
	SEE_OBJECT_DELETE(interp, thisobj, s);
	SEE_SET_NUMBER(&v, i - 1);
	SEE_OBJECT_PUT(interp, thisobj, STR(length), &v, 0);
}

/* 15.4.4.7 */
static void
array_proto_push(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	int i;
	SEE_uint32_t n;
	struct SEE_value v;
	struct SEE_string *np = NULL;

        SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
        n = SEE_ToUint32(interp, &v);
	for (i = 0; i < argc; i++) {
	    intstr(interp, &np, n);
	    SEE_OBJECT_PUT(interp, thisobj, np, argv[i], 0);
	    n++;
	}
	SEE_SET_NUMBER(res, n);
        SEE_OBJECT_PUT(interp, thisobj, STR(length), res, 0);
}

/* 15.4.4.8 */
static void
array_proto_reverse(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v, r9, r10;
	struct SEE_string *r7 = NULL, *r8 = NULL;
	SEE_uint32_t k, r2, r3, r6;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	r2 = SEE_ToUint32(interp, &v);
	r3 = r2 / 2;	/* NB implicit floor() from integer div */
	for (k = 0; k < r3; k++) {
	    r6 = r2 - k - 1;
	    intstr(interp, &r7, k);
	    intstr(interp, &r8, r6);
	    SEE_OBJECT_GET(interp, thisobj, r7, &r9);
	    SEE_OBJECT_GET(interp, thisobj, r8, &r10);
	    if (SEE_OBJECT_HASPROPERTY(interp, thisobj, r8)) {
		if (SEE_OBJECT_HASPROPERTY(interp, thisobj, r7)) {
		    SEE_OBJECT_PUT(interp, thisobj, r7, &r10, 0);
		    SEE_OBJECT_PUT(interp, thisobj, r8, &r9, 0);
		} else {
		    SEE_OBJECT_PUT(interp, thisobj, r7, &r10, 0);
		    SEE_OBJECT_DELETE(interp, thisobj, r8);
		}
	    } else if (SEE_OBJECT_HASPROPERTY(interp, thisobj, r7)) {
		SEE_OBJECT_DELETE(interp, thisobj, r7);
		SEE_OBJECT_PUT(interp, thisobj, r8, &r9, 0);
	    } else {
		SEE_OBJECT_DELETE(interp, thisobj, r7);
		SEE_OBJECT_DELETE(interp, thisobj, r8);
	    }
	}
	SEE_SET_OBJECT(res, thisobj);
}

/* 15.4.4.9 */
static void
array_proto_shift(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v;
	struct SEE_string *s = NULL;
	SEE_uint32_t k, r2;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	r2 = SEE_ToUint32(interp, &v);
	if (r2 == 0) {
	    SEE_SET_NUMBER(&v, r2);
	    SEE_OBJECT_PUT(interp, thisobj, STR(length), &v, 0);
	    SEE_SET_UNDEFINED(res);
	    return;
	}
	SEE_OBJECT_GET(interp, thisobj, STR(zero_digit), res);
	for (k = 1; k < r2; k++) {
	    intstr(interp, &s, k);
	    if (SEE_OBJECT_HASPROPERTY(interp, thisobj, s)) {
		SEE_OBJECT_GET(interp, thisobj, s, &v);
		intstr(interp, &s, k - 1);
		SEE_OBJECT_PUT(interp, thisobj, s, &v, 0);
	    } else {
		intstr(interp, &s, k - 1);
		SEE_OBJECT_DELETE(interp, thisobj, s);
	    }
	}
	intstr(interp, &s, r2 - 1);
	SEE_OBJECT_DELETE(interp, thisobj, s);
	SEE_SET_NUMBER(&v, r2 - 1);
	SEE_OBJECT_PUT(interp, thisobj, STR(length), &v, 0);
}

/* 15.4.4.10 */
static void
array_proto_slice(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_object *A;
	SEE_uint32_t r3, r5, r8, k, n;
	struct SEE_string *s = NULL;
	struct SEE_value v;

	if (argc < 1) {
		SEE_SET_UNDEFINED(res);
		return;
	}

	SEE_OBJECT_CONSTRUCT(interp, interp->Array, interp->Array, 0, NULL, &v);
	A = v.u.object;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	r3 = SEE_ToUint32(interp, &v);

	SEE_ToInteger(interp, argv[0], &v);
	r5 = (v.u.number < 0) ? MAX(r3 + v.u.number, 0)
			      : MIN(v.u.number, r3);
	if (argc < 2 || SEE_VALUE_GET_TYPE(argv[1]) == SEE_UNDEFINED)
		r8 = r3;
	else {
	    SEE_ToInteger(interp, argv[1], &v);
	    r8 = (v.u.number < 0) ? MAX(r3 + v.u.number, 0)
				  : MIN(v.u.number, r3);
	}
	for (k = r5, n = 0; k < r8; k++, n++) {
	    intstr(interp, &s, k);
	    if (SEE_OBJECT_HASPROPERTY(interp, thisobj, s)) {
		SEE_OBJECT_GET(interp, thisobj, s, &v);
		intstr(interp, &s, n);
		SEE_OBJECT_PUT(interp, A, s, &v, 0);
	    }
	}
	SEE_SET_NUMBER(&v, n);
	SEE_OBJECT_PUT(interp, A, STR(length), &v, 0);
	SEE_SET_OBJECT(res, A);
}

/*
 * A sort comparison function similar to that in 15.4.4.11,
 * except that our quicksort has already used get/put/hasproperty
 * on the array, and we represent this with pointers x and y.
 * If the pointers are NULL, then the elements don't exist in the
 * array, and should be sorted higher than anything else. If the
 * pointers point to undefined, then they should be sorted second-highest.
 */
static int
SortCompare(interp, x, y, cmpfn)
	struct SEE_interpreter *interp;
	struct SEE_value *x, *y;
	struct SEE_object *cmpfn;
{
	if (!x && !y)   return 0;
	if (!x)		return 1;
	if (!y)		return -1;
	if (SEE_VALUE_GET_TYPE(x) == SEE_UNDEFINED && SEE_VALUE_GET_TYPE(y) == SEE_UNDEFINED) return 0;
	if (SEE_VALUE_GET_TYPE(x) == SEE_UNDEFINED) return 1;
	if (SEE_VALUE_GET_TYPE(y) == SEE_UNDEFINED) return -1;
	if (cmpfn) {
		struct SEE_value vn, *arg[2];
		arg[0] = x;
		arg[1] = y;
		SEE_OBJECT_CALL(interp, cmpfn, cmpfn, 2, arg, &vn);
		if (SEE_VALUE_GET_TYPE(&vn) != SEE_NUMBER || SEE_NUMBER_ISNAN(&vn)) 
		    SEE_error_throw_string(interp, interp->TypeError,
			STR(array_sort_error));
		if (vn.u.number < 0) return -1;
		if (vn.u.number > 0) return 1;
		return 0;
	} else {
		struct SEE_value xs, ys;
		SEE_ToString(interp, x, &xs);
		SEE_ToString(interp, y, &ys);
		return SEE_string_cmp(xs.u.string, ys.u.string);
	}
}

/**
 * Quicksort partition: partitions the given segment into numbers
 * smaller, then larger than the first element, using swaps.
 * Returns the index of the pivot point.
 */
static int
qs_partition(interp, thisobj, lo, hi, cmpfn, s1, s2)
	struct SEE_interpreter *interp;
	struct SEE_object *thisobj;
	SEE_uint32_t lo, hi;
	struct SEE_object *cmpfn;
	struct SEE_string **s1, **s2;
{
	struct SEE_value xv, iv, jv;
	struct SEE_value *xvp = NULL, *ivp = NULL, *jvp = NULL;
	SEE_uint32_t i = lo - 1;
	SEE_uint32_t j = hi + 1;

	intstr(interp, s1, lo - 1);
	if (SEE_OBJECT_HASPROPERTY(interp, thisobj, *s1)) {
		SEE_OBJECT_GET(interp, thisobj, *s1, &xv);
		xvp = &xv;
	} else
		xvp = NULL;

	for (;;) {
	    do {
		if (j == lo) break;
		j--;
		intstr(interp, s2, j - 1);
		if (SEE_OBJECT_HASPROPERTY(interp, thisobj, *s2)) {
			SEE_OBJECT_GET(interp, thisobj, *s2, &jv);
			jvp = &jv;
		} else
			jvp = NULL;
	    } while (SortCompare(interp, xvp, jvp, cmpfn) < 0);
	    do {
		if (i == hi) break;
		i++;
		intstr(interp, s1, i - 1);
		if (SEE_OBJECT_HASPROPERTY(interp, thisobj, *s1)) {
			SEE_OBJECT_GET(interp, thisobj, *s1, &iv);
			ivp = &iv;
		} else
			ivp = NULL;
	    } while (SortCompare(interp, ivp, xvp, cmpfn) < 0);
	    if (i < j) {
		/*
		 * At this stage, *s1 will always be "i" and *s2 will
		 * always be "j".
		 */

		if (ivp) SEE_OBJECT_PUT(interp, thisobj, *s2, ivp, 0);
		else	 SEE_OBJECT_DELETE(interp, thisobj, *s2);
		if (jvp) SEE_OBJECT_PUT(interp, thisobj, *s1, &jv, 0);
		else     SEE_OBJECT_DELETE(interp, thisobj, *s1);
	    } else
		return j;
	}
}

/**
 * Sorts the array segment by partitioning, and then sorting the
 * partitions. ("Quicksort", Cormen et al "Introduction to Algorithms").
 */
static void
qs_sort(interp, thisobj, lo, hi, cmpfn, s1, s2)
	struct SEE_interpreter *interp;
	struct SEE_object *thisobj;
	SEE_uint32_t lo, hi;
	struct SEE_object *cmpfn;
	struct SEE_string **s1, **s2;
{
	SEE_uint32_t q;

	if (lo < hi) {
	    q = qs_partition(interp, thisobj, lo, hi, cmpfn, s1, s2);
	    qs_sort(interp, thisobj, lo, q, cmpfn, s1, s2);
	    qs_sort(interp, thisobj, q + 1, hi, cmpfn, s1, s2);
	}
}

/* 15.4.4.11 */
static void
array_proto_sort(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_string *s1 = NULL, *s2 = NULL;
	SEE_uint32_t length;
	struct SEE_value v;
	struct SEE_object *cmpfn;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	length = SEE_ToUint32(interp, &v);

	if (argc < 1 || SEE_VALUE_GET_TYPE(argv[0]) == SEE_UNDEFINED)
		cmpfn = NULL;
	else if (SEE_VALUE_GET_TYPE(argv[0]) == SEE_OBJECT &&
		 SEE_OBJECT_HAS_CALL(argv[0]->u.object))
		cmpfn = argv[0]->u.object;
	else
		SEE_error_throw_string(interp, interp->TypeError,
			STR(bad_arg));

	qs_sort(interp, thisobj, 1, length, cmpfn, &s1, &s2);
	/*
	 * NOTE: the standard does not say that the length
	 * of the array should be updated after sorting.
	 * i.e., even as all the non-existent entries get moved 
	 * to the end of the array, the length property will remain
	 * unchanged. This remains consistent with 15.4.5.2.
	 */
	SEE_SET_OBJECT(res, thisobj);
}

/* 15.4.4.12 */
static void
array_proto_splice(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_value v;
	struct SEE_object *A;
	SEE_uint32_t r3, r5, r6, r17, k;
	struct SEE_string *s = NULL;

/*1*/	SEE_OBJECT_CONSTRUCT(interp, interp->Array, interp->Array, 0, NULL, &v);
	A = v.u.object;
/*2*/	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
/*3*/	r3 = SEE_ToUint32(interp, &v);
/*4*/	if (argc < 1) SEE_SET_NUMBER(&v, 0);
	else SEE_ToInteger(interp, argv[0], &v);
/*5*/	r5 = (v.u.number < 0) ? MAX((r3+v.u.number), 0)
			      : MIN(v.u.number, r3);
/*6*/	if (argc < 2) SEE_SET_NUMBER(&v, 0);
	else SEE_ToInteger(interp, argv[1], &v);
	r6 = MIN(MAX(v.u.number, 0), r3 - r5);
/*7*/	for (k = 0; k < r6; k++) {
/*9*/	    intstr(interp, &s, r5 + k);
/*10*/	    if (SEE_OBJECT_HASPROPERTY(interp, thisobj, s)) {
/*12*/		SEE_OBJECT_GET(interp, thisobj, s, &v);
/*11*/		intstr(interp, &s, k);
/*13*/		SEE_OBJECT_PUT(interp, A, s, &v, 0);
	    }
	}
/*16*/	SEE_SET_NUMBER(&v, r6); SEE_OBJECT_PUT(interp, A, STR(length), &v, 0);
/*17*/	r17 = argc < 2 ? 0 : argc - 2;
/*18*/	if (r17 != r6) {
/*19*/	    if (r17 <= r6) {
/*20*/		for (k = r5; k < r3 - r6; k++) {
/*22*/		    intstr(interp, &s, k + r6);
/*23*/		    if (SEE_OBJECT_HASPROPERTY(interp, thisobj, s)) {
/*25*/			SEE_OBJECT_GET(interp, thisobj, s, &v);
			intstr(interp, &s, k + r17);
/*26*/			SEE_OBJECT_PUT(interp, thisobj, s, &v, 0);
		    } else {
			intstr(interp, &s, k + r17);
/*28*/			SEE_OBJECT_DELETE(interp, thisobj, s);
		    }
		}
/*31*/		for (k = r3; k > r3-r6+r17; k--) {
/*33*/		    intstr(interp, &s, k - 1);
/*34*/		    SEE_OBJECT_DELETE(interp, thisobj, s);
		}
	    } else
/*37*/		for (k = r3 - r6; k > r5; k--) {
/*39*/		    intstr(interp, &s, k + r6 - 1);
/*41*/		    if (SEE_OBJECT_HASPROPERTY(interp, thisobj, s)) {
/*43*/			SEE_OBJECT_GET(interp, thisobj, s, &v);
			intstr(interp, &s, k + r17 - 1);
/*44*/			SEE_OBJECT_PUT(interp, thisobj, s, &v, 0);
		    } else {
			intstr(interp, &s, k + r17 - 1);
/*45*/			SEE_OBJECT_DELETE(interp, thisobj, s);
		    }
		}
	}
/*48*/	for (k = 2; k < argc; k++) {
      	    intstr(interp, &s, k - 2 + r5);
/*50*/	    SEE_OBJECT_PUT(interp, thisobj, s, argv[k], 0);
	}
/*53*/	SEE_SET_NUMBER(&v, r3-r6+r17);
	SEE_OBJECT_PUT(interp, thisobj, STR(length), &v, 0);
/*54*/	SEE_SET_OBJECT(res, A);
}

/* 15.4.4.13 */
static void
array_proto_unshift(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	SEE_uint32_t r2, r3, k;
	struct SEE_value v;
	struct SEE_string *s = NULL;

	SEE_OBJECT_GET(interp, thisobj, STR(length), &v);
	r2 = SEE_ToUint32(interp, &v);
	r3 = argc;
	for (k = r2; k > 0; k--) {
	    intstr(interp, &s, k - 1);
	    if (SEE_OBJECT_HASPROPERTY(interp, thisobj, s)) {
		SEE_OBJECT_GET(interp, thisobj, s, &v);
		intstr(interp, &s, k + r3 - 1);
		SEE_OBJECT_PUT(interp, thisobj, s, &v, 0);
	    } else {
		intstr(interp, &s, k + r3 - 1);
		SEE_OBJECT_DELETE(interp, thisobj, s);
	    }
	}
	for (k = 0; k < r3; k++) {
	    intstr(interp, &s, k);
	    SEE_OBJECT_PUT(interp, thisobj, s, argv[k], 0);
	}
	SEE_SET_NUMBER(res, r2 + r3);
	SEE_OBJECT_PUT(interp, thisobj, STR(length), res, 0);
}

/*------------------------------------------------------------
 * Array methods
 */

/*
 * Modify the length of the array. 
 * NB: this can be slow if the length is suddenly set
 * from 4,294,967,295 to 0 (because deletes occur).
 */
static void
array_setlength(interp, ao, val)
	struct SEE_interpreter *interp;
	struct array_object *ao;
	struct SEE_value *val;
{
	SEE_uint32_t i, newlen;
	struct SEE_string *s;
	struct name {
		struct SEE_string *s;
		struct name *next;
	} *name, *names = NULL;
	struct SEE_enum *e;
	int flags;

	newlen = SEE_ToUint32(interp, val);
	if (ao->length > newlen) {
	    e = SEE_OBJECT_ENUMERATOR(interp, (struct SEE_object *)&ao->native);
	    while ((s = SEE_ENUM_NEXT(interp, e, &flags))) 
		if (SEE_to_array_index(s, &i) && i >= newlen) {
		    name = SEE_NEW(interp, struct name);
		    name->s = s;
		    name->next = names;
		    names = name;
		}
	    for (name = names; name; name = name->next)
	        SEE_native_delete(interp, (struct SEE_object *)&ao->native,
		    name->s);
	}
	ao->length = newlen;
}

static void
array_get(interp, o, p, res)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p; 
	struct SEE_value *res;
{
	struct array_object *ao = (struct array_object *)o;

	if (SEE_string_cmp(p, STR(length)) == 0)
	    SEE_SET_NUMBER(res, ao->length);
	else
	    SEE_native_get(interp, o, p, res);
}

static void
array_put(interp, o, p, val, attr)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
	struct SEE_value *val;
	int attr;
{
	struct array_object *ao = (struct array_object *)o;
	SEE_uint32_t i;

	if (SEE_string_cmp(p, STR(length)) == 0)
	    array_setlength(interp, ao, val);
	else {
	    SEE_native_put(interp, o, p, val, attr);
	    if (SEE_to_array_index(p, &i))
		if (i >= ao->length)
		    ao->length = i + 1;
	}
}

static int
array_hasproperty(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	if (SEE_string_cmp(p, STR(length)) == 0)
	    return 1;
	else
	    return SEE_native_hasproperty(interp, o, p);
}

static int
array_delete(interp, o, p)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
	struct SEE_string *p;
{
	if (SEE_string_cmp(p, STR(length)) == 0)
	    return 0;
	else
	    return SEE_native_delete(interp, o, p);
}
