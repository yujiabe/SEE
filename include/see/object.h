/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_object_
#define _h_object_

struct SEE_value;
struct SEE_object;
struct SEE_string;
struct SEE_scope;
struct SEE_enum;
struct SEE_interpreter;

/* 
 * Class method types. Even though ECMAScript uses a prototype
 * inheritance model, objects still have to carry something like
 * a vtbl.
 */
typedef void	(*SEE_get_fn_t)(struct SEE_interpreter *i,
			struct SEE_object *obj, struct SEE_string *prop,
			struct SEE_value *res);
typedef void	(*SEE_put_fn_t)(struct SEE_interpreter *i,
			struct SEE_object *obj, struct SEE_string *prop,
			struct SEE_value *res, int flags);
typedef int	(*SEE_boolean_fn_t)(struct SEE_interpreter *i,
			struct SEE_object *obj, struct SEE_string *prop);
typedef int	(*SEE_hasinstance_fn_t)(struct SEE_interpreter *i,
			struct SEE_object *obj, struct SEE_value *instance);
typedef void	(*SEE_default_fn_t)(struct SEE_interpreter *i,
			struct SEE_object *obj, struct SEE_value *hint, 
			struct SEE_value *res);
typedef void	(*SEE_call_fn_t)(struct SEE_interpreter *i, 
			struct SEE_object *obj, struct SEE_object *thisobj,
			int argc, struct SEE_value **argv,
			struct SEE_value *res);
typedef struct SEE_enum *(*SEE_enumerator_fn_t)(struct SEE_interpreter *i,
			struct SEE_object *obj);

/*
 * Core object definition: an object is a set of named properties.
 * All objects must implement Prototype, Class, Get, Put, CanPut,
 * HasProperty, Delete and DefaultValue. (DefaultValue may simply
 * throw a TypeError, and Proptype may be NULL)
 */

struct SEE_objectclass {
	struct SEE_string *	Class;			/* [[Class]] */
	SEE_get_fn_t		Get;			/* [[Get]] */
	SEE_put_fn_t		Put;			/* [[Put]] */
	SEE_boolean_fn_t	CanPut;			/* [[CanPut]] */
	SEE_boolean_fn_t	HasProperty;		/* [[HasProperty]] */
	SEE_boolean_fn_t	Delete;			/* [[Delete]] */
	SEE_default_fn_t	DefaultValue;		/* [[DefaultValue]] */
	SEE_enumerator_fn_t	enumerator;		/* enumerator */
	SEE_call_fn_t		Construct;		/* [[Construct]] */
	SEE_call_fn_t		Call;			/* [[Call]] */
	SEE_hasinstance_fn_t	HasInstance;		/* [[HasInstance]] */
};

struct SEE_object {
	struct SEE_objectclass *objectclass;
	struct SEE_object *	Prototype;		/* [[Prototype]] */
};

#define SEE_OBJECT_GET(interp, obj, name, res)				\
	(*(obj)->objectclass->Get)(interp, obj, name, res)
#define SEE_OBJECT_PUT(interp, obj, name, val, attrs)			\
	(*(obj)->objectclass->Put)(interp, obj, name, val, attrs)
#define SEE_OBJECT_CANPUT(interp, obj, name)				\
	(*(obj)->objectclass->CanPut)(interp, obj, name)
#define SEE_OBJECT_HASPROPERTY(interp, obj, name)			\
	(*(obj)->objectclass->HasProperty)(interp, obj, name)
#define SEE_OBJECT_DELETE(interp, obj, name)				\
	(*(obj)->objectclass->Delete)(interp, obj, name)
#define SEE_OBJECT_DEFAULTVALUE(interp, obj, hint, res)			\
	(*(obj)->objectclass->DefaultValue)(interp, obj, hint, res)
#define SEE_OBJECT_CONSTRUCT(interp, obj, thisobj, argc, argv, res)	\
	SEE_object_construct(interp, obj, thisobj, argc, argv, res)
#define _SEE_OBJECT_CONSTRUCT(interp, obj, thisobj, argc, argv, res)	\
	(*(obj)->objectclass->Construct)(interp, obj, thisobj, argc, argv, res)
#define SEE_OBJECT_CALL(interp, obj, thisobj, argc, argv, res)		\
	SEE_object_call(interp, obj, thisobj, argc, argv, res)
#define _SEE_OBJECT_CALL(interp, obj, thisobj, argc, argv, res)		\
	(*(obj)->objectclass->Call)(interp, obj, thisobj, argc, argv, res)
#define SEE_OBJECT_HASINSTANCE(interp, obj, instance)			\
	(*(obj)->objectclass->HasInstance)(interp, obj, instance)
#define SEE_OBJECT_ENUMERATOR(interp, obj)				\
	(*(obj)->objectclass->enumerator)(interp, obj)

#define SEE_OBJECT_HAS_CALL(obj)	((obj)->objectclass->Call)
#define SEE_OBJECT_HAS_CONSTRUCT(obj)	((obj)->objectclass->Construct)
#define SEE_OBJECT_HAS_HASINSTANCE(obj)	((obj)->objectclass->HasInstance)
#define SEE_OBJECT_HAS_ENUMERATOR(obj)	((obj)->objectclass->enumerator)

/* [[Put]] attributes */
#define SEE_ATTR_READONLY   0x01
#define SEE_ATTR_DONTENUM   0x02
#define SEE_ATTR_DONTDELETE 0x04
#define SEE_ATTR_INTERNAL   0x08

struct SEE_enumclass {
	void (*reset)(struct SEE_interpreter *i, struct SEE_enum *e);
	struct SEE_string *(*next)(struct SEE_interpreter *i,
			struct SEE_enum *e, int *flags_return);
};

struct SEE_enum {
	struct SEE_enumclass *enumclass;
};

#define SEE_ENUM_RESET(i,e)    ((e)->enumclass->reset)(i,e)
#define SEE_ENUM_NEXT(i,e,dep) ((e)->enumclass->next)(i,e,dep)

/*
 * Test to see if two objects are "joined", i.e. a change to one
 * is reflected in the other.
 */
#define SEE_OBJECT_JOINED(a,b)					\
	((a) == (b) || 						\
	  ((a)->objectclass == (b)->objectclass &&		\
	   SEE_function_is_joined(a,b)))
int SEE_function_is_joined(struct SEE_object *a, struct SEE_object *b);

/* Convenience function equivalent to "new Object()" */
struct SEE_object *SEE_Object_new(struct SEE_interpreter *);

/* Wrappers that check for recursion limits being reached */
void SEE_object_call(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
void SEE_object_construct(struct SEE_interpreter *, struct SEE_object *,
	struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

#endif /* _h_object_ */
