/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_type_
#define _h_type_

/*
 * Machine-dependent types and definitions
 */

#include <stdlib.h>
#include <sys/types.h>

/* 16-bit unsigned integer */
#if SIZEOF_UNSIGNED_SHORT == 2
typedef unsigned short	  SEE_uint16_t;
#elif SIZEOF_UNSIGNED_INT == 2
typedef unsigned int	  SEE_uint16_t;
#else
# error "cannot provide type for SEE_uint16_t"
#endif

/* 32-bit signed integer */
#if SIZEOF_SIGNED_SHORT == 4
typedef signed short	  SEE_int32_t;
#elif SIZEOF_SIGNED_INT == 4
typedef signed int	  SEE_int32_t;
#elif SIZEOF_SIGNED_LONG == 4
typedef signed long	  SEE_int32_t;
#else
# error "cannot provide type for SEE_int32_t"
#endif

/* 32-bit unsigned integer */
#if SIZEOF_UNSIGNED_SHORT == 4
typedef unsigned short	  SEE_uint32_t;
#elif SIZEOF_UNSIGNED_INT == 4
typedef unsigned int	  SEE_uint32_t;
#elif SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long	  SEE_uint32_t;
#else
# error "cannot provide type for SEE_uint32_t"
#endif

/* 64-bit signed integer */
#if SIZEOF_SIGNED_INT == 8
typedef signed int	  SEE_int64_t;
#elif SIZEOF_SIGNED_LONG == 8
typedef signed long	  SEE_int64_t;
#elif SIZEOF_SIGNED_LONG_LONG == 8
typedef signed long long  SEE_int64_t;
#else
# error "cannot provide type for SEE_int64_t"
#endif

/* 64-bit unsigned integer */
#if SIZEOF_UNSIGNED_INT == 8
typedef unsigned int	  SEE_uint64_t;
#elif SIZEOF_UNSIGNED_LONG == 8
typedef unsigned long	  SEE_uint64_t;
#elif SIZEOF_UNSIGNED_LONG_LONG == 8
typedef unsigned long long SEE_uint64_t;
#else
# error "cannot provide type for SEE_uint64_t"
#endif

/* 64-bit floating point */
#if SIZEOF_FLOAT == 8
typedef float SEE_number_t;
#elif SIZEOF_DOUBLE == 4
typedef double SEE_number_t;
#else
# error "cannot provide type for SEE_number_t"
#endif

typedef unsigned char     SEE_boolean_t;  /* 1 (or more) bit unsigned integer */

typedef SEE_uint16_t	  SEE_char_t;     /* UTF-16 encoding */
typedef SEE_uint32_t	  SEE_unicode_t;  /* UCS-4 encoding */

/* an attribute indicating which functions never return. (can be empty) */
#define SEE_dead	__attribute__((__noreturn__))

/* numeric constants */
#define SEE_Infinity	((SEE_number_t) 1.0 / 0.0)   /* use HUGE_VAL instead? */
#define SEE_NaN		((SEE_number_t) 0.0 / 0.0)

/* on-stack allocation */
#define SEE_ALLOCA(n, t)	(t *)((n) ? alloca((n) * sizeof (t)) : 0)

#endif /* _h_type_ */
