#include "dtoa.h"

/*
 * Pass dtoa() calls through to the native dtoa() in libc.
 */

double strtod(const char *s00, char **se)
char *dtoa(double d, int mode, int ndigits, int *decpt, int *sign, char **rve);
void freedtoa(char *s);

double
SEE_strtod(const char *s00, char **se)
{
	return strtod(s00, se);
}

char *
SEE_dtoa(double d, int mode, int ndigits, int *decpt, int *sign, char **rve)
{
	return SEE_dtoa(d, mode, ndigits, decpt, sign, rve);
}

void
SEE_freedtoa(char *s)
{
	freedtoa(s);
}
