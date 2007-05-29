/*
 * Copyright (c) 2007
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
 * 3. Neither the name of Mr Leonard nor the names of the contributors
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
# include <config.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include <see/interpreter.h>
#include <see/type.h>
#include <see/mem.h>
#include <see/value.h>
#include <see/error.h>
#include <see/try.h>
#include <see/string.h>
#include <see/context.h>
#include <see/intern.h>

#include "dprint.h"
#include "code.h"
#include "stringdefs.h"
#include "scope.h"
#include "nmath.h"
#include "function.h"
#include "code1.h"
#include "replace.h"

#ifdef NDEBUG
# define CAST_CODE(c)	((struct code1 *)(c))
#else
# define CAST_CODE(c)	cast_code((c), __FILE__, __LINE__)
static struct code1 *cast_code(struct SEE_code *, const char *, int);
#endif

/* Prototypes */
static void code1_gen_op0(struct SEE_code *co, enum SEE_code_op0 op);
static void code1_gen_op1(struct SEE_code *co, enum SEE_code_op1 op, int n);
static void code1_gen_literal(struct SEE_code *co, const struct SEE_value *v);
static void code1_gen_func(struct SEE_code *co, struct function *f);
static void code1_gen_loc(struct SEE_code *co, struct SEE_throw_location *loc);
static void code1_gen_opa(struct SEE_code *co, enum SEE_code_opa op,
		SEE_code_patchable_t *patchp, SEE_code_addr_t addr);
static SEE_code_addr_t code1_here(struct SEE_code *co);
static void code1_patch(struct SEE_code *co, SEE_code_patchable_t patch,
		SEE_code_addr_t addr);
static void code1_maxstack(struct SEE_code *co, int);
static void code1_maxblock(struct SEE_code *co, int);
static void code1_close(struct SEE_code *co);
static void code1_eval(struct SEE_code *co, struct SEE_context *ctxt,
		struct SEE_value *res);

static unsigned int add_literal(struct code1 *code, 
		const struct SEE_value *val);
static unsigned int add_function(struct code1 *code, struct function *f);
static void add_byte(struct code1 *code, unsigned int c);
static unsigned int here(struct code1 *code);

static struct SEE_code_class code1_class = {
    "code1",
    code1_gen_op0,
    code1_gen_op1,
    code1_gen_literal,
    code1_gen_func,
    code1_gen_loc,
    code1_gen_opa,
    code1_here,
    code1_patch,
    code1_maxstack,
    code1_maxblock,
    code1_close,
    code1_eval
};

struct SEE_code *
_SEE_code1_alloc(interp)
    struct SEE_interpreter *interp;
{
    struct code1 *co;
    
    co = SEE_NEW(interp, struct code1);
    co->code.code_class = &code1_class;
    co->code.interpreter = interp;

    SEE_GROW_INIT(interp, &co->ginst, co->inst, co->ninst);
    SEE_GROW_INIT(interp, &co->gliteral, co->literal, co->nliteral);
    SEE_GROW_INIT(interp, &co->gfunc, co->func, co->nfunc);
    co->maxstack = -1;
    co->maxblock = -1;
    co->maxargc = 0;
    return (struct SEE_code *)co;
}

/* Adds a (unique) literal to the code object, returning its index */
static unsigned int
add_literal(code, val)
    struct code1 *code;
    const struct SEE_value *val;
{
    unsigned int i;
    struct SEE_interpreter *interp = code->code.interpreter;

    for (i = 0; i < code->nliteral; i++)
	if (memcmp(val, code->literal + i, sizeof *val) == 0)
	    return i;
    SEE_GROW_TO(interp, &code->gliteral, code->nliteral + 1);
    memcpy((void *)val, code->literal + i, sizeof *val);
    return i;
}


/* Adds a (unique) function to the code object, returning its index */
static unsigned int
add_function(code, f)
    struct code1 *code;
    struct function *f;
{
    unsigned int i;
    struct SEE_interpreter *interp = code->code.interpreter;

    for (i = 0; i < code->nfunc; i++)
	if (code->func[i] == f)
	    return i;
    SEE_GROW_TO(interp, &code->gfunc, code->nfunc + 1);
    code->func[i] = f;
    return i;
}

