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

#include <see/object.h>
#include <see/value.h>
#include <see/native.h>
#include <see/debug.h>
#include <see/string.h>
#include "context.h"

#ifndef NDEBUG
int SEE_context_debug = 0;
#endif

/*
 * Used in the 'PrimaryExpression: Identifier' production
 * to resolve an identifier within an execution context.
 * Returns a reference.
 * -- 10.1.4
 */
void
SEE_context_lookup(context, ident, res)
	struct context *context;
	struct SEE_string *ident; 
	struct SEE_value *res;
{
	struct SEE_scope *scope;
	struct SEE_interpreter *interp = context->interpreter;

	res->type = SEE_REFERENCE;
	res->u.reference.property = ident;

	for (scope = context->scope; scope; scope = scope->next) {

#ifndef NDEBUG
	    if (SEE_context_debug) {
		fprintf(stderr, "context_lookup: searching for '");
		SEE_string_fputs(ident, stderr);
		fprintf(stderr, "' in scope %p, obj = ", scope);
		SEE_PrintObject(context->interpreter, scope->obj, stderr);
		fprintf(stderr, "\n");
	    }
#endif

	    if (SEE_OBJECT_HASPROPERTY(interp, scope->obj, ident)) {
		res->u.reference.base = scope->obj;

#ifndef NDEBUG
	    if (SEE_context_debug) {
		fprintf(stderr, "context_lookup: found '");
		SEE_string_fputs(ident, stderr);
		fprintf(stderr, "' in ");
		SEE_PrintObject(context->interpreter, scope->obj, stderr);
		fprintf(stderr, "\n");
	    }
#endif

		return;
	    }
	}

#ifndef NDEBUG
	if (SEE_context_debug) {
	    fprintf(stderr, "context_lookup: not found: '");
	    SEE_string_fputs(ident, stderr);
	    fprintf(stderr, "'\n");
	}
#endif

	res->u.reference.base = NULL;
}

/*
 * Return false if the two scopes have observable difference.
 * In some cases, (esp. mutually recursion) this simple test
 * will incorrectly return false. Simple scope recursion is handled OK.
 */
int
SEE_context_scope_eq(s1, s2)
	struct SEE_scope *s1, *s2;
{
	struct SEE_object *o;

	while (s1 && s2) {
	    if (s1 == s2)
		return 1;
	    if (!SEE_OBJECT_JOINED(s1->obj, s2->obj))
		return 0;

	    /* Advance down the scope chains, skipping duplicates */
	    o = s1->obj;
	    do {
		    s1 = s1->next;
	    } while (s1 && SEE_OBJECT_JOINED(s1->obj, o));
	    o = s2->obj;
	    do {
		    s2 = s2->next;
	    } while (s2 && SEE_OBJECT_JOINED(s2->obj, o));
	}
	return s1 == s2;
}

