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

#if STDC_HEADERS
# include <stdio.h>   
#endif

#include <see/see.h>
#include <see/value.h>
#include <see/mem.h>
#include <see/string.h>
#include <see/object.h>
#include <see/native.h>
#include <see/no.h>
#include <see/cfunction.h>
#include <see/interpreter.h>

#include "stringdefs.h"

/*
 * cfunction
 * 
 * These are the ECMAScript objects that wrap native C functions.
 * They are referred to in the introduction of section 15 of the
 * standard as 'built-in' functions.
 *
 * They have a [[Call]] property which invokes the appropriate 
 * C function, and also has a "length" property which 
 * gives the typical number of arguments to the [[Call]] method.
 * Their prototype is the anonymous CFunction prototype object (whose 
 * own prototype is Function.prototype).
 *
 * The length property is implemented in a way equivalent to the
 * requirement that it "has the attributes { ReadOnly, DontDelete,
 * DontEnum } (and not others)." (15)
 *
 */

struct cfunction {
	struct SEE_object object;
	SEE_call_fn_t func;
	int length;
	struct SEE_string *name;
};

static void cfunction_get(struct SEE_interpreter *, struct SEE_object *, 
	struct SEE_string *, struct SEE_value *);
static int cfunction_hasproperty(struct SEE_interpreter *, struct SEE_object *, 
	struct SEE_string *);
static void cfunction_call(struct SEE_interpreter *, struct SEE_object *, 
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

/*
 * CFunction object class
 */
struct SEE_objectclass SEE_cfunction_class = {
	STR(Function),		/* Class */
	cfunction_get,		/* Get */
	SEE_no_put,		/* Put */
	SEE_no_canput,		/* CanPut */
	cfunction_hasproperty,	/* HasProperty */
	SEE_no_delete,		/* Delete */
	SEE_native_defaultvalue,/* DefaultValue */
	SEE_no_enumerator,	/* enumerator */
	NULL,			/* Construct (15) */
	cfunction_call		/* Call */
};

/*
 * Return a CFunction object that wraps a C function
 */
struct SEE_object *
SEE_cfunction_make(interp, func, name, length)
	struct SEE_interpreter *interp;
	SEE_call_fn_t func;
	struct SEE_string *name;
	int length;
{
	struct cfunction *f;

	f = SEE_NEW(interp, struct cfunction);
	f->object.objectclass = &SEE_cfunction_class;
	f->object.Prototype = interp->Function_prototype;	/* 15 */
	f->func = func;
	f->name = name;
	f->length = length;

	return (struct SEE_object *)f;
}

/*------------------------------------------------------------
 * CFunction class methods
 */

static void
cfunction_get(interp, o, p, res)
	struct SEE_interpreter *interp;
        struct SEE_object *o;
        struct SEE_string *p;
        struct SEE_value *res;
{
	struct cfunction *f = (struct cfunction *)o;

	if ((interp->compatibility & SEE_COMPAT_EXT1) &&
		SEE_string_cmp(p, STR(__proto__)) == 0)
	{
		SEE_SET_OBJECT(res, o->Prototype);
		return;
	}
	if (SEE_string_cmp(p, STR(length)) == 0)
		SEE_SET_NUMBER(res, f->length);
	else
		SEE_OBJECT_GET(interp, o->Prototype, p, res);
}

static int
cfunction_hasproperty(interp, o, p)
	struct SEE_interpreter *interp;
        struct SEE_object *o;
        struct SEE_string *p;
{
	if (SEE_string_cmp(p, STR(length)) == 0)
		return 1;
	return SEE_OBJECT_HASPROPERTY(interp, o->Prototype, p);
}

static void
cfunction_call(interp, o, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *o, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct cfunction *f = (struct cfunction *)o;
	(*f->func)(interp, o, thisobj, argc, argv, res);
}

void
SEE_cfunction_toString(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_string *s;
	struct cfunction *f = (struct cfunction *)thisobj;

	s = SEE_string_new(interp, 0);
	SEE_string_append(s, STR(cfunction_body1));
	SEE_string_append(s, f->name);
	SEE_string_append(s, STR(cfunction_body2));
	SEE_string_append_int(s, (int)f->func);
	SEE_string_append(s, STR(cfunction_body3));
	SEE_SET_STRING(res, s);
}

struct SEE_string *
SEE_cfunction_getname(interp, o)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
{
	struct cfunction *f = (struct cfunction *)o;

	return f->name;
}
