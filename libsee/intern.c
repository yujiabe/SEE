/*
 * Copyright (c) 2003
 *      David Leonard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of David Leonard nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* $Id$ */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
#endif

#include <see/type.h>
#include <see/string.h>
#include <see/try.h>
#include <see/mem.h>
#include <see/intern.h>
#include <see/error.h>
#include <see/interpreter.h>
#include <see/system.h>
#include <see/object.h>

#include "stringdefs.h"
#include "dprint.h"

/*
 * Internalised strings.
 *
 * 'Interning' a string means to replace it with a unique pointer
 * to a previously stored string.  
 * When a previously unseen string is interned, a copy is made.
 * The result is that string comparison becomes a pointer comparison
 * (much faster) and is primarily used for identifiers.
 *
 * This module uses a three-level intern strategy: the first level is the
 * static list of library strings generated by string.defs. The second level
 * is the application-wide "global" intern table, which applications must set
 * up early and not change after the creation of any interpreter. The
 * third level is the interpreter-local intern cache.
 * 
 * This strategy allow the sharing of application static strings,
 * while avoiding the need for mutual exclusion techniques between
 * interpreters (since the library and application static strings are
 * read-only).
 */

#define HASHTABSZ	257
#define HASHLENMAX	8		/* prefix of string hashed on */

struct intern {				/* element in the intern hash table */
	struct intern *next;
	struct SEE_string *string;
};
typedef struct intern *(intern_tab_t[HASHTABSZ]);

/* Prototypes */
static struct intern *  make(struct SEE_interpreter *, struct SEE_string *);
static unsigned int     hash(const struct SEE_string *);
static struct intern ** find(intern_tab_t *, struct SEE_string *,
			     unsigned int);
static int internalized(struct SEE_interpreter *interp,
			const struct SEE_string *s);
static void global_init(void);

/** System-wide intern table */
static intern_tab_t	global_intern_tab;
static int		global_intern_tab_initialized;

#ifndef NDEBUG
static int		global_intern_tab_locked = 0;
int			SEE_debug_intern;
#endif

#ifndef NDEBUG
static int
string_only_contains_ascii(s)
	const char *s;
{
	for (; *s; s++)
		if (*s & 0x80)
			return 0;
	return 1;
}
#endif

/**
 * Make an intern entry in the hash table containing the string s,
 *  and flag s as being interned.
 */
static struct intern *
make(interp, s)
	struct SEE_interpreter *interp;		/* may be NULL */
	struct SEE_string *s;
{
	struct intern *i;

	i = SEE_NEW(interp, struct intern);
	i->string = s;
	s->flags |= SEE_STRING_FLAG_INTERNED;
	i->next = NULL;
	return i;
}

/** Compute the hash value of a UTF-16 string */
static unsigned int
hash(s)
	const struct SEE_string *s;
{
	unsigned int j, h = 0;
	for (j = 0; j < HASHLENMAX && j < s->length; j++)
		h = (h << 1) ^ s->data[j];
	return h % HASHTABSZ;
}

/** 
 * Compute the hash value of an ASCII string.
 * Returns the same value as if the ASCII string had been converted to a
 * UTF-16 string and passed to hash().
 * Assumes the input string is indeed ASCII (bytes in 0x00..0x7f).
 */
static unsigned int
hash_ascii(s, lenret)
	const char *s;
	unsigned int *lenret;
{
	unsigned int j, h = 0;
	const char *t;

	for (j = 0, t = s; j < HASHLENMAX && *t; j++, t++)
		h = (h << 1) ^ *t;
	while (*t) t++;
	*lenret = t - s;
	return h % HASHTABSZ;
}

/** Find an interned string */
static struct intern **
find(intern_tab, s, hash)
	intern_tab_t *intern_tab;
	struct SEE_string *s;
	unsigned int hash;
{
	struct intern **x;

	x = &(*intern_tab)[hash];
	while (*x && SEE_string_cmp((*x)->string, s) != 0)
		x = &((*x)->next);
	return x;
}

