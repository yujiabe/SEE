/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_value_
#define _h_value_

/*
 * Values are small units of short-life, typed memory
 * that contain primitive information, object 
 * references or string references.
 */

#if STDC_HEADERS
# include <math.h>
#endif

#include "type.h"

/* define NULL now as a pointer, not as ansi C's (0) */
#ifndef NULL
#define NULL	((void *)0)
#endif

struct SEE_object;
struct SEE_string;
struct SEE_value;
struct SEE_interpreter;

struct SEE_reference {
	struct SEE_object *base;
	struct SEE_string *property;
};

/* Value types */
enum SEE_type {
	SEE_UNDEFINED,
	SEE_NULL,
	SEE_BOOLEAN,
	SEE_NUMBER,
	SEE_STRING,
	SEE_OBJECT,
	SEE_REFERENCE,			/* internal type (8.7) */
	SEE_COMPLETION			/* internat type (8.9) */
};

struct SEE_completion {
	struct SEE_value *value;
	void *target;
	enum { SEE_NORMAL, SEE_BREAK, SEE_CONTINUE, 
	       SEE_RETURN, SEE_THROW } type;
};

/* Value storage */
struct SEE_value {
	enum SEE_type		      type;
	union {
		SEE_number_t	      number;
		SEE_boolean_t	      boolean;
		struct SEE_object    *object;
		struct SEE_string    *string;
		struct SEE_reference  reference;
		struct SEE_completion completion;
	} u;
};

#if SEE_NUMBER_IS_FLOAT
# define SEE_NUMBER_ISNAN(v)    isnanf((v)->u.number)
# define SEE_NUMBER_ISPINF(v)   (isinff((v)->u.number) && (v)->u.number > 0)
# define SEE_NUMBER_ISNINF(v)   (isinff((v)->u.number) && (v)->u.number < 0)
# define SEE_NUMBER_ISINF(v)    isinff((v)->u.number)
# define SEE_NUMBER_ISFINITE(v) finitef((v)->u.number)
#elif SEE_NUMBER_IS_DOUBLE
# define SEE_NUMBER_ISNAN(v)    isnan((v)->u.number)
# define SEE_NUMBER_ISPINF(v)   (isinf((v)->u.number) && (v)->u.number > 0)
# define SEE_NUMBER_ISNINF(v)   (isinf((v)->u.number) && (v)->u.number < 0)
# define SEE_NUMBER_ISINF(v)    isinf((v)->u.number)
# define SEE_NUMBER_ISFINITE(v) finite((v)->u.number)
#endif

#define SEE_VALUE_COPY(dst, src)		\
	memcpy(dst, src, sizeof (struct SEE_value))

#define SEE_SET_UNDEFINED(v)			\
	(v)->type = SEE_UNDEFINED

#define SEE_SET_NULL(v)				\
	(v)->type = SEE_NULL

#define SEE_SET_BOOLEAN(v, b) 			\
    do {					\
	(v)->type = SEE_BOOLEAN;		\
	(v)->u.boolean = (b);			\
    } while (0)

#define SEE_SET_NUMBER(v, n) 			\
    do {					\
	(v)->type = SEE_NUMBER;			\
	(v)->u.number = (n);			\
    } while (0)

#define SEE_SET_STRING(v, s)			\
    do {					\
	(v)->type = SEE_STRING;			\
	(v)->u.string = (s);			\
    } while (0)

#define SEE_SET_OBJECT(v, o)			\
    do {					\
	(v)->type = SEE_OBJECT;			\
	(v)->u.object = (o);			\
    } while (0)

/* Return completion - NB: 'val' must NOT be on the stack */
#define SEE_SET_COMPLETION(v, typ, val, tgt)	\
    do {					\
	(v)->type = SEE_COMPLETION;		\
	(v)->u.completion.type = (typ);		\
	(v)->u.completion.value = (val);	\
	(v)->u.completion.target = (tgt);	\
    } while (0)

/* Converters */
void SEE_ToPrimitive(struct SEE_interpreter *i,
			struct SEE_value *val, struct SEE_value *type, 
			struct SEE_value *res);
void SEE_ToBoolean(struct SEE_interpreter *i, struct SEE_value *val, 
			struct SEE_value *res);
void SEE_ToNumber(struct SEE_interpreter *i, struct SEE_value *val,
			struct SEE_value *res);
void SEE_ToInteger(struct SEE_interpreter *i, struct SEE_value *val,
			struct SEE_value *res);
void SEE_ToString(struct SEE_interpreter *i, struct SEE_value *val,
			struct SEE_value *res);
void SEE_ToObject(struct SEE_interpreter *i, struct SEE_value *val, 
			struct SEE_value *res);

/* Integer converters */
SEE_int32_t  SEE_ToInt32(struct SEE_interpreter *i, struct SEE_value *val);
SEE_uint32_t SEE_ToUint32(struct SEE_interpreter *i, struct SEE_value *val);
SEE_uint16_t SEE_ToUint16(struct SEE_interpreter *i, struct SEE_value *val);

/* "0123456789abcdef" */
extern char SEE_hexstr[16], SEE_hexstr_uppercase[16];

#endif /* _h_value_ */
