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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by David Leonard and 
 *      contributors.
 * 4. Neither the name of Mr Leonard nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID LEONARD AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID LEONARD OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id$ */

#if HAVE_CONFIG_H
# include <see/config.h>
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

#include "stringdefs.h"

/*
 * Internalised strings.
 *
 * 'Interning' a string means to replace it with a unique pointer
 * to a previously stored string.  
 * When a previously unseen string is interned, a copy is made.
 * The result is that string comparison becomes a pointer comparison
 * (much faster) and is primarily used for identifiers.
 */

#define HASHTABSZ	257
#define HASHLENMAX	8

struct intern {
	struct intern *next;
	struct SEE_string *string;
};

typedef struct intern *(intern_tab_t[HASHTABSZ]);
static struct intern ** find(struct SEE_interpreter *, struct SEE_string *);

static struct intern *
make(interp, s)
	struct SEE_interpreter *interp;
	struct SEE_string *s;
{
	struct intern *i;

	i = SEE_NEW(interp, struct intern);
	i->string = s;
	s->flags |= SEE_STRING_FLAG_INTERNED;
	i->next = NULL;
	return i;
}

static struct intern **
find(interp, s)
	struct SEE_interpreter *interp;
	struct SEE_string *s;
{
	struct intern **x;
	unsigned int j, hash = 0;
	intern_tab_t *intern_tab;

	intern_tab = (intern_tab_t *)interp->intern_tab;

	for (j = 0; j < HASHLENMAX && j < s->length; j++)
		hash = (hash << 1) ^ s->data[j];
	x = &(*intern_tab)[hash % HASHTABSZ];
	while (*x && SEE_string_cmp((*x)->string, s) != 0)
		x = &((*x)->next);
	return x;
}

void
SEE_intern_init(interp)
	struct SEE_interpreter *interp;
{
	intern_tab_t *intern_tab;
	int hash, i;
	struct intern **x;

	intern_tab = SEE_NEW(interp, intern_tab_t);
	for (hash = 0; hash < HASHTABSZ; hash++)
		(*intern_tab)[hash] = NULL;

	interp->intern_tab = intern_tab;

	/* Add all the predefined strings to the intern table */
	for (i = 0; i < _SEE_STR_MAX; i++) {
		x = find(interp, &SEE_stringtab[i]);
		SEE_ASSERT(interp, *x == NULL);
		*x = make(interp, &SEE_stringtab[i]);
	}
}

struct SEE_string *
SEE_intern(interp, s)
	struct SEE_interpreter *interp;
	struct SEE_string *s;
{
	struct intern **x;

	if (s == NULL)
		return NULL;

	if (s >= &SEE_stringtab[0] && s < &SEE_stringtab[_SEE_STR_MAX])
		return s;

	if (s->flags & SEE_STRING_FLAG_INTERNED)
		return s;

	x = find(interp, s);
	if (!*x)
		*x = make(interp, SEE_string_dup(interp, s));
	return (*x)->string;
}
