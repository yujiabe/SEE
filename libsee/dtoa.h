/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_dtoa_
#define _SEE_h_dtoa_

double	SEE_strtod(const char *s00, char **se);
char *	SEE_dtoa(double d, int mode, int ndigits, int *decpt, int *sign, 
		char **rve);
void	SEE_freedtoa(char *s);

#endif /* _SEE_h_dtoa_ */
