/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_unicode_
#define _SEE_h_unicode_

#include <see/type.h>

/*
 * Macros to test if a given character is in a Unicode 4.0 (meta)class
 *	Cf - Format control   (=Cf)
 *	Zs - Whitespace       (=Zs)
 *	IS - identifier start (=Lu+Ll+Lt+Lm+Lo+Nl + '$' + '_')
 *	IP - identifier part  (=Lu+Ll+Lt+Lm+Lo+Nl+Mn+Mc+Nd+Pc + '$' + '_')
 */
#define UNICODE_IS_Cf(c) _UNICODE_IS(c, SEE_unicode_Cf, 11)
#define UNICODE_IS_Zs(c) _UNICODE_IS(c, SEE_unicode_Zs, 14)
#define UNICODE_IS_IS(c) _UNICODE_IS(c, SEE_unicode_IS, 11)
#define UNICODE_IS_IP(c) _UNICODE_IS(c, SEE_unicode_IP, 11)

/*
 * The sparse bit tables would be 600kB in total size, but because
 * of some clustering in the Unicode layout this can be 
 * easily reduced to 20kB using a two-level lookup. My
 * analyses showed that using the lower 11 bits for Cf, IP and IS
 * produced the smallest tables, while Zs was minimal at 14 bits.
 * (Based on UnicodeData.txt 4.0).  (See gen.c)
 */
extern unsigned char* SEE_unicode_Cf[];
extern unsigned char* SEE_unicode_Zs[];
extern unsigned char* SEE_unicode_IS[];
extern unsigned char* SEE_unicode_IP[];
#define _UNICODE_MAX	0x10ffff
#define _UNICODE_IS(c, table, grp)				\
	((c) < _UNICODE_MAX && 					\
		table[(c)>>grp] &&				\
		(table[(c)>>grp][((c) >> 3) & ((1<<(grp-3))-1)]	\
		    & (1<<((c)&7))))

extern SEE_unicode_t  SEE_unicode_Zscodes[];
extern int	      SEE_unicode_Zscodeslen;

#endif /* _SEE_h_unicode_ */