/** 
 * Returns true if a SEE_string matches the ASCII string s.
 * Assumes the string s is ASCII (0x00..0x7f).
 */
static int
ascii_eq(str, s)
	struct SEE_string *str;
	const char *s;
{
	unsigned int len = str->length;
	SEE_char_t *c = str->data;

	while (len--)
		if (!*s)
			return 0;
		else if (*c++ != *s++)
			return 0;
	return *s == 0;
}

/** Find an interned ASCII string */
static struct intern **
find_ascii(intern_tab, s, hash)
	intern_tab_t *intern_tab;
	const char *s;
	unsigned int hash;
{
	struct intern **x;

	x = &(*intern_tab)[hash];
	while (*x && !ascii_eq((*x)->string, s))
		x = &((*x)->next);
	return x;
}

/** Create an interpreter-local intern table */
void
_SEE_intern_init(interp)
	struct SEE_interpreter *interp;
{
	intern_tab_t *intern_tab;
	unsigned int i;

	global_init();
#ifndef NDEBUG
	global_intern_tab_locked = 1;
#endif

	intern_tab = SEE_NEW(interp, intern_tab_t);
	for (i = 0; i < HASHTABSZ; i++)
		(*intern_tab)[i] = NULL;

	interp->intern_tab = intern_tab;
}

/* Returns true if the string is already internalized */
static int
internalized(interp, s)
	struct SEE_interpreter *interp;
	const struct SEE_string *s;
{
	/*
	 * A string is internalized if
	 *  - is already internalized in this interpreter or the global hash
	 *  - is one of the static resource strings
	 */
	return 
	    ((!s->interpreter || s->interpreter == interp) &&
	     (s->flags & SEE_STRING_FLAG_INTERNED)) ||
	    (s >= STRn(0) && s < STRn(SEE_nstringtab));
}

/**
 * Intern a string relative to an interpreter. Also reads the global table
 * Note that a different pointer to s is *ALWAYS* returned unless s was
 * originally returned by SEE_intern. In other words, on the first occurrence
 * of a string, it is duplicated. This makes it safe to intern strings that
 * will be later grown. However, you MUST not alter content of strings 
 * returned from this function.
 */
struct SEE_string *
SEE_intern(interp, s)
	struct SEE_interpreter *interp;
	struct SEE_string *s;
{
	struct intern **x;
	unsigned int h;
#ifndef NDEBUG
	const char *where = NULL;
# define WHERE(f) do { if (SEE_debug_intern) where=f; } while (0)
#else
# define WHERE(f) /* nothing */
#endif

	/* Allow interning NULL to lessen the number of error checks */
	if (!s)
	    return NULL;

	if (internalized(interp, s)) {
#ifndef NDEBUG
		if (SEE_debug_intern) {
		    dprintf("INTERN ");
		    dprints(s);
		    dprintf(" -> %p [interned]\n", s);
		}
#endif
		return s;
	}

	/* If the string is from another interpreter, then it must
	 * have been intern'd already. This is to prevent race conditions
	 * with string whose content is changing. */
	SEE_ASSERT(interp, !s->interpreter || s->interpreter == interp ||
		(s->flags & SEE_STRING_FLAG_INTERNED));

	/* Look in system-wide intern table first */
	h = hash(s);
	x = find(&global_intern_tab, s, h);
	WHERE("global");
	if (!*x) {
		x = find(interp->intern_tab, s, h);
		WHERE("local");
		if (!*x) {
			*x = make(interp, _SEE_string_dup_fix(interp, s));
			WHERE("new");
		}
	}
#ifndef NDEBUG
	if (SEE_debug_intern) {
	    dprintf("INTERN ");
	    dprints(s);
	    dprintf(" -> %p [%s h=%d]\n", (*x)->string, where, h);
	}
#endif
	return (*x)->string;

}

