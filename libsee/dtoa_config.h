/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

/*
 * Configuration directives for dtoa when used by SEE
 */

#include <float.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined(__i386__)
#   define IEEE_8087
#endif

#if defined(__m68k__) || defined(__sparc__)
#   define IEEE_MC68k 
#endif

#if defined(__vax__) && !defined(VAX)
#   define VAX
#endif

/* #define IBM for IBM mainframe-style floating-point arithmetic. */

#define Long	int32_t
#define ULong	u_int32_t
#define LLong	int64_t
#define ULLong	u_int64_t

#define MALLOC	malloc

#define NO_ERRNO
#define IEEE_Arith
#define CONST	const

/* #define No_leftright to omit left-right logic in fast floating-point */

/* #define Honor_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3 */
/* #define Check_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3 */
/* #define RND_PRODQUOT to use rnd_prod and rnd_quot (assembly routines */
/* #define ROUND_BIASED for IEEE-format with biased rounding. */
/* #define Inaccurate_Divide for IEEE-format with correctly rounded */
/* #define KR_headers for old-style C function headers. */
/* #define Bad_float_h if your system lacks a float.h or if it does not */
/* #define INFNAN_CHECK on IEEE systems to cause strtod to check for */
/* #define MULTIPLE_THREADS if the system offers preemptively scheduled */
/* #define NO_IEEE_Scale to disable new (Feb. 1997) logic in strtod that */
/* #define YES_ALIAS to permit aliasing certain double values with */
/* #define USE_LOCALE to use the current locale's decimal_point value. */
/* #define SET_INEXACT if IEEE arithmetic is being used and extra */

#define strtod		SEE_strtod
#define dtoa		SEE_dtoa
#define freedtoa	SEE_freedtoa
