/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_string_
#define _h_string_

/*
 * Strings are read-only, in-memory, sized arrays of 16-bit characters.
 * Strings can optionally provide a 'growto' method that is used
 * by append() and addch() to grow the string, although care must be
 * taken to ensure that the growing string's data is not in use elsewhere.
 *
 * How I Learned To Stop Worrying And Love The Garbage Collector:
 * There is no mechanism for disposing of strings 
 */

#include "config.h"

#if STDC_HEADERS
#include <stdio.h>
#include <stdarg.h>
#endif

#include "type.h"

struct SEE_stringclass;

struct SEE_string {
	unsigned int		 length;
	SEE_char_t		*data;
	struct SEE_stringclass	*stringclass;	/* NULL means static */
	struct SEE_interpreter	*interpreter;
	int 			 flags;
};
#define SEE_STRING_FLAG_INTERNED 1
#define SEE_STRING_FLAG_STATIC   2

#define SEE_STRING_DECL(chararray) \
	{ sizeof (chararray) / sizeof (SEE_char_t), (chararray), \
	  NULL, NULL, SEE_STRING_FLAG_STATIC }

struct SEE_stringclass {
	void (*growto)(struct SEE_string *, unsigned int);
};

void	SEE_string_addch(struct SEE_string *s, SEE_char_t ch);
void	SEE_string_append(struct SEE_string *s, const struct SEE_string *sffx);
void	SEE_string_append_int(struct SEE_string *s, int i);
void	SEE_string_fputs(const struct SEE_string *s, FILE *file);
int	SEE_string_cmp(const struct SEE_string *s1,
			  const struct SEE_string *s2);

struct SEE_string *SEE_string_new(struct SEE_interpreter *i,
				unsigned int space);
struct SEE_string *SEE_string_dup(struct SEE_interpreter *i,
				struct SEE_string *s);
struct SEE_string *SEE_string_substr(struct SEE_interpreter *i,
				struct SEE_string *s, int index, int length);
struct SEE_string *SEE_string_concat(struct SEE_interpreter *i,
				struct SEE_string *s1, struct SEE_string *s2);
struct SEE_string *SEE_string_sprintf(struct SEE_interpreter *i,
				const char *fmt, ...);
struct SEE_string *SEE_string_vsprintf(struct SEE_interpreter *i,
				const char *fmt, va_list ap);
struct SEE_string *SEE_string_literal(struct SEE_interpreter *i,
				const struct SEE_string *s);

#endif /* _h_string_ */
