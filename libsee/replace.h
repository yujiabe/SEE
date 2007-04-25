/* Copyright (c) 2007, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_replace_
#define _SEE_h_replace_

/*
 * Declaration of replaced functions
 */

#if !HAVE_MEMCMP
#define memcmp _SEE_memcmp
int memcmp(void *, void *, int);
#endif

#if !HAVE_MEMMOVE
#define memmove _SEE_memmove
void memmove(void *, void *, int);
#endif

#endif /* _SEE_h_replace_ */
