/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_native_
#define _SEE_h_native_

/*
 * All native objects have a hash table of attributed name-value properties.
 * Static declarations of objects can provide an array of property 
 * initialisations that are inserted into the hashtable on the first
 * property access.
 */

#include "value.h"
#include "object.h"

#define SEE_ATTR_DEFAULT	(SEE_ATTR_DONTENUM)	/* (see section 15) */
#define SEE_ATTR_LENGTH		(SEE_ATTR_READONLY |	\
				 SEE_ATTR_DONTDELETE |	\
				 SEE_ATTR_DONTENUM)

struct SEE_interpreter;
struct SEE_property;

#define SEE_NATIVE_HASHLEN  257
struct SEE_native {
	struct SEE_object       object;
	struct SEE_property *   properties[SEE_NATIVE_HASHLEN];
};

void SEE_native_get(struct SEE_interpreter *i, struct SEE_object *obj, 
	struct SEE_string *prop, struct SEE_value *res);
void SEE_native_put(struct SEE_interpreter *i, struct SEE_object *obj, 
	struct SEE_string *prop, struct SEE_value *val, int flags);
int  SEE_native_canput(struct SEE_interpreter *i, struct SEE_object *obj, 
	struct SEE_string *prop);
int  SEE_native_hasproperty(struct SEE_interpreter *i, struct SEE_object *obj, 
	struct SEE_string *prop);
int  SEE_native_hasownproperty(struct SEE_interpreter *i,
	struct SEE_object *obj, struct SEE_string *prop);
int  SEE_native_getownattr(struct SEE_interpreter *i, struct SEE_object *obj, 
	struct SEE_string *prop);
int  SEE_native_delete(struct SEE_interpreter *i, struct SEE_object *obj, 
	struct SEE_string *prop);
void SEE_native_defaultvalue(struct SEE_interpreter *i, struct SEE_object *obj, 
	struct SEE_value *hint, struct SEE_value *res);
struct SEE_enum *SEE_native_enumerator(struct SEE_interpreter *i, 
	struct SEE_object *obj);

struct SEE_object *SEE_native_new(struct SEE_interpreter *i);
void SEE_native_init(struct SEE_native *obj, struct SEE_interpreter *i,
		struct SEE_objectclass *obj_class, 
		struct SEE_object *prototype);

#endif /* _SEE_h_native_ */
