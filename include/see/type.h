/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_type_
#define _h_type_

/*
 * Machine-dependent types and definitions
 */

#include <stdlib.h>
#include <sys/types.h>

typedef u_int16_t	  SEE_uint16_t;	  /* 16-bit unsigned integer */
typedef int32_t	  	  SEE_int32_t;	  /* 32-bit signed integer */
typedef u_int32_t	  SEE_uint32_t;   /* 32-bit unsigned integer */
typedef SEE_uint16_t	  SEE_char_t;     /* UTF-16 encoding */
typedef SEE_uint32_t	  SEE_unicode_t;  /* UCS-4 encoding */
typedef double            SEE_number_t;   /* 64-bit IEEE754 floating point */
typedef unsigned char     SEE_boolean_t;  /* 1 (or more) bit unsigned integer */

/* an attribute indicating which functions never return. (can be empty) */
#define SEE_dead	__attribute__((__noreturn__))

/* numeric constants */
#define SEE_Infinity	((SEE_number_t) 1.0 / 0.0)   /* use HUGE_VAL instead? */
#define SEE_NaN		((SEE_number_t) 0.0 / 0.0)

/* on-stack allocation */
#define SEE_ALLOCA(n, t)	(t *)((n) ? alloca((n) * sizeof (t)) : 0)

#endif /* _h_type_ */
