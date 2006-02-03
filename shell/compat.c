/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <string.h>
#endif

#include <see/see.h>

#include "compat.h"

static struct { const char *name; int flag; } names[] = {
	{ "262_3b",	SEE_COMPAT_262_3B },
	{ "arrayjoin1",	SEE_COMPAT_ARRAYJOIN1 },
	{ "ext1",	SEE_COMPAT_EXT1 },
	{ "sgmlcom",	SEE_COMPAT_SGMLCOM },
	{ "undefdef",	SEE_COMPAT_UNDEFDEF },
	{ "utf_unsafe",	SEE_COMPAT_UTF_UNSAFE },
};

/*
 * Convert a compatability flag name into an integer bit-flag.
 * The following compatbility flags are understood (see the documentation
 * for more details). They may be prefixed with 'no_' to turn them off.
 *
 *  sgmlcom      - treat SGML comments in program text as normal comments
 *  utf_unsafe   - pass through invalid UTF-8 characters without error
 *  undefdef     - return undefined for unknown names instead of throwing
 *  262_3b       - provide the optional functions in section 3B of standard
 *  ext1         - enable local SEE extension set number 1
 *  arrayjoin1   - emulate old array.join(undefined) bug
 */
int
compat_tovalue(name, compatibility)
	const char *name;
	int *compatibility;
{

	int i;
	int no = 0;
	int bit = 0;

	if (name[0] == 'n' && name[1] == 'o' && name[2] == '_') {
		name += 3;
		no = 1;
	}
	for (i = 0; i < sizeof names / sizeof names[0]; i++) 
		if (strcmp(name, names[i].name) == 0) {
			bit = names[i].flag;
			break;
		}
	if (name[0] >= '0' && name[0] <= '9')
		bit = atoi(name);
	else if (bit == 0) {
		fprintf(stderr, "WARNING: unknown compatability flag '%s'\n",
		    name);
		return -1;
	}

	if (no)
		*compatibility &= ~bit;
	else
		*compatibility |= bit;
	return 0;
}


/*
 * Parse a string consisting of whitespace separated compatibility flag names,
 * and modify the compatibility value accordingly.
 * THE STRING IS MODIFIED
 * If the string starts with "=", then the compatibility value is initialised
 * to zero.
 * If an error occurs, returns -1 and does not modify the compatibility value.
 */
int
compat_fromstring(name, compatibility)
	char *name;
	int *compatibility;
{
	int compat;
	char *start, *s;

	compat = 0;
	s = name;
	if (*s == '+') {
		compat = *compatibility;
		s++;
	}
	while (*s == ' ') s++;
	while (*s) {
		start = s;
		while (*s && *s != ' ') s++;
		if (*s == ' ') *s++ = '\0';
		if (compat_tovalue(start, &compat) == -1)
			return -1; 
	}
	*compatibility = compat;
	return 0;
}

/*
 * Converts a compatibility flag value to a string suitable for
 * use with compat_fromstring(). The returned string always starts
 * with "=".
 */
struct SEE_string *
compat_tostring(interp, compatibility)
	struct SEE_interpreter *interp;
	int compatibility;
{
	struct SEE_string *s = SEE_string_new(interp, 0);
	int i;

	for (i = 0; i < sizeof names / sizeof names[0]; i++)  {
	    if (i)
		    SEE_string_addch(s, ' ');
	    if (!(compatibility & names[i].flag)) {
		    SEE_string_addch(s, 'n');
		    SEE_string_addch(s, 'o');
		    SEE_string_addch(s, '_');
	    }
	    SEE_string_append_ascii(s, names[i].name);
	}
	return s;
}