/* Appends a byte to the code stream  */
static void
add_byte(code, c)
    struct code1 *code;
    unsigned int c;
{
    struct SEE_interpreter *interp = code->code.interpreter;

    SEE_GROW_TO(interp, &code->ginst, code->ninst + 1);
    code->inst[code->ninst - 1] = c;
}

static unsigned int
here(code)
    struct code1 *code;
{
    return code->ninst;
}

/* Appends a 32-bit signed integer to the code stream  */
static void
add_word(code, n)
    struct code1 *code;
    SEE_int32_t n;
{
    unsigned int i = code->ninst;
    struct SEE_interpreter *interp = code->code.interpreter;

    SEE_GROW_TO(interp, &code->ginst, i + sizeof n);
    memcpy(code->func + i, &n, sizeof n);
}

/* Inserts a 32-bit signed integer into the code stream  */
static void
put_word(code, n, offset)
    struct code1 *code;
    SEE_int32_t n;
    unsigned int offset;
{
    memcpy(code->func + offset, &n, sizeof n);
}

/* Adds a byte followed by a compact integer */
static void
add_byte_arg(code, c, arg)
    struct code1 *code;
    unsigned char c;
    int arg;
{
    if (arg >= 0 && arg < 0x100) {
	add_byte(code, c | INST_ARG_BYTE);
	add_byte(code, arg & 0xff);
    } else {
	add_byte(code, c | INST_ARG_WORD);
	add_word(code, arg);
    }
}


/* Safe cast. Aborts if it is asked to cast a NULL pointer, or a SEE_code 
 * object that does not come from this module. */
#ifndef NDEBUG
static struct code1 *
cast_code(sco, file, line)
    struct SEE_code *sco;
    const char *file;
    int line;
{
    if (!sco || sco->code_class != &code1_class) {
	dprintf("%s:%d: internal error: cast to code1 failed [vers %s]\n",
	    file, line, PACKAGE_VERSION);
	abort();
    }
    return (struct code1 *)sco;
}
#endif

/*------------------------------------------------------------
 * SEE_code interface for parser
 */

static void
code1_gen_op0(sco, op)
	struct SEE_code *sco;
	enum SEE_code_op0 op;
{
	struct code1 *co = CAST_CODE(sco);