/** Efficiently converts an ASCII C string into an internalised SEE string */
struct SEE_string *
SEE_intern_ascii(interp, s)
	struct SEE_interpreter *interp;
	const char *s;
{
	struct SEE_string *str;
	const char *t;
	SEE_char_t *c;
	unsigned int h, len;
	struct intern **x;
#ifndef NDEBUG
	const char *where = NULL;
#endif

	SEE_ASSERT(interp, s != NULL);
	SEE_ASSERT(interp, string_only_contains_ascii(s));

	h = hash_ascii(s, &len);
	x = find_ascii(&global_intern_tab, s, h);
	WHERE("global");
	if (!*x) {
	    x = find_ascii(interp->intern_tab, s, h);
	    WHERE("local");
	    if (!*x) {
		WHERE("new");
		str = SEE_NEW(interp, struct SEE_string);
		str->length = len;
		str->data = SEE_NEW_STRING_ARRAY(interp, SEE_char_t, len);
		for (c = str->data, t = s; *t;)
			*c++ = *t++;
		str->interpreter = interp;
		str->stringclass = NULL;
		str->flags = 0;
	    	SEE_ASSERT(interp, hash(str) == h);
		*x = make(interp, str);
		}
	    }
#ifndef NDEBUG
	if (SEE_debug_intern)
	    dprintf("INTERN %s -> %p [%s h=%d ascii]\n", 
		s, (*x)->string, where, h);
#endif
	return (*x)->string;
}

/*
 * Interns a string, and frees the original string.
 */
void
SEE_intern_and_free(interp, sp)
	struct SEE_interpreter *interp;
	struct SEE_string **sp;
{
	struct SEE_string *is;

	is = SEE_intern(interp, *sp);
	SEE_ASSERT(interp, is != *sp);
#ifndef NDEBUG
	if (SEE_debug_intern) {
	    dprintf("INTERN ");
	    dprints(*sp);
	    dprintf(" -> %p [hit & free]\n", is);
	}
#endif
	SEE_string_free(interp, sp);
	*sp = is;
}

static void
global_init()
{
	unsigned int i, h;
	struct intern **x;

	if (global_intern_tab_initialized)
		return;

	/* Add all the predefined strings to the global intern table */
	for (i = 0; i < SEE_nstringtab; i++) {
		h = hash(STRn(i));
		x = find(&global_intern_tab, STRn(i), h);
		if (*x == NULL) 
			*x = make(NULL, STRn(i));
	}
	global_intern_tab_initialized = 1;
}

/**
 * Adds an ASCII string into the system-wide intern table if
 * not already there.
 * Should not be called after any interpeters are created.
 */
struct SEE_string *
SEE_intern_global(s)
	const char *s;
{
	struct SEE_string *str;
	const char *t;
	SEE_char_t *c;
	unsigned int h, len;
	struct intern **x;

#ifndef NDEBUG
	if (global_intern_tab_locked)
		SEE_ABORT(NULL, "SEE_intern_global: table is now read-only");
#endif
	global_init();

	h = hash_ascii(s, &len);
	x = find_ascii(&global_intern_tab, s, h);
	if (*x) return (*x)->string;

	str = SEE_NEW(NULL, struct SEE_string);
	str->length = len;
	str->data = SEE_NEW_STRING_ARRAY(NULL, SEE_char_t, len);
	for (c = str->data, t = s; *t;)
		*c++ = *t++;
	str->interpreter = NULL;
	str->stringclass = NULL;
	str->flags = 0;
	*x = make(NULL, str);
	return (*x)->string;
}

#ifndef NDEBUG
/**
 * Raises an assertion failure if the passed string is not internalised.
 * This function used by the SEE_OBJECT_*() macros.
 */
struct SEE_string *
_SEE_intern_assert(interp, s)
	struct SEE_interpreter *interp;
        struct SEE_string *s;
{
	if (s)
	    SEE_ASSERT(interp, internalized(interp, s));
	return s;
}
#endif