	switch (op) {
	case SEE_CODE_NOP:	add_byte(co, INST_NOP); break;
	case SEE_CODE_DUP:	add_byte(co, INST_DUP); break;
	case SEE_CODE_POP:	add_byte(co, INST_POP); break;
	case SEE_CODE_EXCH:	add_byte(co, INST_EXCH); break;
	case SEE_CODE_ROLL3:	add_byte(co, INST_ROLL3); break;
	case SEE_CODE_THROW:	add_byte(co, INST_THROW); break;
	case SEE_CODE_SETC:	add_byte(co, INST_SETC); break;
	case SEE_CODE_GETC:	add_byte(co, INST_GETC); break;
	case SEE_CODE_THIS:	add_byte(co, INST_THIS); break;
	case SEE_CODE_OBJECT:	add_byte(co, INST_OBJECT); break;
	case SEE_CODE_ARRAY:	add_byte(co, INST_ARRAY); break;
	case SEE_CODE_REGEXP:	add_byte(co, INST_REGEXP); break;
	case SEE_CODE_REF:	add_byte(co, INST_REF); break;
	case SEE_CODE_GETVALUE:	add_byte(co, INST_GETVALUE); break;
	case SEE_CODE_LOOKUP:	add_byte(co, INST_LOOKUP); break;
	case SEE_CODE_PUTVALUE:	add_byte(co, INST_PUTVALUE); break;
	case SEE_CODE_PUTVAR:	add_byte(co, INST_PUTVAR); break;
	case SEE_CODE_VAR:	add_byte(co, INST_VAR); break;
	case SEE_CODE_DELETE:	add_byte(co, INST_DELETE); break;
	case SEE_CODE_TYPEOF:	add_byte(co, INST_TYPEOF); break;
	case SEE_CODE_TOOBJECT:	add_byte(co, INST_TOOBJECT); break;
	case SEE_CODE_TONUMBER:	add_byte(co, INST_TONUMBER); break;
	case SEE_CODE_TOBOOLEAN:add_byte(co, INST_TOBOOLEAN); break;
	case SEE_CODE_TOSTRING:	add_byte(co, INST_TOSTRING); break;
	case SEE_CODE_TOPRIMITIVE:add_byte(co, INST_TOPRIMITIVE); break;
	case SEE_CODE_NEG:	add_byte(co, INST_NEG); break;
	case SEE_CODE_INV:	add_byte(co, INST_INV); break;
	case SEE_CODE_NOT:	add_byte(co, INST_NOT); break;
	case SEE_CODE_MUL:	add_byte(co, INST_MUL); break;
	case SEE_CODE_DIV:	add_byte(co, INST_DIV); break;
	case SEE_CODE_MOD:	add_byte(co, INST_MOD); break;
	case SEE_CODE_ADD:	add_byte(co, INST_ADD); break;
	case SEE_CODE_SUB:	add_byte(co, INST_SUB); break;
	case SEE_CODE_LSHIFT:	add_byte(co, INST_LSHIFT); break;
	case SEE_CODE_RSHIFT:	add_byte(co, INST_RSHIFT); break;
	case SEE_CODE_URSHIFT:	add_byte(co, INST_URSHIFT); break;
	case SEE_CODE_LT:	add_byte(co, INST_LT); break;
	case SEE_CODE_INSTANCEOF:add_byte(co, INST_INSTANCEOF); break;
	case SEE_CODE_IN:	add_byte(co, INST_IN); break;
	case SEE_CODE_EQ:	add_byte(co, INST_EQ); break;
	case SEE_CODE_SEQ:	add_byte(co, INST_SEQ); break;
	case SEE_CODE_BAND:	add_byte(co, INST_BAND); break;
	case SEE_CODE_BXOR:	add_byte(co, INST_BXOR); break;
	case SEE_CODE_BOR:	add_byte(co, INST_BOR); break;
	case SEE_CODE_S_ENUM:	add_byte(co, INST_S_ENUM); break;
	case SEE_CODE_S_WITH:	add_byte(co, INST_S_WITH); break;
	default: SEE_ASSERT(sco->interpreter, !"bad op0");
	}
}

static void
code1_gen_op1(sco, op, n)
	struct SEE_code *sco;
	enum SEE_code_op1 op; 
	int n;
{
	struct code1 *co = CAST_CODE(sco);

	switch (op) {
	case SEE_CODE_NEW:	add_byte_arg(co, INST_NEW, n); break;
	case SEE_CODE_CALL:	add_byte_arg(co, INST_CALL, n); break;
	case SEE_CODE_END:	add_byte_arg(co, INST_END, n); break;
	default: SEE_ASSERT(sco->interpreter, !"bad op1");
	}

	if (op == SEE_CODE_NEW || op == SEE_CODE_CALL) {
	    if (n > co->maxargc)
		co->maxargc = n;
	}
}

static void
code1_gen_literal(sco, v)
	struct SEE_code *sco;
	const struct SEE_value *v;
{
	struct code1 *co = CAST_CODE(sco);
	unsigned int id = add_literal(co, v);

	add_byte_arg(co, INST_LITERAL, id);
}

static void
code1_gen_func(sco, f)
	struct SEE_code *sco;
	struct function *f;
{
	struct code1 *co = CAST_CODE(sco);
	unsigned int id = add_function(co, f);

	add_byte_arg(co, INST_FUNC, id);
}

static void
code1_gen_loc(sco, loc)
	struct SEE_code *sco;
	struct SEE_throw_location *loc;
{
	/* TBD: XXX */
}

static void
code1_gen_opa(sco, opa, patchp, addr)
	struct SEE_code *sco;
	enum SEE_code_opa opa;
	SEE_code_patchable_t *patchp;
	SEE_code_addr_t addr;
{
	struct code1 *co = CAST_CODE(sco);
	unsigned char b;

	switch (opa) {
	case SEE_CODE_B_ALWAYS:	b = INST_B_ALWAYS; break;
	case SEE_CODE_B_TRUE:	b = INST_B_TRUE; break;
	case SEE_CODE_B_ENUM:	b = INST_B_ENUM; break;
	case SEE_CODE_S_TRYC:	b = INST_S_TRYC; break;
	case SEE_CODE_S_TRYF:	b = INST_S_TRYF; break;
	default: SEE_ASSERT(sco->interpreter, !"bad opa");return;
	}
	add_byte(co, b | INST_ARG_WORD);
	if (patchp)
	    *(unsigned int *)patchp = here(co);
	add_word(co, (SEE_uint32_t)addr);
}

static SEE_code_addr_t
code1_here(sco)
	struct SEE_code *sco;
{
	struct code1 *co = CAST_CODE(sco);

	return (SEE_code_addr_t)here(co);
}

static void
code1_patch(sco, patch, addr)
	struct SEE_code *sco;
	SEE_code_patchable_t patch;
	SEE_code_addr_t addr;
{
	struct code1 *co = CAST_CODE(sco);

	put_word(co, (SEE_uint32_t)addr, (unsigned int)patch);

}

static void
code1_maxstack(sco, maxstack)
	struct SEE_code *sco;
	int maxstack;
{
	struct code1 *co = CAST_CODE(sco);

	co->maxstack = maxstack;
}

static void
code1_maxblock(sco, maxblock)
	struct SEE_code *sco;
	int maxblock;
{
	struct code1 *co = CAST_CODE(sco);

	co->maxblock = maxblock;
}

static void
code1_close(sco)
	struct SEE_code *sco;
{
	/* Not implemented */
}

/*------------------------------------------------------------
 * Execution
 */

/* Converts a reference to a value, in situ */
static void
GetValue(interp, vp)
	struct SEE_interpreter *interp;
	struct SEE_value *vp;
{
	if (SEE_VALUE_GET_TYPE(vp) == SEE_REFERENCE) {
	    struct SEE_object *base = vp->u.reference.base;
	    struct SEE_string *prop = vp->u.reference.property;
	    if (base == NULL)
		SEE_error_throw_string(interp, interp->ReferenceError, prop);
	    SEE_OBJECT_GET(interp, base, prop, vp);
	}
}

static void
code1_eval(sco, ctxt, res)
    struct SEE_code *sco;
    struct SEE_context *ctxt;
    struct SEE_value *res;
{
    struct SEE_interpreter *interp = sco->interpreter;
    struct code1 *co = CAST_CODE(sco);
    struct SEE_string *str;
    struct SEE_value t, u, v;		/* scratch values */
    struct SEE_value *up, *vp, *wp;
    struct SEE_value **argv;
    struct SEE_value undefined, Number;
    struct SEE_object *obj, *baseobj;
    unsigned char *pc;
    struct SEE_value *stack, *stackbottom;
    struct SEE_location *location = NULL;
    unsigned char op;
    SEE_int32_t arg;
    SEE_int32_t int32;
    int i;


/*
 * The PUSH() and POP() macros work by setting /pointers/ into
 * the stack. They don't copy any values. Only pointers. So,
 * to use these, you call POP to get a pointer onto the stack
 * which you are expected to read; and you use PUSH to get a 
 * pointer into the stack where you are expected to store a 
 * result. Be very careful that you read from the popped pointer
 * before you write into the pushed pointer! The whole reason
 * it is done like this is to improve performance, and avoid
 * value copying. i.e. you must explicitly copy values if you
 * fear overwriting a pointer.
 */

#define POP0()	do {					\
	/* Macro to pop a value and discard it */	\
	SEE_ASSERT(interp, stack > stackbottom);	\
	stack--;					\
    } while (0)

#define POP(vp)	do {					\
	/* Macro to pop a value off the stack		\
	 * and set vp to the value */			\
	SEE_ASSERT(interp, stack > stackbottom);	\
	vp = --stack;					\
    } while (0)

#define PUSH(vp) do {					\
	/* Macro to prepare pushing a value onto the	\
	 * stack. vp is set to point to the storage. */	\
	vp = stack++;					\
    } while (0)

#define TOP(vp)	do {					\
	/* Macro to access the value on top of the	\
	 * without popping it. */			\
	SEE_ASSERT(interp, stack > stackbottom);	\
	vp = stack - 1;					\
    } while (0)


/* TONUMBER() ensures that the value pointer vp points at a number value.
 * It may use storage at the work pointer! */
#define TONUMBER(vp, work) do {				\
    if (SEE_VALUE_GET_TYPE(vp) != SEE_NUMBER) {		\
	SEE_ToNumber(interp, vp, work);			\
	vp = (work);					\
    }							\
 } while (0)

#define TOOBJECT(vp, work) do {				\
    if (SEE_VALUE_GET_TYPE(vp) != SEE_OBJECT) {		\
	SEE_ToObject(interp, vp, work);			\
	vp = (work);					\
    }							\
 } while (0)

    SEE_ASSERT(interp, co->maxstack >= 0);

    stackbottom = SEE_ALLOCA(interp, struct SEE_value, co->maxstack);
    argv = SEE_ALLOCA(interp, struct SEE_value *, co->maxargc);

    /* Constants */
    SEE_SET_UNDEFINED(&undefined);
    SEE_SET_OBJECT(&Number, interp->Number);

    SEE_SET_UNDEFINED(res);	    /* C = undefined */
    pc = co->inst;
    stack = stackbottom;
    for (;;) {
	/* Fetch next instruction byte */
	op = *pc++;

	if ((op & INST_ARG_MASK) == INST_ARG_NONE) {
	} else if ((op & INST_ARG_MASK) == INST_ARG_BYTE) {
	    arg = *pc++;
	} else {
	    memcpy(&arg, pc, sizeof arg);
	    pc += sizeof arg;
	}

	switch (op & INST_OP_MASK) {
	case INST_NOP:
	    break;

	case INST_DUP:
	    TOP(vp);
	    PUSH(up);
	    SEE_VALUE_COPY(up, vp);
	    break;

	case INST_POP:
	    POP0();
	    break;

	case INST_EXCH:
	    SEE_VALUE_COPY(&t, stack - 1);
	    SEE_VALUE_COPY(stack - 1, stack - 2);
	    SEE_VALUE_COPY(stack - 2, &t);
	    break;
	
	case INST_ROLL3:
	    SEE_VALUE_COPY(&t, stack - 1);
	    SEE_VALUE_COPY(stack - 1, stack - 2);
	    SEE_VALUE_COPY(stack - 2, stack - 3);
	    SEE_VALUE_COPY(stack - 3, &t);
	    break;

	case INST_THROW:
	    POP(up);	/* val */
	    SEE_THROW(interp, up);
	    /* NOTREACHED */
	    break;

	case INST_SETC:
	    POP(vp);
	    SEE_VALUE_COPY(res, vp);
	    break;

	case INST_GETC:
	    PUSH(vp);
	    SEE_VALUE_COPY(vp, res);
	    break;

	case INST_THIS:
	    PUSH(vp);
	    SEE_SET_OBJECT(vp, ctxt->thisobj);
	    break;

	case INST_OBJECT:
	    PUSH(vp);
	    SEE_SET_OBJECT(vp, interp->Object);
	    break;

	case INST_ARRAY:
	    PUSH(vp);
	    SEE_SET_OBJECT(vp, interp->Array);
	    break;

	case INST_REGEXP:
	    PUSH(vp);	/* obj */
	    SEE_SET_OBJECT(vp, interp->RegExp);
	    break;

	case INST_REF:
	    POP(up);	/* str */
	    POP(vp);	/* obj */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(up) == SEE_STRING);
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_OBJECT);
	    str = up->u.string;
	    obj = vp->u.object;
	    PUSH(wp);	/* ref */
	    _SEE_SET_REFERENCE(wp, obj, str);
	    break;

	case INST_GETVALUE:
	    TOP(vp);	/* any -> val */
	    GetValue(interp, vp);
	    break;

	case INST_LOOKUP:
	    POP(up);	/* str */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(up) == SEE_STRING);
	    str = up->u.string;
	    PUSH(vp);	/* val */
	    SEE_scope_lookup(interp, ctxt->scope, str, vp);
	    break;

	case INST_PUTVALUE:
	    POP(up);	/* val */
	    POP(vp);	/* ref */
	    if (SEE_VALUE_GET_TYPE(vp) == SEE_REFERENCE) {
		struct SEE_object *base = vp->u.reference.base;
		struct SEE_string *prop = vp->u.reference.property;
		if (base == NULL)
		    base = interp->Global;
		SEE_OBJECT_PUT(interp, base, prop, up, 0);
	    } else
		SEE_error_throw_string(interp, interp->ReferenceError,
		    STR(bad_lvalue));
	    break;

	case INST_PUTVAR:
	    POP(up);	/* val */
	    POP(vp);	/* str */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_STRING);
	    SEE_OBJECT_PUT(interp, ctxt->variable, vp->u.string, up,
		ctxt->varattr);
	    break;

	case INST_VAR:
	    POP(vp);	/* str */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_STRING);
	    if (!SEE_OBJECT_HASPROPERTY(interp, ctxt->variable, vp->u.string))
		SEE_OBJECT_PUT(interp, ctxt->variable, vp->u.string, &undefined,
		    ctxt->varattr);
	    break;

	case INST_DELETE:
	    TOP(vp);	/* any -> bool */
	    if (SEE_VALUE_GET_TYPE(vp) == SEE_REFERENCE) {
		struct SEE_object *base = vp->u.reference.base;
		struct SEE_string *prop = vp->u.reference.property;
		if (base == NULL || 
		    SEE_OBJECT_DELETE(interp, base, SEE_intern(interp, prop)))
			SEE_SET_BOOLEAN(vp, 1);
		else
			SEE_SET_BOOLEAN(vp, 0);
	    } else
		SEE_SET_BOOLEAN(vp, 0);
	    break;

	case INST_TYPEOF:
	    TOP(vp);	/* any -> str */
	    if (SEE_VALUE_GET_TYPE(vp) == SEE_REFERENCE &&
		vp->u.reference.base == NULL) 
		    SEE_SET_STRING(vp, STR(undefined));
	    else {
		struct SEE_string *s;
		GetValue(interp, vp);
		switch (SEE_VALUE_GET_TYPE(vp)) {
		case SEE_UNDEFINED:	s = STR(undefined); break;
		case SEE_NULL:     	s = STR(object);    break;
		case SEE_BOOLEAN:  	s = STR(boolean);   break;
		case SEE_NUMBER:   	s = STR(number);    break;
		case SEE_STRING:   	s = STR(string);    break;
		case SEE_OBJECT:   	s = SEE_OBJECT_HAS_CALL(vp->u.object)
					  ? STR(function)
					  : STR(object);    break;
		default:		s = STR(unknown);
		}
		SEE_SET_STRING(vp, s);
	    }
	    break;

	case INST_TOOBJECT:
	    TOP(vp);	    /* val -> obj */
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_OBJECT) {
		struct SEE_value tmp;
		SEE_VALUE_COPY(&tmp, vp);
		SEE_ToObject(interp, &tmp, vp);
	    }
	    break;

	case INST_TONUMBER:
	    TOP(vp);	    /* val -> num */
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_NUMBER) {
		struct SEE_value tmp;
		SEE_VALUE_COPY(&tmp, vp);
		SEE_ToNumber(interp, &tmp, vp);
	    }
	    break;

	case INST_TOBOOLEAN:
	    TOP(vp);	    /* val -> bool */
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_BOOLEAN) {
		struct SEE_value tmp;
		SEE_VALUE_COPY(&tmp, vp);
		SEE_ToBoolean(interp, &tmp, vp);
	    }
	    break;

	case INST_TOSTRING:
	    TOP(vp);	    /* val -> str */
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_STRING) {
		struct SEE_value tmp;
		SEE_VALUE_COPY(&tmp, vp);
		SEE_ToString(interp, &tmp, vp);
	    }
	    break;

	case INST_TOPRIMITIVE:
	    TOP(vp);	    /* val -> str */
	    if (SEE_VALUE_GET_TYPE(vp) == SEE_OBJECT) {
		struct SEE_object *obj = vp->u.object;
		SEE_OBJECT_DEFAULTVALUE(interp, obj, NULL, vp);
	    }
	    break;

	case INST_NEG:
	    TOP(vp);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_NUMBER);
	    vp->u.number = -vp->u.number;
	    break;

	case INST_INV:
	    TOP(vp);
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) != SEE_REFERENCE);
	    int32 = SEE_ToInt32(interp, vp);
	    SEE_SET_NUMBER(vp, ~int32);
	    break;

	case INST_NOT:
	    TOP(vp);
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_BOOLEAN);
	    vp->u.boolean = !vp->u.boolean;
	    break;

	case INST_MUL:
	    POP(vp);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_NUMBER);
	    TOP(up);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(up) == SEE_NUMBER);
	    SEE_SET_NUMBER(up, up->u.number * vp->u.number);
	    break;

	case INST_DIV:
	    POP(vp);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_NUMBER);
	    TOP(up);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(up) == SEE_NUMBER);
	    SEE_SET_NUMBER(up, up->u.number / vp->u.number);
	    break;

	case INST_MOD:
	    POP(vp);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_NUMBER);
	    TOP(up);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(up) == SEE_NUMBER);
	    SEE_SET_NUMBER(up, NUMBER_fmod(up->u.number, vp->u.number));
	    break;

	case INST_ADD:
	    POP(vp);	/* prim */
	    TOP(up);	/* prim -> num/str */
	    wp = up;
	    if (SEE_VALUE_GET_TYPE(up) == SEE_STRING ||
		    SEE_VALUE_GET_TYPE(vp) == SEE_STRING)
	    {
		if (SEE_VALUE_GET_TYPE(up) != SEE_STRING)
		    SEE_ToString(interp, up, &u), up = &u;
		if (SEE_VALUE_GET_TYPE(vp) != SEE_STRING)
		    SEE_ToString(interp, vp, &v), vp = &v;
		SEE_SET_STRING(wp, SEE_string_concat(interp,
		    up->u.string, vp->u.string));
	    } else {
		if (SEE_VALUE_GET_TYPE(up) != SEE_NUMBER)
		    SEE_ToNumber(interp, up, &u), up = &u;
		if (SEE_VALUE_GET_TYPE(vp) != SEE_NUMBER)
		    SEE_ToNumber(interp, vp, &v), vp = &v;
		SEE_SET_NUMBER(wp, up->u.number + vp->u.number);
	    }
	    break;

	case INST_SUB:
	    POP(vp);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_NUMBER);
	    TOP(up);	    /* num */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(up) == SEE_NUMBER);
	    SEE_SET_NUMBER(up, up->u.number - vp->u.number);
	    break;

	case INST_LSHIFT:
	    POP(vp);	/* val2 */
	    TOP(up);	/* val1 */
	    SEE_SET_NUMBER(up, SEE_ToInt32(interp, up) << 
		    (SEE_ToUint32(interp, vp) & 0x1f));
	    break;

	case INST_RSHIFT:
	    POP(vp);	/* val2 */
	    TOP(up);	/* val1 */
	    SEE_SET_NUMBER(up, SEE_ToInt32(interp, up) >> 
		    (SEE_ToUint32(interp, vp) & 0x1f));
	    break;

	case INST_URSHIFT:
	    POP(vp);	/* val2 */
	    TOP(up);	/* val1 */
	    SEE_SET_NUMBER(up, SEE_ToUint32(interp, up) >> 
		    (SEE_ToUint32(interp, vp) & 0x1f));
	    break;

	case INST_LT:
	    /* TBD */
	    break;

	case INST_INSTANCEOF:
	    POP(vp);	/* val */
	    TOP(up);	/* val */
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(instanceof_not_object));
	    if (!SEE_OBJECT_HAS_HASINSTANCE(vp->u.object))
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(no_hasinstance));
	    SEE_SET_BOOLEAN(up,
		SEE_OBJECT_HASINSTANCE(interp, vp->u.object, up));
	    break;

	case INST_IN:
	    POP(vp);	/* val */
	    TOP(up);	/* str */
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(up) == SEE_STRING);
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(in_not_object));
	    SEE_SET_BOOLEAN(up, SEE_OBJECT_HASPROPERTY(interp,
		vp->u.object, SEE_intern(interp, up->u.string)));
	    break;

	case INST_EQ:
	    /* TBD */
	    break;

	case INST_SEQ:
	    /* TBD */
	    break;

	case INST_BAND:
	    POP(vp);	    /* val */
	    TOP(up);	    /* val */
	    SEE_SET_NUMBER(up, 
		SEE_ToInt32(interp, up) & SEE_ToInt32(interp, vp));
	    break;

	case INST_BXOR:
	    POP(vp);	    /* val */
	    TOP(up);	    /* val */
	    SEE_SET_NUMBER(up, 
		SEE_ToInt32(interp, up) ^ SEE_ToInt32(interp, vp));
	    break;

	case INST_BOR:
	    POP(vp);	    /* val */
	    TOP(up);	    /* val */
	    SEE_SET_NUMBER(up, 
		SEE_ToInt32(interp, up) | SEE_ToInt32(interp, vp));
	    break;

	case INST_S_ENUM:
	    /* TBD */
	    break;

	case INST_S_WITH:
	    /* TBD */
	    break;

	/*--------------------------------------------------
	 * Instructions that take one argument
	 */

	case INST_NEW:
	    SEE_ASSERT(interp, stack >= stackbottom + arg + 1);
	    stack -= arg;
	    SEE_ASSERT(interp, arg <= co->maxargc);
	    for (i = 0; i < arg; i++)
		argv[i] = stack + i;
	    POP(vp);        /* obj */
	    if (SEE_VALUE_GET_TYPE(vp) == SEE_UNDEFINED)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(no_such_function));
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(not_a_function));
	    obj = vp->u.object;
	    if (!SEE_OBJECT_HAS_CONSTRUCT(obj))
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(not_a_constructor));
	    PUSH(up);
	    /* XXX SEE_TRACE_CALL */
	    SEE_OBJECT_CONSTRUCT(interp, obj, NULL, arg, argv, up);
	    /* XXX SEE_TRACE_RETURN */
	    break;

	case INST_CALL:
	    SEE_ASSERT(interp, stack >= stackbottom + arg + 1);
	    stack -= arg;
	    SEE_ASSERT(interp, arg <= co->maxargc);
	    for (i = 0; i < arg; i++)
		argv[i] = stack + i;
	    POP(vp);      /* ref */

	    baseobj = NULL;
	    if (SEE_VALUE_GET_TYPE(vp) == SEE_REFERENCE) {
		baseobj = vp->u.reference.base;
		if (baseobj && IS_ACTIVATION_OBJECT(baseobj))
		    baseobj = NULL;
		GetValue(interp, vp);
	    }
	    if (SEE_VALUE_GET_TYPE(vp) == SEE_UNDEFINED)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(no_such_function));
	    if (SEE_VALUE_GET_TYPE(vp) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(not_a_function));
	    obj = vp->u.object;
	    if (!SEE_OBJECT_HAS_CALL(obj))
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(not_callable));
	    PUSH(up);
	    /* XXX SEE_TRACE_CALL */
	    SEE_OBJECT_CALL(interp, obj, baseobj, arg, argv, up);
	    /* XXX SEE_TRACE_RETURN */
	    break;

	case INST_END:
	    /* TBD */
	    if (arg == 0)
		return;
	    break;

	/*--------------------------------------------------
	 * Instructions that take an address argument
	 */

	case INST_B_ALWAYS:
	    pc = co->inst + arg;
	    break;

	case INST_B_TRUE:
	    POP(vp);
	    SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(vp) == SEE_BOOLEAN);
	    if (vp->u.boolean)
		pc = co->inst + arg;
	    break;

	case INST_B_ENUM:
	    /* TBD */
	    break;

	case INST_S_TRYC:
	    /* TBD */
	    break;

	case INST_S_TRYF:
	    /* TBD */
	    break;

	case INST_FUNC:
	    /* TBD */
	    break;

	case INST_LITERAL:
	    PUSH(vp);
	    SEE_VALUE_COPY(vp, co->literal + arg);
	    break;

	case INST_LOC:
	    /* TBD */
	    break;

	default:
	    SEE_ASSERT(interp, !"bad instruction");
	}
    }
}

