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

/*
 * Combined parser and evaluator.
 *
 * This file contains two threads (storylines): the LL(2) recursive
 * descent parser thread, and the semantic functions thread. The parsing and 
 * semantics stages are grouped together by their common productions in the
 * grammar, to facilitate reference to the ECMA-262 standard.
 *
 * The input to the parser is an instance of the lexical analyser.
 * The output of the parser is an abstract syntax tree (AST). The input to
 * the evaluator is the AST, and the output of the evaluator is a SEE_value.
 *
 * For each production PROD in the grammar, the function PROD_parse() 
 * allocates and populates a 'node' structure, representing the root
 * of the syntax tree that represents the parsed production. Each node
 * holds a 'nodeclass' pointer to semantics information as well as
 * production-specific information. 
 *
 * Names of structures and functions have been chosen to correspond 
 * with the production names from the standard's grammar.
 *
 * The semantic functions in each node class are the following:
 *
 *  - PROD_eval() functions are called at runtime to implement
 *    the behaviour of the program. They "evaluate" the program.
 *
 *  - PROD_fproc() functions are called at execution time to generate
 *    the name/value bindings between container objects and included
 *    function objects. (It finds functions and assigns them to properties.)
 *    They provide a parallel, but independent, recusive semantic operation
 *    described in the standard as "process[ing] for function 
 *    declarations".
 *
 *  - PROD_print() functions are used to print the abstract syntax tree 
 *    to stdout during debugging.
 *
 * TODO This file is far too big; need to split it up.
 * TODO Compact/bytecode intermediate form
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
# include <stdlib.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include <see/mem.h>
#include <see/string.h>
#include <see/value.h>
#include <see/intern.h>
#include <see/object.h>
#include <see/cfunction.h>
#include <see/input.h>
#include <see/eval.h>
#include <see/try.h>
#include <see/error.h>
#include <see/debug.h>
#include <see/interpreter.h>
#include <see/context.h>
#include <see/system.h>

#include "lex.h"
#include "parse.h"
#include "scope.h"
#include "function.h"
#include "enumerate.h"
#include "tokens.h"
#include "stringdefs.h"
#include "dtoa.h"
#include "dprint.h"
#include "nmath.h"

#if WITH_PARSER_CODEGEN
# include "code.h"
#endif

#if WITH_PARSER_PRINT
# include "parse_print.h"
#endif

#include "parse_node.h"

#define MAX3(a, b, c)    MAX(MAX(a, b), c)
#define MAX4(a, b, c, d) MAX(MAX(a, b), MAX(c, d))

#ifndef NDEBUG
int SEE_parse_debug = 0;
int SEE_eval_debug = 0;
#endif

/*------------------------------------------------------------
 * structure types
 */

/*
 * Abstract syntax tree basic structure
 */
struct node;

#if WITH_PARSER_CODEGEN
struct code_varscope {
	struct SEE_string *ident;
	unsigned int id;
	int in_scope;
};

struct code_context {
	struct SEE_code *code;

	/* A structure used to hold the break and continue patchables */
	struct patchables {
		SEE_code_patchable_t *cont_patch;
		unsigned int ncont_patch;
		struct SEE_growable gcont_patch;
		SEE_code_patchable_t *break_patch;
		unsigned int nbreak_patch;
		struct SEE_growable gbreak_patch;
		unsigned int target;
		struct patchables *prev;
		int continuable;
		unsigned int block_depth;
	} *patchables;

	/* The current block depth. Starts at zero. */
	unsigned int block_depth, max_block_depth;

	/* True when directly in the variables scope. This
	 * allows us to use VREF statements instead of LOOKUP.
	 * It goes false inside 'with' and 'catch' blocks.
	 * Individual vars can be descoped;
	 */
	int in_var_scope;

	/* True when we want to disable constant folding */
	int no_const;

	struct code_varscope *varscope;
	unsigned int          nvarscope;
	struct SEE_growable   gvarscope;
};
#endif

#if WITH_PARSER_EVAL
extern void (*_SEE_nodeclass_eval[])(struct node *, 
        struct SEE_context *, struct SEE_value *);
#endif
#if WITH_PARSER_CODEGEN
extern void (*_SEE_nodeclass_codegen[])(struct node *, 
        struct code_context *);
#endif
extern int (*_SEE_nodeclass_isconst[])(struct node *, 
        struct SEE_interpreter *);

#if WITH_PARSER_CODEGEN
        /* unsigned int node.maxstack */
	/* Keeps track of the maximum stack space needed
	 * to run the code. */
	
        /* unsigned int node.is */
	/* Represents a union of the possible types that
	 * are left on top of the stack when code from
	 * an Expression node is run */
# define CG_TYPE_UNDEFINED	0x01
# define CG_TYPE_NULL		0x02
# define CG_TYPE_BOOLEAN	0x04
# define CG_TYPE_NUMBER		0x08
# define CG_TYPE_STRING		0x10
# define CG_TYPE_OBJECT		0x20
# define CG_TYPE_REFERENCE	0x40
# define CG_TYPE_PRIMITIVE	(CG_TYPE_UNDEFINED | \
				 CG_TYPE_NULL | \
				 CG_TYPE_BOOLEAN | \
				 CG_TYPE_NUMBER | \
				 CG_TYPE_STRING)
# define CG_TYPE_VALUE		(CG_TYPE_PRIMITIVE | CG_TYPE_OBJECT)
# define CG_IS_VALUE(n)	    (!((n)->is & CG_TYPE_REFERENCE))
# define CG_IS_PRIMITIVE(n) (!((n)->is & (CG_TYPE_REFERENCE|CG_TYPE_OBJECT)))
# define CG_IS_BOOLEAN(n)   ((n)->is == CG_TYPE_BOOLEAN)
# define CG_IS_NUMBER(n)    ((n)->is == CG_TYPE_NUMBER)
# define CG_IS_STRING(n)    ((n)->is == CG_TYPE_STRING)
# define CG_IS_OBJECT(n)    ((n)->is == CG_TYPE_OBJECT)
#endif

/*
 * A label is the identifier (string) before a statement that binds a 
 * "labelset" to the statement. Conversely, a labelset represents the 
 * set of labels associated with a statement. Labelsets are the targets 
 * of break and continue statements. If a 'break' or 'continue' statement 
 * is followed by an identifier, then the current label stack is searched 
 * for the corresponding labelset.
 *
 * For 'break' or 'continue' statements without a label, the label stack 
 * is searched for the internal names ".BREAK" or ".CONTINUE", respectively.
 * All 'iteration' statements are initialised with a labelset containing
 * one or both of .BREAK or .CONTINUE.
 *
 * A break to a labelset branches to the end of its statement. A continue
 * to a labelset branches to its statement's 'continue point', which is 
 * usually the body of the iteration.
 *
 * During any statement the labelset_current() method returns the currently
 * active labelset.
 * 
 * A labelset is given a unique target number only if and when it is 
 * referenced. In this way, the parser generates sequential branch targets 
 * IDs. Unused labelsets have an internal ID of -1.
 */

struct labelset {
	int		 continuable;	    /* can be target of continue */
	unsigned int	 target;	    /* unique id or -1 */
	struct labelset *next;		    /* list of all labelsets */
};

struct label {
	struct SEE_string *name;	    /* interned label name */
	struct labelset	*labelset;	    /* containing labelset */
	struct SEE_throw_location location; /* where the label is defined */
	struct label	*next;		    /* stack link of active labels */
};

#define UNGET_MAX 3
struct parser {
	struct SEE_interpreter *interpreter;
	struct lex 	 *lex;
	int		  unget, unget_end;
	struct SEE_value  unget_val[UNGET_MAX];
	int               unget_tok[UNGET_MAX];
	int               unget_lin[UNGET_MAX];
	SEE_boolean_t     unget_fnl[UNGET_MAX];
	int 		  noin;	  /* ignore 'in' in RelationalExpression */
	int		  is_lhs; /* derived LeftHandSideExpression */
	int		  funcdepth;
	struct var	**vars;		    /* list of declared variables */
	struct labelset	 *labelsets;	    /* list of all labelsets */
	struct label     *labels;	    /* stack of active labels */
	struct labelset	 *current_labelset; /* statement's labelset or NULL */
};


/*------------------------------------------------------------
 * function prototypes
 */

static struct node *new_node_internal(struct SEE_interpreter*interp, int sz, 
        enum nodeclass_enum nc, struct SEE_string* filename, int lineno,
	const char *dbg_nc);
static struct node *new_node(struct parser *parser, int sz, 
        enum nodeclass_enum nc, const char *dbg_nc);
static void parser_init(struct parser *parser, 
        struct SEE_interpreter *interp, struct lex *lex);
static unsigned int target_lookup(struct parser *parser, 
	struct SEE_string *name, int kind);
static int lookahead(struct parser *parser, int n);
#if WITH_PARSER_EVAL
static void trace_event(struct SEE_context *ctxt, enum SEE_trace_event);
static struct SEE_traceback *traceback_enter(struct SEE_interpreter *interp, 
        struct SEE_object *callee, struct SEE_throw_location *loc, 
        int call_type);
static void traceback_leave(struct SEE_interpreter *interp, 
        struct SEE_traceback *old_tb);
static void GetValue(struct SEE_context *context, struct SEE_value *v, 
        struct SEE_value *res);
static void PutValue(struct SEE_context *context, struct SEE_value *v, 
        struct SEE_value *w);
static void Literal_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void RegularExpressionLiteral_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void StringLiteral_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void PrimaryExpression_this_eval(struct node *n, 
        struct SEE_context *context, struct SEE_value *res);
static void PrimaryExpression_ident_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ArrayLiteral_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void ObjectLiteral_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void Arguments_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void MemberExpression_new_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void MemberExpression_dot_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void MemberExpression_bracket_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void CallExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void PostfixExpression_inc_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void PostfixExpression_dec_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_delete_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_void_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_typeof_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_preinc_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_predec_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_plus_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_minus_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_inv_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void UnaryExpression_not_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void MultiplicativeExpression_mul_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void MultiplicativeExpression_div_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context, 
	struct SEE_value *res);
static void MultiplicativeExpression_div_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void MultiplicativeExpression_mod_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context, 
	struct SEE_value *res);
static void MultiplicativeExpression_mul_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context, 
	struct SEE_value *res);
static void MultiplicativeExpression_mod_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AdditiveExpression_add_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context,
	struct SEE_value *res);
static void AdditiveExpression_add_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AdditiveExpression_sub_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context,
	struct SEE_value *res);
static void AdditiveExpression_sub_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ShiftExpression_lshift_common(struct SEE_value *r2, 
        struct node *bn, struct SEE_context *context, struct SEE_value *res);
static void ShiftExpression_lshift_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ShiftExpression_rshift_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context, 
	struct SEE_value *res);
static void ShiftExpression_rshift_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ShiftExpression_urshift_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context,
	struct SEE_value *res);
static void ShiftExpression_urshift_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void RelationalExpression_sub(struct SEE_interpreter *interp, 
        struct SEE_value *x, struct SEE_value *y, struct SEE_value *res);
static void RelationalExpression_lt_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void RelationalExpression_gt_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void RelationalExpression_le_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void RelationalExpression_ge_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void RelationalExpression_instanceof_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void RelationalExpression_in_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void EqualityExpression_eq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void EqualityExpression_ne_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void EqualityExpression_seq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void EqualityExpression_sne_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void BitwiseANDExpression_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context, 
	struct SEE_value *res);
static void BitwiseANDExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void BitwiseXORExpression_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context, 
	struct SEE_value *res);
static void BitwiseXORExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void BitwiseORExpression_common(struct SEE_value *r2, 
        struct SEE_value *r4, struct SEE_context *context,
	struct SEE_value *res);
static void BitwiseORExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void LogicalANDExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void LogicalORExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ConditionalExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_simple_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_muleq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_diveq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_modeq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_addeq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_subeq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_lshifteq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_rshifteq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_urshifteq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_andeq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_xoreq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void AssignmentExpression_oreq_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void Expression_comma_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void Block_empty_eval(struct node *n, struct SEE_context *context, 
        struct SEE_value *res);
static void StatementList_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void VariableStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void VariableDeclarationList_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void VariableDeclaration_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void EmptyStatement_eval(struct node *n, struct SEE_context *context, 
        struct SEE_value *res);
static void ExpressionStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void IfStatement_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void IterationStatement_dowhile_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void IterationStatement_while_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void IterationStatement_for_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void IterationStatement_forvar_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void IterationStatement_forin_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void IterationStatement_forvarin_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ContinueStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void BreakStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ReturnStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ReturnStatement_undef_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void WithStatement_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void SwitchStatement_caseblock(struct SwitchStatement_node *n, 
        struct SEE_context *context, struct SEE_value *input, 
        struct SEE_value *res);
static void SwitchStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void LabelledStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void ThrowStatement_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static int TryStatement_catch(struct TryStatement_node *n, 
        struct SEE_context *context, struct SEE_value *C, 
        struct SEE_value *res, SEE_try_context_t *ctxt);
static void TryStatement_catch_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void TryStatement_finally_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void TryStatement_catchfinally_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void FunctionExpression_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void FunctionBody_eval(struct node *na, struct SEE_context *context, 
        struct SEE_value *res);
static void SourceElements_eval(struct node *na, 
        struct SEE_context *context, struct SEE_value *res);
static void FunctionDeclaration_fproc(struct node *na, 
        struct SEE_context *context);
static void SourceElements_fproc(struct node *na, 
        struct SEE_context *context);
static void EqualityExpression_seq(struct SEE_context *context, 
        struct SEE_value *x, struct SEE_value *y, struct SEE_value *res);
#endif

static struct SEE_string *error_at(struct parser *parser, const char *fmt, 
        ...);
static int Always_isconst(struct node *na, struct SEE_interpreter *interp);
static void EqualityExpression_eq(struct SEE_interpreter *interp, 
        struct SEE_value *x, struct SEE_value *y, struct SEE_value *res);
static struct node *Literal_parse(struct parser *parser);
static struct node *NumericLiteral_parse(struct parser *parser);
static struct node *StringLiteral_parse(struct parser *parser);
static struct node *RegularExpressionLiteral_parse(struct parser *parser);
static struct node *PrimaryExpression_parse(struct parser *parser);
static struct node *ArrayLiteral_parse(struct parser *parser);
static struct node *ObjectLiteral_parse(struct parser *parser);
#if WITH_PARSER_CODEGEN
static void Arguments_codegen(struct node *na, struct code_context *cc);
#endif
static int Arguments_isconst(struct node *na, 
        struct SEE_interpreter *interp);
static struct Arguments_node *Arguments_parse(struct parser *parser);
static struct node *MemberExpression_parse(struct parser *parser);
static struct node *LeftHandSideExpression_parse(struct parser *parser);
static int Unary_isconst(struct node *na, struct SEE_interpreter *interp);
static struct node *PostfixExpression_parse(struct parser *parser);
static struct node *UnaryExpression_parse(struct parser *parser);
static int Binary_isconst(struct node *na, struct SEE_interpreter *interp);
static struct node *MultiplicativeExpression_parse(struct parser *parser);
static struct node *AdditiveExpression_parse(struct parser *parser);
static struct node *ShiftExpression_parse(struct parser *parser);
static struct node *RelationalExpression_parse(struct parser *parser);
static struct node *EqualityExpression_parse(struct parser *parser);
static struct node *BitwiseANDExpression_parse(struct parser *parser);
static struct node *BitwiseXORExpression_parse(struct parser *parser);
static struct node *BitwiseORExpression_parse(struct parser *parser);
static int LogicalANDExpression_isconst(struct node *na, 
        struct SEE_interpreter *interp);
static struct node *LogicalANDExpression_parse(struct parser *parser);
static int LogicalORExpression_isconst(struct node *na, 
        struct SEE_interpreter *interp);
static struct node *LogicalORExpression_parse(struct parser *parser);
static int ConditionalExpression_isconst(struct node *na, 
        struct SEE_interpreter *interp);
static struct node *ConditionalExpression_parse(struct parser *parser);
static struct node *AssignmentExpression_parse(struct parser *parser);
static struct node *Expression_parse(struct parser *parser);
static struct node *Statement_parse(struct parser *parser);
static struct node *Block_parse(struct parser *parser);
static struct node *StatementList_parse(struct parser *parser);
static struct node *VariableStatement_parse(struct parser *parser);
static struct node *VariableDeclarationList_parse(struct parser *parser);
static struct node *VariableDeclaration_parse(struct parser *parser);
static struct node *EmptyStatement_parse(struct parser *parser);
static struct node *ExpressionStatement_parse(struct parser *parser);
static struct node *ExpressionStatement_make(struct SEE_interpreter *,
	struct node *);
static struct node *IfStatement_parse(struct parser *parser);
static struct node *IterationStatement_parse(struct parser *parser);
static struct node *ContinueStatement_parse(struct parser *parser);
static struct node *BreakStatement_parse(struct parser *parser);
static struct node *ReturnStatement_parse(struct parser *parser);
static struct node *WithStatement_parse(struct parser *parser);
static struct node *SwitchStatement_parse(struct parser *parser);
static struct node *LabelledStatement_parse(struct parser *parser);
static struct node *ThrowStatement_parse(struct parser *parser);
static struct node *TryStatement_parse(struct parser *parser);
static struct node *FunctionDeclaration_parse(struct parser *parser);
static struct node *FunctionExpression_parse(struct parser *parser);
static struct var *FormalParameterList_parse(struct parser *parser);
static struct node *FunctionBody_parse(struct parser *parser);
static struct node *FunctionBody_make(struct SEE_interpreter *, 
	struct node *, int);
static struct node *FunctionStatement_parse(struct parser *parser);
static struct function *Program_parse(struct parser *parser);
static struct node *SourceElements_make1(struct SEE_interpreter *,
	struct node *);
static struct node *SourceElements_parse(struct parser *parser);
static int FunctionBody_isempty(struct SEE_interpreter *interp, 
        struct node *body);
static void eval(struct SEE_context *context, struct SEE_object *thisobj, 
        int argc, struct SEE_value **argv, struct SEE_value *res);
static void eval_functionbody(void *, struct SEE_context *, struct SEE_value *);



#if WITH_PARSER_CODEGEN
static void push_patchables(struct code_context *cc, unsigned int target, 
	int cont);
static void pop_patchables(struct code_context *cc, 
	SEE_code_addr_t cont_addr, SEE_code_addr_t break_addr);
static struct patchables *patch_find(struct code_context *cc, 
	unsigned int target, int tok);
static void patch_add_continue(struct code_context *cc, struct patchables *p,
	SEE_code_patchable_t pa);
static void patch_add_break(struct code_context *cc, struct patchables *p,
	SEE_code_patchable_t pa);
#endif

#if WITH_PARSER_CODEGEN
static void cg_init(struct SEE_interpreter *, struct code_context *, int);
static void cg_const_codegen(struct node *node, struct code_context *cc);
static struct SEE_code *cg_fini(struct SEE_interpreter *interp,
	struct code_context *cc, unsigned int maxstack);
static void cg_block_enter(struct code_context *cc);
static void cg_block_leave(struct code_context *cc);
static unsigned int cg_block_current(struct code_context *cc);
static unsigned int cg_var_id(struct code_context *, struct SEE_string *);
static int cg_var_is_in_scope(struct code_context *, struct SEE_string *);
static void cg_var_set_scope(struct code_context *, struct SEE_string *, int);
static int cg_var_set_all_scope(struct code_context *, int);
#endif

static void *make_body(struct SEE_interpreter *, struct node *, int);
static void const_evaluate(struct node *, struct SEE_interpreter *,
	struct SEE_value *);

#define CONTINUABLE 1
#define NO_CONST    1

/*------------------------------------------------------------
 * macros
 */

/*
 * Macros for accessing the tokeniser, with lookahead
 */
#define NEXT 						\
	(parser->unget != parser->unget_end		\
		? parser->unget_tok[parser->unget] 	\
		: parser->lex->next)
#define NEXT_VALUE					\
	(parser->unget != parser->unget_end		\
		? &parser->unget_val[parser->unget] 	\
		: &parser->lex->value)

#define NEXT_LINENO					\
	(parser->unget != parser->unget_end		\
		? parser->unget_lin[parser->unget] 	\
		: parser->lex->next_lineno)

#define NEXT_FILENAME					\
		  parser->lex->next_filename

#define NEXT_FOLLOWS_NL					\
	(parser->unget != parser->unget_end		\
		? parser->unget_fnl[parser->unget] 	\
		: parser->lex->next_follows_nl)

#define SKIP						\
    do {						\
	if (parser->unget == parser->unget_end)		\
		SEE_lex_next(parser->lex);		\
	else 						\
		parser->unget = 			\
		    (parser->unget + 1) % UNGET_MAX;	\
	SKIP_DEBUG					\
    } while (0)

#ifndef NDEBUG
#  define SKIP_DEBUG					\
    if (SEE_parse_debug)				\
      dprintf("SKIP: next = %s\n", SEE_tokenname(NEXT));
#else
#  define SKIP_DEBUG
#endif

/* Handy macros for describing syntax errors */
#define EXPECT(c) EXPECTX(c, SEE_tokenname(c))
#define EXPECTX(c, tokstr)				\
    do { 						\
	EXPECTX_NOSKIP(c, tokstr);			\
	SKIP;						\
    } while (0)
#define EXPECT_NOSKIP(c) EXPECTX_NOSKIP(c, SEE_tokenname(c))
#define EXPECTX_NOSKIP(c, tokstr)			\
    do { 						\
	if (NEXT != (c)) 				\
	    EXPECTED(tokstr);				\
    } while (0)
#define EXPECTED(tokstr)				\
    do { 						\
	    char nexttok[30];				\
	    SEE_tokenname_buf(NEXT, nexttok, 		\
		sizeof nexttok);			\
	    SEE_error_throw_string(			\
		parser->interpreter,			\
		parser->interpreter->SyntaxError,	\
		error_at(parser, 			\
		         "expected %s but got %s",	\
		         tokstr,			\
		         nexttok));			\
    } while (0)

#define EMPTY_LABEL		((struct SEE_string *)NULL)
#define NO_TARGET		0

/* 
 * Automatic semicolon insertion macros.
 *
 * Using these instead of NEXT/SKIP allows synthesis of
 * semicolons where they are permitted by the standard.
 */
#define NEXT_IS_SEMICOLON				\
	(NEXT == ';' || NEXT == '}' || NEXT_FOLLOWS_NL)
#define EXPECT_SEMICOLON				\
    do {						\
	if (NEXT == ';')				\
		SKIP;					\
	else if ((NEXT == '}' || NEXT_FOLLOWS_NL)) {	\
		/* automatic semicolon insertion */	\
	} else						\
		EXPECTX(';', "';', '}' or newline");	\
    } while (0)

/* Traces a statement-level event, or an eval() */
#define TRACE(loc, ctxt, event)				\
    do {						\
	if (ctxt) {					\
            if (SEE_system.periodic)			\
	    	(*SEE_system.periodic)((ctxt)->interpreter); \
	    (ctxt)->interpreter->try_location =	loc;	\
	    trace_event(ctxt, event);			\
	}						\
    } while (0)

/*
 * Macros for accessing the abstract syntax tree
 */

#ifndef NDEBUG

# if !HAVE___FUNCTION__
   /* Some trickery to stringize the __LINE__ macro */
#  define X_STR2(s) #s
#  define X_STR(s) X_STR2(s)
#  define __FUNCTION__   __FILE__ ":" X_STR(__LINE__)
# endif

# define EVAL_DEBUG_ENTER(node)				\
	if (SEE_eval_debug) 				\
	    dprintf("eval: %s enter %p\n", 		\
		__FUNCTION__, node);

# define EVAL_DEBUG_LEAVE(node, ctxt, res)		\
	if (SEE_eval_debug && (ctxt)) {			\
	    dprintf("eval: %s leave %p -> %p = ", 	\
		__FUNCTION__, node, (void *)(res));	\
	    dprintv((ctxt)->interpreter, res);		\
	    dprintf("\n");				\
	}

#else /* NDEBUG */

# define EVAL_DEBUG_ENTER(node)
# define EVAL_DEBUG_LEAVE(node, ctxt, res)

#endif /* NDEBUG */

#define EVALFN(node) _SEE_nodeclass_eval[(node)->nodeclass]
# define EVAL(node, ctxt, res)				\
    do {						\
	struct SEE_throw_location * _loc_save = NULL;	\
	EVAL_DEBUG_ENTER(node)				\
	if (ctxt) {					\
	  _loc_save = (ctxt)->interpreter->try_location;\
	  (ctxt)->interpreter->try_location =		\
		&(node)->location;			\
	}						\
	(*EVALFN(node))(node, ctxt, res);	\
	EVAL_DEBUG_LEAVE(node, ctxt, res)		\
    } while (0)
  /*
   * Note: there is no need to restore the _loc_save in
   * a try-finally block
   */
/* There are only TWO fprocs used in ECMAScript node classes, so
 * don't waste space. */
#define FPROC(node, ctxt)				\
    do {						\
	if ((node)->nodeclass == NODECLASS_FunctionDeclaration) \
            FunctionDeclaration_fproc(node, ctxt);      \
	else if ((node)->nodeclass == NODECLASS_SourceElements) \
            SourceElements_fproc(node, ctxt);      \
    } while (0)

#ifndef NDEBUG
#define NEW_NODE(t, nc)					\
	((t *)new_node(parser, sizeof (t), nc, #nc))
#define NEW_NODE_INTERNAL(i, t, nc)			\
	((t *)new_node_internal(i, sizeof (t), nc, STR(empty_string), 0, #nc))
#else
#define NEW_NODE(t, nc)					\
	((t *)new_node(parser, sizeof (t), nc, NULL))
#define NEW_NODE_INTERNAL(i, t, nc)			\
	((t *)new_node_internal(i, sizeof (t), nc, STR(empty_string), 0, NULL))
#endif

#ifndef NDEBUG
#define PARSE(prod)					\
    ((void)(SEE_parse_debug ? 				\
	dprintf("parse %s next=%s\n", #prod,		\
	    SEE_tokenname(NEXT)) : (void)0),		\
        prod##_parse(parser))
#else
#define PARSE(prod)					\
        prod##_parse(parser)
#endif

/* Generates a generic parse error */
#define ERROR						\
	SEE_error_throw_string(				\
	    parser->interpreter,			\
	    parser->interpreter->SyntaxError,		\
	    error_at(parser, "parse error before %s",	\
	    SEE_tokenname(NEXT)))

/* Generates a specific parse error */
#define ERRORm(m)					\
	SEE_error_throw_string(				\
	    parser->interpreter,			\
	    parser->interpreter->SyntaxError,		\
	    error_at(parser, "%s, near %s",		\
	    m, SEE_tokenname(NEXT)))



/* Returns true if the node returns a constant expression */
#define ISCONSTFN(n)    _SEE_nodeclass_isconst[(n)->nodeclass]
#define ISCONST(n, interp) 				\
        isconst(n, interp)

static int isconst(struct node *n, struct SEE_interpreter *interp) {
    int flags = n->flags;
    int isconst = 0;

    if (flags & NODE_FLAG_ISCONST_VALID) {
        isconst = flags & NODE_FLAG_ISCONST;
    } else {
        isconst = ISCONSTFN(n) ? (*ISCONSTFN(n))(n, interp) : 0;
        if (isconst)
            flags |= NODE_FLAG_ISCONST;
        n->flags = flags | NODE_FLAG_ISCONST_VALID;
    }
    return isconst;
}

/* Codegen macros */

#if WITH_PARSER_CODEGEN
# define CODEGENFN(node) _SEE_nodeclass_codegen[(node)->nodeclass]
# define CODEGEN(node)	do {				\
	if (!(cc)->no_const &&				\
	    ISCONST(node, (cc)->code->interpreter) &&	\
	    node->nodeclass != NODECLASS_Literal)	\
		cg_const_codegen(node, cc);		\
	else						\
	    (*CODEGENFN(node))(node, cc);	        \
    } while (0)

/* Call/construct operators */
# define _CG_OP1(name, n) \
    (*cc->code->code_class->gen_op1)(cc->code, SEE_CODE_##name, n)
# define CG_NEW(n)		_CG_OP1(NEW, n)
# define CG_CALL(n)		_CG_OP1(CALL, n)
# define CG_END(n)		_CG_OP1(END, n)
# define CG_VREF(n)		_CG_OP1(VREF, n)

/* Generic operators */
# define _CG_OP0(name) \
    (*cc->code->code_class->gen_op0)(cc->code, SEE_CODE_##name)
# define CG_NOP()		_CG_OP0(NOP)
# define CG_DUP()		_CG_OP0(DUP)
# define CG_POP()		_CG_OP0(POP)
# define CG_EXCH()		_CG_OP0(EXCH)
# define CG_ROLL3()		_CG_OP0(ROLL3)
# define CG_THROW()		_CG_OP0(THROW)
# define CG_SETC()		_CG_OP0(SETC)
# define CG_GETC()		_CG_OP0(GETC)
# define CG_THIS()		_CG_OP0(THIS)
# define CG_OBJECT()		_CG_OP0(OBJECT)
# define CG_ARRAY()		_CG_OP0(ARRAY)
# define CG_REGEXP()		_CG_OP0(REGEXP)
# define CG_REF()		_CG_OP0(REF)
# define CG_GETVALUE()		_CG_OP0(GETVALUE)
# define CG_LOOKUP()		_CG_OP0(LOOKUP)
# define CG_PUTVALUE()		_CG_OP0(PUTVALUE)
# define CG_DELETE()		_CG_OP0(DELETE)
# define CG_TYPEOF()		_CG_OP0(TYPEOF)
# define CG_TOOBJECT()		_CG_OP0(TOOBJECT)
# define CG_TONUMBER()		_CG_OP0(TONUMBER)
# define CG_TOBOOLEAN()		_CG_OP0(TOBOOLEAN)
# define CG_TOSTRING()		_CG_OP0(TOSTRING)
# define CG_TOPRIMITIVE()	_CG_OP0(TOPRIMITIVE)
# define CG_NEG()		_CG_OP0(NEG)
# define CG_INV()		_CG_OP0(INV)
# define CG_NOT()		_CG_OP0(NOT)
# define CG_MUL()		_CG_OP0(MUL)
# define CG_DIV()		_CG_OP0(DIV)
# define CG_MOD()		_CG_OP0(MOD)
# define CG_ADD()		_CG_OP0(ADD)
# define CG_SUB()		_CG_OP0(SUB)
# define CG_LSHIFT()		_CG_OP0(LSHIFT)
# define CG_RSHIFT()		_CG_OP0(RSHIFT)
# define CG_URSHIFT()		_CG_OP0(URSHIFT)
# define CG_LT()		_CG_OP0(LT)
# define CG_GT()		_CG_OP0(GT)
# define CG_LE()		_CG_OP0(LE)
# define CG_GE()		_CG_OP0(GE)
# define CG_INSTANCEOF()	_CG_OP0(INSTANCEOF)
# define CG_IN()		_CG_OP0(IN)
# define CG_EQ()		_CG_OP0(EQ)
# define CG_SEQ()		_CG_OP0(SEQ)
# define CG_BAND()		_CG_OP0(BAND)
# define CG_BXOR()		_CG_OP0(BXOR)
# define CG_BOR()		_CG_OP0(BOR)
# define CG_S_ENUM()		_CG_OP0(S_ENUM)
# define CG_S_WITH()		_CG_OP0(S_WITH)

/* Special PUTVALUE that takes attributes */
# define CG_PUTVALUEA(attr)	_CG_OP1(PUTVALUEA,attr)

/* Literals */
# define CG_LITERAL(vp) \
	(*cc->code->code_class->gen_literal)(cc->code, vp)

# define CG_UNDEFINED() do {		/* - | num */	\
	struct SEE_value _cgtmp;			\
	SEE_SET_UNDEFINED(&_cgtmp);			\
	CG_LITERAL(&_cgtmp);				\
  } while (0)

# define CG_STRING(str) do {		/* - | str */	\
	struct SEE_value _cgtmp;			\
	SEE_SET_STRING(&_cgtmp, str);\
	CG_LITERAL(&_cgtmp);				\
  } while (0)

# define CG_NUMBER(num) do {		/* - | num */	\
	struct SEE_value _cgtmp;			\
	SEE_SET_NUMBER(&_cgtmp, num);			\
	CG_LITERAL(&_cgtmp);				\
  } while (0)

# define CG_BOOLEAN(bool) do {		/* - | bool */	\
	struct SEE_value _cgtmp;			\
	SEE_SET_BOOLEAN(&_cgtmp, bool);			\
	CG_LITERAL(&_cgtmp);				\
  } while (0)
# define CG_TRUE()   CG_BOOLEAN(1)	/* - | true */
# define CG_FALSE()  CG_BOOLEAN(0)	/* - | false */

/* Function instance */
# define CG_FUNC(fn) \
	(*cc->code->code_class->gen_func)(cc->code, fn)	/* - | obj */

/* Record source location */
# define CG_LOC(loc) \
	(*cc->code->code_class->gen_loc)(cc->code, loc)

/* Branching and patching */
# define CG_HERE()						\
	(*cc->code->code_class->here)(cc->code)

/* Patch a previously saved address to point to CG_HERE() */
# define CG_LABEL(var)						\
	(*cc->code->code_class->patch)(cc->code, var, CG_HERE())

# define _CG_OPA(name, patchp, addr)				\
    (*cc->code->code_class->gen_opa)(cc->code, SEE_CODE_##name, patchp, addr)

/* Backward (_b) and forward (_f) branching */
# define CG_B_ALWAYS_b(addr)		_CG_OPA(B_ALWAYS, 0, addr)
# define CG_B_TRUE_b(addr)		_CG_OPA(B_TRUE, 0, addr)
# define CG_B_ENUM_b(addr)		_CG_OPA(B_ENUM, 0, addr)
# define CG_S_TRYC_b(addr)		_CG_OPA(S_TRYC, 0, addr)
# define CG_S_TRYF_b(addr)		_CG_OPA(S_TRYF, 0, addr)

# define CG_B_ALWAYS_f(var)		_CG_OPA(B_ALWAYS, &(var), 0)
# define CG_B_TRUE_f(var)		_CG_OPA(B_TRUE, &(var), 0)
# define CG_B_ENUM_f(var)		_CG_OPA(B_ENUM, &(var), 0)
# define CG_S_TRYC_f(var)		_CG_OPA(S_TRYC, &(var), 0)
# define CG_S_TRYF_f(var)		_CG_OPA(S_TRYF, &(var), 0)

/* Execute program code */
# define CG_EXEC(co, ctxt, res)		(*(co)->code_class->exec)(co, ctxt, res)

#endif /* WITH_PARSER_CODEGEN */

/*------------------------------------------------------------
 * Allocators and initialisers
 */

/*
 * Creates a new AST node, initialising it with the 
 * given node class nc, and recording the current
 * line number as reported by the parser.
 */
static struct node *
new_node_internal(interp, sz, nc, filename, lineno, dbg_nc)
	struct SEE_interpreter *interp;
	int sz;
	enum nodeclass_enum nc;
	struct SEE_string *filename;
	int lineno;
	const char *dbg_nc;
{
	struct node *n;

	n = (struct node *)SEE_malloc(interp, sz);
	n->nodeclass = nc;
	n->location.filename = filename;
	n->location.lineno = lineno;
	n->flags = 0;
#if WITH_PARSER_CODEGEN
	n->is = 0;
	n->maxstack = 0;
#endif

	return n;
}

static struct node *
new_node(parser, sz, nc, dbg_nc)
	struct parser *parser;
	int sz;
	enum nodeclass_enum nc;
	const char *dbg_nc;
{
	struct node *n;

	n = new_node_internal(parser->interpreter, sz, nc, 
	    NEXT_FILENAME, NEXT_LINENO, dbg_nc);
#ifndef NDEBUG
	if (SEE_parse_debug) 
		dprintf("parse: %p %s (next=%s)\n", 
			n, dbg_nc, SEE_tokenname(NEXT));
#endif
	return n;
}

/*
 * Initialises a parser state.
 */
static void
parser_init(parser, interp, lex)
	struct parser *parser;
	struct SEE_interpreter *interp;
	struct lex *lex;
{
	parser->interpreter = interp;
	parser->lex = lex;
	parser->unget = 0;
	parser->unget_end = 0;
	parser->noin = 0;
	parser->is_lhs = 0;
	parser->funcdepth = 0;
	parser->vars = NULL;
	parser->labelsets = NULL;
	parser->labels = NULL;
	parser->current_labelset = NULL;
}

/*------------------------------------------------------------
 * Labels
 */

/* Returns a labelset for the current statement, creating one if needed. */
static struct labelset *
labelset_current(parser)
	struct parser *parser;
{
	struct labelset *ls;

	if (!parser->current_labelset) {
	    ls = SEE_NEW(parser->interpreter, struct labelset);
	    if (parser->labelsets)
		ls->target = parser->labelsets->target + 1;
	    else
		ls->target = 1;
	    ls->next = parser->labelsets;
	    parser->labelsets = ls;
	    parser->current_labelset = ls;
#ifndef NDEBUG
	    if (SEE_parse_debug)
		dprintf("labelset_current(): new %p\n", 
		    parser->current_labelset);
#endif
	}
	return parser->current_labelset;
}

/*
 * Pushes a new label for the current labelset onto the label scope stack.
 * Checks for duplicate labels, which are not allowed, except for EMPTY_LABEL.
 */
static void
label_enter(parser, name)
	struct parser *parser;
	struct SEE_string *name;
{
	struct label *l;
	struct SEE_string *msg;
	struct SEE_throw_location location;

	location.lineno = NEXT_LINENO;
	location.filename = NEXT_FILENAME;

#ifndef NDEBUG
	if (SEE_parse_debug) {
	    dprintf("label_enter() [");
	    if (name == EMPTY_LABEL)
		dprintf("EMPTY_LABEL");
	    else
		dprints(name);
	    dprintf("]\n");
	}
#endif

	if (name != EMPTY_LABEL)
	    for (l = parser->labels; l; l = l->next)
		if (l->name == name) {
		    msg = SEE_location_string(parser->interpreter, &location);
		    SEE_string_append(msg, STR(duplicate_label));
		    SEE_string_append(msg, name);
		    SEE_string_addch(msg, '\'');
		    SEE_string_addch(msg, ';');
		    SEE_string_addch(msg, ' ');
		    SEE_string_append(msg, 
			SEE_location_string(parser->interpreter, 
			    &l->location));
		    SEE_string_append(msg, STR(previous_definition));
		    SEE_error_throw_string(parser->interpreter,
			    parser->interpreter->SyntaxError, msg);
		}


	l = SEE_NEW(parser->interpreter, struct label);
	l->name = name;
	l->labelset = labelset_current(parser);
	l->location.lineno = location.lineno;
	l->location.filename = location.filename;
	/* Push onto parser->labels */
	l->next = parser->labels;
	parser->labels = l;
}

/* Pops the last label pushed by label_enter(). */
static void
label_leave(parser)
	struct parser *parser;
{

	SEE_ASSERT(parser->interpreter, parser->labels != NULL);
#ifndef NDEBUG
	if (SEE_parse_debug) {
	    dprintf("label_leave() [");
	    if (parser->labels->name == EMPTY_LABEL)
		dprintf("EMPTY_LABEL");
	    else
		dprints(parser->labels->name);
	    dprintf("]\n");
	}
#endif
	parser->labels = parser->labels->next;
}

/*
 * Returns the target ID correspnding to the label, or raises a SyntaxError
 * if it isn't found.
 * Kind is a token indicating the kind of statement using the label 
 * (tBREAK or tCONTINUE), and a SyntaxError is thrown if the found labelset
 * is incompatible.
 * Always returns a valid target ID.
 */
static unsigned int
target_lookup(parser, label_name, kind)
	struct parser *parser;
	struct SEE_string *label_name;
	int kind;
{
	struct SEE_string *msg;
	struct label *l;

	SEE_ASSERT(parser->interpreter, kind == tBREAK || kind == tCONTINUE);

#ifndef NDEBUG
	if (SEE_parse_debug) {
	    dprintf("labelset_lookup_target: searching for '");
	    if (label_name == EMPTY_LABEL)
	        dprintf("EMPTY_LABEL");
	    else
	    	dprints(label_name);
	    dprintf("\n");
	}
#endif

	for (l = parser->labels; l; l = l->next)
	    if (l->name == label_name) {
	        if (kind == tCONTINUE && !l->labelset->continuable) {
		    if (label_name == EMPTY_LABEL)
		        continue;
		    msg = error_at(parser, "label '");
		    SEE_string_append(msg, label_name);
		    SEE_string_append(msg, 
			SEE_string_sprintf(parser->interpreter,
			"' not suitable for continue"));
		    SEE_error_throw_string(parser->interpreter,
			    parser->interpreter->SyntaxError, msg);
		}
		return l->labelset->target;
	    }

	if (label_name) {
	    msg = error_at(parser, "label '");
	    SEE_string_append(msg, label_name);
	    SEE_string_append(msg, 
		SEE_string_sprintf(parser->interpreter,
		"' not defined, or not reachable"));
	} else if (kind == tCONTINUE)
	    msg = error_at(parser,
		"continue statement not within a loop");
	else /* kind == tBREAK */
	    msg = error_at(parser,
		"break statement not within loop or switch");

	SEE_error_throw_string(parser->interpreter,
		parser->interpreter->SyntaxError, msg);
	/* NOTREACHED */
}

#if WITH_PARSER_CODEGEN
/* Creates a new patchables for breaking/continuing */
static void
push_patchables(cc, target, continuable)
	struct code_context *cc;
	unsigned int target;
	int continuable;
{
	struct patchables *p;
	struct SEE_interpreter *interp = cc->code->interpreter;

	/* Initialise two empty lists of patchable locations */
	p = SEE_NEW(interp, struct patchables);
	SEE_GROW_INIT(interp, &p->gcont_patch, p->cont_patch,
	    p->ncont_patch);
	SEE_GROW_INIT(interp, &p->gbreak_patch, p->break_patch, 
	    p->nbreak_patch);
	p->target = target;
	p->continuable = continuable;
	p->block_depth = cc->block_depth;
	p->prev = cc->patchables;
	cc->patchables = p;
}

/* Pops a patchables, performing the previously pending patches */
static void
pop_patchables(cc, cont_addr, break_addr)
	struct code_context *cc;
	SEE_code_addr_t cont_addr;
	SEE_code_addr_t break_addr;
{
	struct patchables *p = cc->patchables;
	unsigned int i;

	/* Patch the continue locations with the break addresses */
	for (i = 0; i < p->ncont_patch; i++) {
#ifndef NDEBUG
	    if (SEE_parse_debug)
		dprintf("patching continue to 0x%x at 0x%x\n", 
		    cont_addr, p->cont_patch[i]);
#endif
	    (*cc->code->code_class->patch)(cc->code, p->cont_patch[i], 
		cont_addr);
	}

	/* Patch the break locations with the break address */
	for (i = 0; i < p->nbreak_patch; i++) {
#ifndef NDEBUG
	    if (SEE_parse_debug)
		dprintf("patching break to 0x%x at 0x%x\n", 
		    break_addr, p->break_patch[i]);
#endif
	    (*cc->code->code_class->patch)(cc->code, p->break_patch[i], 
		break_addr);
	}

	cc->patchables = p->prev;
}

/* Return the right patchables when breaking/continuing */
static struct patchables *
patch_find(cc, target, tok)
	struct code_context *cc;
	unsigned int target;
	int tok;    /* tBREAK or tCONTINUE */
{
	struct patchables *p;

	if (target == NO_TARGET && tok == tCONTINUE) {
	    for (p = cc->patchables; p; p = p->prev)
		if (p->continuable)
		    return p;
	} else if (target == NO_TARGET)
	    return cc->patchables;
	else
	    for (p = cc->patchables; p; p = p->prev)
		if (p->target == target)
		    return p;
	SEE_ASSERT(cc->code->interpreter, !"lost patchable");
	/* UNREACHABLE */
	return NULL; 
}

/* Add a pending continue patch */
static void
patch_add_continue(cc, p, pa)
	struct code_context *cc;
	struct patchables *p;
	SEE_code_patchable_t pa;
{
	struct SEE_interpreter *interp = cc->code->interpreter;
	unsigned int n = p->ncont_patch;

	SEE_GROW_TO(interp, &p->gcont_patch, n + 1);
	p->cont_patch[n] = pa;
}

/* Add a pending break patch */
static void
patch_add_break(cc, p, pa)
	struct code_context *cc;
	struct patchables *p;
	SEE_code_patchable_t pa;
{
	struct SEE_interpreter *interp = cc->code->interpreter;
	unsigned int n = p->nbreak_patch;

	SEE_GROW_TO(interp, &p->gbreak_patch, n + 1);
	p->break_patch[n] = pa;
}
#endif

/*------------------------------------------------------------
 * Code generator helper functions
 */

#if WITH_PARSER_CODEGEN
static void
cg_init(interp, cc, no_const)
	struct SEE_interpreter *interp;
	struct code_context *cc;
	int no_const;
{
	cc->code = (*SEE_system.code_alloc)(interp);
	cc->patchables = NULL;
	cc->block_depth = 0;
	cc->max_block_depth = 0;
	cc->in_var_scope = 1;
	cc->no_const = no_const;
	SEE_GROW_INIT(interp, &cc->gvarscope, cc->varscope, cc->nvarscope);
}

static struct SEE_code *
cg_fini(interp, cc, maxstack)
	struct SEE_interpreter *interp;
	struct code_context *cc;
	unsigned int maxstack;
{
	struct SEE_code *co = cc->code;

	SEE_ASSERT(interp, cc->block_depth == 0);
	SEE_ASSERT(interp, cc->in_var_scope);
	(*co->code_class->maxstack)(co, maxstack);
	(*co->code_class->maxblock)(co, cc->max_block_depth);
	(*co->code_class->close)(co);
	cc->code = NULL;
	return co;
}

/*
 * Evaluates a (constant) expression node, and then generates a LITERAL
 * instruction.
 */
static void
cg_const_codegen(node, cc)
	struct node *node;
	struct code_context *cc;
{
	struct SEE_value value;

	const_evaluate(node, cc->code->interpreter, &value);
	CG_LITERAL(&value);
	switch (SEE_VALUE_GET_TYPE(&value)) {
	case SEE_UNDEFINED: node->is = CG_TYPE_UNDEFINED; break;
	case SEE_NULL:	    node->is = CG_TYPE_NULL;	  break;
	case SEE_BOOLEAN:   node->is = CG_TYPE_BOOLEAN;   break;
	case SEE_NUMBER:    node->is = CG_TYPE_NUMBER;    break;
	case SEE_STRING:    node->is = CG_TYPE_STRING;    break;
	case SEE_OBJECT:    node->is = CG_TYPE_OBJECT;    break;
	case SEE_REFERENCE: node->is = CG_TYPE_REFERENCE; break;
	default:	    node->is = 0;
	}
	node->maxstack = 1;
}

/* Called when entering a block. Increments the block depth */
static void
cg_block_enter(cc)
	struct code_context *cc;
{
	cc->block_depth++;
	if (cc->block_depth > cc->max_block_depth)
	    cc->max_block_depth = cc->block_depth;
}

/* Called when leaving a block. Restores the block depth */
static void
cg_block_leave(cc)
	struct code_context *cc;
{
	cc->block_depth--;
}

/* Returns the current block depth, suitable for CG_END() */
static unsigned int
cg_block_current(cc)
	struct code_context *cc;
{
	return cc->block_depth;
}

/* Returns the VREF ID of a identifier in the immediate variable scope */
static unsigned int
cg_var_id(cc, ident)
	struct code_context *cc;
	struct SEE_string *ident;
{
	unsigned int i;

	for (i = 0; i < cc->nvarscope; i++)
	    if (cc->varscope[i].ident == ident) {
#ifndef NDEBUG
		if (SEE_parse_debug) {
		    dprintf("cg_var_id(");
		    dprints(ident);
		    dprintf(") = %u\n", cc->varscope[i].id);
		}
#endif
		return cc->varscope[i].id;
	    }
	SEE_ASSERT(cc->code->interpreter, !"bad cg var identifier");
	return ~0; /* unreachable */
}

/* Returns true if the identifier is a variable in the immediate scope */
static int
cg_var_is_in_scope(cc, ident)
	struct code_context *cc;
	struct SEE_string *ident;
{
	unsigned int i;

	/* If in a 'with' block, then nothing is certain */
	if (cc->in_var_scope)
	    for (i = 0; i < cc->nvarscope; i++)
		if (cc->varscope[i].ident == ident) {
#ifndef NDEBUG
		    if (SEE_parse_debug) {
			dprintf("cg_var_is_in_scope(");
			dprints(ident);
			dprintf("): found, in_scope=%d\n",
			    cc->varscope[i].in_scope);
		    }
#endif		    
		    return cc->varscope[i].in_scope;
		}
#ifndef NDEBUG
	if (SEE_parse_debug) {
	    dprintf("cg_var_is_in_scope(");
	    dprints(ident);
	    dprintf("): not found\n");
	}
#endif		    
	return 0;
}

/* Sets the scope of a variable identifier */
static void
cg_var_set_scope(cc, ident, in_scope)
	struct code_context *cc;
	struct SEE_string *ident;
	int in_scope;
{
	unsigned int i;

	for (i = 0; i < cc->nvarscope; i++)
	    if (cc->varscope[i].ident == ident) {
#ifndef NDEBUG
		if (SEE_parse_debug) {
		    dprintf("cg_var_set_scope(");
		    dprints(ident);
		    dprintf(", %d): previously %d\n",
			in_scope, cc->varscope[i].in_scope);
		}
#endif		    
		cc->varscope[i].in_scope = in_scope;
		return;
	    }
	if (in_scope) {
	    SEE_GROW_TO(cc->code->interpreter, &cc->gvarscope,
			cc->nvarscope + 1);
	    cc->varscope[i].ident = ident;
	    cc->varscope[i].id = 
		(*cc->code->code_class->gen_var)(cc->code, ident);
	    cc->varscope[i].in_scope = 1;
#ifndef NDEBUG
	    if (SEE_parse_debug) {
		dprintf("cg_var_set_scope(");
		dprints(ident);
		dprintf(", %d): NEW (id %u)\n", in_scope, cc->varscope[i].id);
	    }
#endif
	}
}

/* Temporarily sets the scope visibility of all var idents. Returns old value.
 * This is used when entering a 'with' scope */
static int
cg_var_set_all_scope(cc, in_scope)
	struct code_context *cc;
	int in_scope;
{
	int old_scope = cc->in_var_scope;
	cc->in_var_scope = in_scope;
#ifndef NDEBUG
	if (SEE_parse_debug)
	    dprintf("cg_var_set_all_scope(%d) -> %d\n", in_scope, old_scope);
#endif
	return old_scope;
}
#endif /* WITH_PARSER_CODEGEN */

/* Returns a body suitable for use by eval_functionbody() */
static void *
make_body(interp, node, no_const)
	struct SEE_interpreter *interp;
	struct node *node;
	int no_const;
{
#if WITH_PARSER_CODEGEN
	struct code_context ccstorage, *cc;

	/* If there is no body, return NULL */
	if (FunctionBody_isempty(interp, node))
	    return NULL;

	cc = &ccstorage;
	cg_init(interp, cc, no_const);
	CODEGEN(node);
	return cg_fini(interp, cc, node->maxstack);
#else
	return node;
#endif
}


/*------------------------------------------------------------
 * LL(2) lookahead implementation
 */

/*
 * Returns the token that is n tokens ahead. (0 is the next token.)
 */
static int
lookahead(parser, n)
	struct parser *parser;
	int n;
{
	int token;
	SEE_ASSERT(parser->interpreter, n < (UNGET_MAX - 1));

	while ((UNGET_MAX + parser->unget_end - parser->unget) % UNGET_MAX < n)
	{
	    SEE_VALUE_COPY(&parser->unget_val[parser->unget_end], 
		&parser->lex->value);
	    parser->unget_tok[parser->unget_end] =
		parser->lex->next;
	    parser->unget_lin[parser->unget_end] =
		parser->lex->next_lineno;
	    parser->unget_fnl[parser->unget_end] = 
		parser->lex->next_follows_nl;
	    SEE_lex_next(parser->lex);
	    parser->unget_end = (parser->unget_end + 1) % UNGET_MAX;
	}
	if ((parser->unget + n) % UNGET_MAX == parser->unget_end)
		token = parser->lex->next;
	else
		token = parser->unget_tok[(parser->unget + n) % UNGET_MAX];

#ifndef NDEBUG
	if (SEE_parse_debug)
	    dprintf("lookahead(%d) -> %s\n", n, SEE_tokenname(token));
#endif

	return token;
}

#if WITH_PARSER_EVAL
/*
 * Generates a trace event, giving the host application an opportunity to
 * step or trace execution.
 */
static void
trace_event(ctxt, event)
	struct SEE_context *ctxt;
	enum SEE_trace_event event;
{
	if (ctxt->interpreter->trace)
	    (*ctxt->interpreter->trace)(ctxt->interpreter,
		ctxt->interpreter->try_location, ctxt, event);
}

/*
 * Pushes a new call context entry onto the traceback stack.
 * Returns the old traceback stack.
 */
static struct SEE_traceback *
traceback_enter(interp, callee, loc, call_type)
	struct SEE_interpreter *interp;
	struct SEE_object *callee;
	struct SEE_throw_location *loc;
	int call_type;
{
	struct SEE_traceback *old_tb, *tb;

	old_tb = interp->traceback;

	tb = SEE_NEW(interp, struct SEE_traceback);
	tb->call_location = loc;
	tb->callee = callee;
	tb->call_type = call_type;
	tb->prev = old_tb;
	interp->traceback = tb;

	return old_tb;
}

/*
 * Restores the traceback list before a call context was entered.
 */
static void
traceback_leave(interp, old_tb)
	struct SEE_interpreter *interp;
	struct SEE_traceback *old_tb;
{
	interp->traceback = old_tb;
}
#endif


#if WITH_PARSER_EVAL
/*------------------------------------------------------------
 * GetValue/SetValue
 */

/* 8.7.1 */
static void
GetValue(context, v, res)
	struct SEE_context *context;
	struct SEE_value *v;
	struct SEE_value *res;
{
	struct SEE_interpreter *interp = context->interpreter;

	if (SEE_VALUE_GET_TYPE(v) != SEE_REFERENCE) {
		if (v != res)
			SEE_VALUE_COPY(res, v);
		return;
	}
	if (v->u.reference.base == NULL)
		SEE_error_throw_string(interp, interp->ReferenceError, 
		    v->u.reference.property);
	else
		SEE_OBJECT_GET(interp, v->u.reference.base, 
		    v->u.reference.property, res);
}

/* 8.7.2 */
static void
PutValue(context, v, w)
	struct SEE_context *context;
	struct SEE_value *v;
	struct SEE_value *w;
{
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_object *target;

	if (SEE_VALUE_GET_TYPE(v) != SEE_REFERENCE)
		SEE_error_throw_string(interp, interp->ReferenceError,
		    STR(bad_lvalue));
	target = v->u.reference.base;
	if (target == NULL)
		target = interp->Global;
	SEE_OBJECT_PUT(interp, target, v->u.reference.property, w, 0);
}
#endif

/*------------------------------------------------------------
 * Error handling
 */

/*
 * Generates an error string prefixed with the filename and 
 * line number of the next token. e.g. "foo.js:23: blah blah".
 * This is useful for error messages.
 */
static struct SEE_string *
error_at(struct parser *parser, const char *fmt, ...)
{
	va_list ap;
	struct SEE_throw_location here;
	struct SEE_string *msg;
	struct SEE_interpreter *interp = parser->interpreter;

	here.lineno = NEXT_LINENO;
	here.filename = NEXT_FILENAME;

	va_start(ap, fmt);
	msg = SEE_string_vsprintf(interp, fmt, ap);
	va_end(ap);

	return SEE_string_concat(interp,
	    SEE_location_string(interp, &here), msg);
}

/*------------------------------------------------------------
 * Constant subexpression reduction
 *
 *  A subtree is 'constant' iff it
 *	- has no side-effects; and
 *	- yields the same result independent of context
 *  It follows then that a constant subtree can be evaluated using 
 *  a NULL context. We can perform that eval, and then replace the
 *  subtree with a node that generates that expression statically.
 */

/* Always returns true to indicate this class of node is always constant. */
static int
Always_isconst(na, interp)
	struct node *na;
	struct SEE_interpreter *interp;
{
	return 1;
}

/*------------------------------------------------------------
 * Parser
 *
 * Each group of grammar productions is ordered:
 *   - production summary as a comment
 *   - node structure
 *   - evaluator function
 *   - function processor
 *   - node printer
 *   - recursive-descent parser
 */

/* -- 7.8
 *	Literal:
 *	 	NullLiteral
 *	 	BooleanLiteral
 *	 	NumericLiteral
 *	 	StringLiteral
 *
 *	NullLiteral:
 *		tNULL				-- 7.8.1
 *
 *	BooleanLiteral:
 *		tTRUE				-- 7.8.2
 *		tFALSE				-- 7.8.2
 */

#if WITH_PARSER_EVAL
static void
Literal_eval(na, context, res)
	struct node *na; /* (struct Literal_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Literal_node *n = CAST_NODE(na, Literal);
	SEE_VALUE_COPY(res, &n->value);
}
#endif

#if WITH_PARSER_CODEGEN
static void
Literal_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Literal_node *n = CAST_NODE(na, Literal);

	CG_LITERAL(&n->value);		    /* val */
	if (SEE_VALUE_GET_TYPE(&n->value) == SEE_BOOLEAN)
	    n->node.is = CG_TYPE_BOOLEAN;
	else if (SEE_VALUE_GET_TYPE(&n->value) == SEE_NULL)
	    n->node.is = CG_TYPE_NULL;
	n->node.maxstack = 1;
}
#endif


static struct node *
Literal_parse(parser)
	struct parser *parser;
{
	struct Literal_node *n;

	/*
	 * Convert the next token into a regular expression
	 * if possible
	 */

	switch (NEXT) {
	case tNULL:
		n = NEW_NODE(struct Literal_node, NODECLASS_Literal);
		SEE_SET_NULL(&n->value);
		SKIP;
		return (struct node *)n;
	case tTRUE:
	case tFALSE:
		n = NEW_NODE(struct Literal_node,  NODECLASS_Literal);
		SEE_SET_BOOLEAN(&n->value, (NEXT == tTRUE));
		SKIP;
		return (struct node *)n;
	case tNUMBER:
		return PARSE(NumericLiteral);
	case tSTRING:
		return PARSE(StringLiteral);
	case tDIV:
	case tDIVEQ:
		SEE_lex_regex(parser->lex);
		return PARSE(RegularExpressionLiteral);
	default:
		EXPECTED("null, true, false, number, string, or regex");
	}
	/* NOTREACHED */
}

/*
 *	NumericLiteral:
 *		tNUMBER				-- 7.8.3
 */


static struct node *
NumericLiteral_parse(parser)
	struct parser *parser;
{
	struct Literal_node *n;

	EXPECT_NOSKIP(tNUMBER);
	n = NEW_NODE(struct Literal_node, NODECLASS_Literal);
	SEE_VALUE_COPY(&n->value, NEXT_VALUE);
	SKIP;
	return (struct node *)n;
}

/*
 *	StringLiteral:
 *		tSTRING				-- 7.8.4
 */

#if WITH_PARSER_EVAL
static void
StringLiteral_eval(na, context, res)
	struct node *na; /* (struct StringLiteral_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct StringLiteral_node *n = CAST_NODE(na, StringLiteral);
	SEE_SET_STRING(res, n->string);
}
#endif

#if WITH_PARSER_CODEGEN
static void
StringLiteral_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct StringLiteral_node *n = CAST_NODE(na, StringLiteral);

	CG_STRING(n->string);		/* str */
	n->node.is = CG_TYPE_STRING;
	n->node.maxstack = 1;
}
#endif


static struct node *
StringLiteral_parse(parser)
	struct parser *parser;
{
	struct StringLiteral_node *n;

	EXPECT_NOSKIP(tSTRING);
	n = NEW_NODE(struct StringLiteral_node, NODECLASS_StringLiteral);
	n->string = NEXT_VALUE->u.string;
	SKIP;
	return (struct node *)n;
}

/*
 *	RegularExpressionLiteral:
 *		tREGEX				-- 7.8.5
 */

#if WITH_PARSER_EVAL
static void
RegularExpressionLiteral_eval(na, context, res)
	struct node *na; /* (struct RegularExpressionLiteral_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct RegularExpressionLiteral_node *n = 
		CAST_NODE(na, RegularExpressionLiteral);
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_traceback *tb;

        tb = traceback_enter(interp, interp->RegExp, &n->node.location,
		SEE_CALLTYPE_CONSTRUCT);
	TRACE(&na->location, context, SEE_TRACE_CALL);
	SEE_OBJECT_CONSTRUCT(interp, interp->RegExp, NULL, 
		2, n->argv, res);
	TRACE(&na->location, context, SEE_TRACE_RETURN);
        traceback_leave(interp, tb);
}
#endif

#if WITH_PARSER_CODEGEN
static void
RegularExpressionLiteral_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct RegularExpressionLiteral_node *n = 
		CAST_NODE(na, RegularExpressionLiteral);

	SEE_ASSERT(cc->code->interpreter, 
	    SEE_VALUE_GET_TYPE(&n->pattern) == SEE_STRING);
	SEE_ASSERT(cc->code->interpreter, 
	    SEE_VALUE_GET_TYPE(&n->flags) == SEE_STRING);

	CG_REGEXP();			/* obj */
	CG_STRING(n->pattern.u.string);	/* obj str */
	CG_STRING(n->flags.u.string);	/* obj str str */
	CG_NEW(2);			/* obj */

	n->node.is = CG_TYPE_OBJECT;
	n->node.maxstack = 3;
}
#endif


static struct node *
RegularExpressionLiteral_parse(parser)
	struct parser *parser;
{
	struct RegularExpressionLiteral_node *n = NULL;
	struct SEE_string *s, *pattern, *flags;
	int p;

	if (NEXT == tREGEX)  {
	    /*
	     * Find the position after the regexp's closing '/'.
	     * i.e. the position of the regexp flags.
	     */
	    s = NEXT_VALUE->u.string;
	    for (p = s->length; p > 0; p--)
		    if (s->data[p-1] == '/')
			    break;
	    SEE_ASSERT(parser->interpreter, p > 1);

	    pattern = SEE_string_substr(parser->interpreter,
		s, 1, p - 2);
	    flags = SEE_string_substr(parser->interpreter,
		s, p, s->length - p);

	    n = NEW_NODE(struct RegularExpressionLiteral_node,
		NODECLASS_RegularExpressionLiteral);
	    SEE_SET_STRING(&n->pattern, pattern);
	    SEE_SET_STRING(&n->flags, flags);
	    n->argv[0] = &n->pattern;
	    n->argv[1] = &n->flags;

	}
	EXPECT(tREGEX);
	return (struct node *)n;
}

/*------------------------------------------------------------
 * -- 11.1
 *
 *	PrimaryExpression
 *	:	tTHIS				-- 11.1.1
 *	|	tIDENT				-- 11.1.2
 *	|	Literal				-- 11.1.3
 *	|	ArrayLiteral
 *	|	ObjectLiteral
 *	|	'(' Expression ')'		-- 11.1.6
 *	;
 */

/* 11.1.1 */
#if WITH_PARSER_EVAL
static void
PrimaryExpression_this_eval(n, context, res)
	struct node *n;
	struct SEE_context *context;
	struct SEE_value *res;
{
	SEE_ASSERT(context->interpreter, context->thisobj != NULL);
	SEE_SET_OBJECT(res, context->thisobj);
}
#endif

#if WITH_PARSER_CODEGEN
static void
PrimaryExpression_this_codegen(n, cc)
	struct node *n;
	struct code_context *cc;
{
	CG_THIS();		/* obj */

	n->is = CG_TYPE_OBJECT;
	n->maxstack = 1;
}
#endif



/* 11.1.2 */
#if WITH_PARSER_EVAL
static void
PrimaryExpression_ident_eval(na, context, res)
	struct node *na; /* (struct PrimaryExpression_ident_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct PrimaryExpression_ident_node *n = 
		CAST_NODE(na, PrimaryExpression_ident);
	SEE_scope_lookup(context->interpreter, context->scope, n->string, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
PrimaryExpression_ident_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct PrimaryExpression_ident_node *n = 
		CAST_NODE(na, PrimaryExpression_ident);

	if (cg_var_is_in_scope(cc, n->string)) 
	    CG_VREF(cg_var_id(cc, n->string));	/* ref */
	else {
	    CG_STRING(n->string);		/* str */
	    CG_LOOKUP();			/* ref */
	}

	n->node.is = CG_TYPE_REFERENCE;
	n->node.maxstack = 2;
}
#endif



static struct node *
PrimaryExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct PrimaryExpression_ident_node *i;

	switch (NEXT) {
	case tTHIS:
		n = NEW_NODE(struct node, NODECLASS_PrimaryExpression_this);
		SKIP;
		return n;
	case tIDENT:
		i = NEW_NODE(struct PrimaryExpression_ident_node,
			NODECLASS_PrimaryExpression_ident);
		i->string = NEXT_VALUE->u.string;
		SKIP;
		return (struct node *)i;
	case '[':
		return PARSE(ArrayLiteral);
	case '{':
		return PARSE(ObjectLiteral);
	case '(':
		SKIP;
		n = PARSE(Expression);
		EXPECT(')');
		return n;
	default:
		return PARSE(Literal);
	}
}

/*
 *	ArrayLiteral				-- 11.1.4
 *	:	'[' ']'
 *	|	'[' Elision ']'
 *	|	'[' ElementList ']'
 *	|	'[' ElementList ',' ']'
 *	|	'[' ElementList ',' Elision ']'
 *	;
 *
 *	ElementList
 *	:	Elision AssignmentExpression
 *	|	AssignmentExpression
 *	|	ElementList ',' Elision AssignmentExpression
 *	|	ElementList ',' AssignmentExpression
 *	;
 *
 *	Elision
 *	:	','
 *	|	Elision ','
 *	;
 *
 * NB: I ignore the above elision nonsense and just build a list of
 * (index,expr) nodes with an overall length. It is equivalent 
 * to that in the standard.
 */

/* 11.1.4 */
#if WITH_PARSER_EVAL
static void
ArrayLiteral_eval(na, context, res)
	struct node *na; /* (struct ArrayLiteral_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct ArrayLiteral_node *n = CAST_NODE(na, ArrayLiteral);
	struct ArrayLiteral_element *element;
	struct SEE_value expv, elv;
	struct SEE_string *ind;
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_traceback *tb;

	ind = SEE_string_new(interp, 16);

        tb = traceback_enter(interp, interp->Array, &n->node.location,
		SEE_CALLTYPE_CONSTRUCT);
	TRACE(&na->location, context, SEE_TRACE_CALL);
	SEE_OBJECT_CONSTRUCT(interp, interp->Array, NULL, 
		0, NULL, res);
	TRACE(&na->location, context, SEE_TRACE_RETURN);
        traceback_leave(interp, tb);

	for (element = n->first; element; element = element->next) {
		EVAL(element->expr, context, &expv);
		GetValue(context, &expv, &elv);
		ind->length = 0;
		SEE_string_append_int(ind, element->index);
		SEE_OBJECT_PUT(interp, res->u.object, 
		    SEE_intern(interp, ind), &elv, 0);
	}
	SEE_SET_NUMBER(&elv, n->length);
	SEE_OBJECT_PUT(interp, res->u.object, STR(length), &elv, 0);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ArrayLiteral_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct ArrayLiteral_node *n = CAST_NODE(na, ArrayLiteral);
	struct ArrayLiteral_element *element;
	struct SEE_string *ind;
	struct SEE_interpreter *interp = cc->code->interpreter;
	unsigned int maxstack = 0;

	ind = SEE_string_new(interp, 16);

	CG_ARRAY();			    /* Array */
	CG_NEW(0);			    /* a */

	for (element = n->first; element; element = element->next) {
		CG_DUP();		    /* a a */
		ind->length = 0;
		SEE_string_append_int(ind, element->index);
		CG_STRING(SEE_intern(interp, ind)); /* a a "element" */
		CG_REF();		    /* a a[element] */
		CODEGEN(element->expr);	    /* a a[element] ref */
		maxstack = MAX(maxstack, element->expr->maxstack);

		if (!CG_IS_VALUE(element->expr))
		    CG_GETVALUE();	    /* a a[element] val */
		CG_PUTVALUE();		    /* a */
	}
	
	CG_DUP();			    /* a a */
	CG_STRING(STR(length));		    /* a a "length" */
	CG_REF();			    /* a a.length */
	CG_NUMBER(n->length);		    /* a a.length num */ 
	CG_PUTVALUE();			    /* a */

	n->node.is = CG_TYPE_OBJECT;
	n->node.maxstack = MAX(3, 2 + maxstack);
}
#endif



static struct node *
ArrayLiteral_parse(parser)
	struct parser *parser;
{
	struct ArrayLiteral_node *n;
	struct ArrayLiteral_element **elp;
	int index;

	n = NEW_NODE(struct ArrayLiteral_node,
	    NODECLASS_ArrayLiteral);
	elp = &n->first;

	EXPECT('[');
	index = 0;
	while (NEXT != ']')
		if (NEXT == ',') {
			index++;
			SKIP;
		} else {
			*elp = SEE_NEW(parser->interpreter,
			    struct ArrayLiteral_element);
			(*elp)->index = index;
			(*elp)->expr = PARSE(AssignmentExpression);
			elp = &(*elp)->next;
			index++;
			if (NEXT != ']')
				EXPECTX(',', "',' or ']'");
		}
	n->length = index;
	*elp = NULL;
	EXPECT(']');
	return (struct node *)n;
}

/*
 *	ObjectLiteral				-- 11.1.5
 *	:	'{' '}'
 *	|	'{' PropertyNameAndValueList '}'
 *	;
 *
 *	PropertyNameAndValueList
 *	:	PropertyName ':' AssignmentExpression
 *	|	PropertyNameAndValueList ',' PropertyName ':' 
 *							AssignmentExpression
 *	;
 *
 *	PropertyName
 *	:	tIDENT
 *	|	StringLiteral
 *	|	NumericLiteral
 *	;
 */

/* 11.1.5 */
#if WITH_PARSER_EVAL
static void
ObjectLiteral_eval(na, context, res)
	struct node *na; /* (struct ObjectLiteral_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct ObjectLiteral_node *n = CAST_NODE(na, ObjectLiteral);
	struct SEE_value valuev, v;
	struct SEE_object *o;
	struct ObjectLiteral_pair *pair;
	struct SEE_interpreter *interp = context->interpreter;

	o = SEE_Object_new(interp);
	for (pair = n->first; pair; pair = pair->next) {
		EVAL(pair->value, context, &valuev);
		GetValue(context, &valuev, &v);
		SEE_OBJECT_PUT(interp, o, pair->name, &v, 0);
	}
	SEE_SET_OBJECT(res, o);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ObjectLiteral_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct ObjectLiteral_node *n = CAST_NODE(na, ObjectLiteral);
	struct ObjectLiteral_pair *pair;
	unsigned int maxstack = 0;

	CG_OBJECT();			    /* Object */
	CG_NEW(0);			    /* o */
	for (pair = n->first; pair; pair = pair->next) {
		CG_DUP();		    /* o o */
		CG_STRING(pair->name);	    /* o o name */
		CG_REF();		    /* o o.name */
		CODEGEN(pair->value);	    /* o o.name ref */
		maxstack = MAX(maxstack, pair->value->maxstack);
		if (!CG_IS_VALUE(pair->value))
		    CG_GETVALUE();	    /* o o.name val */
		CG_PUTVALUE();		    /* o */
	}

	n->node.is = CG_TYPE_OBJECT;
	n->node.maxstack = MAX(maxstack + 2, 3);
}
#endif



static struct node *
ObjectLiteral_parse(parser)
	struct parser *parser;
{
	struct ObjectLiteral_node *n;
	struct ObjectLiteral_pair **pairp;
	struct SEE_value sv;
	struct SEE_interpreter *interp = parser->interpreter;

	n = NEW_NODE(struct ObjectLiteral_node,
			NODECLASS_ObjectLiteral);
	pairp = &n->first;

	EXPECT('{');
	while (NEXT != '}') {
	    *pairp = SEE_NEW(interp, struct ObjectLiteral_pair);
	    switch (NEXT) {
	    case tIDENT:
	    case tSTRING:
		(*pairp)->name = SEE_intern(interp, NEXT_VALUE->u.string);
		SKIP;
		break;
	    case tNUMBER:
		SEE_ToString(parser->interpreter, NEXT_VALUE, &sv);
		(*pairp)->name = SEE_intern(interp, sv.u.string);
		SKIP;
		break;
	    default:
		EXPECTED("string, identifier or number");
	    }
	    EXPECT(':');
	    (*pairp)->value = PARSE(AssignmentExpression);
	    if (NEXT != '}') {
		    /* XXX permits trailing comma e.g. {a:b,} */
		    EXPECTX(',', "',' or '}'"); 
	    }
	    pairp = &(*pairp)->next;
	}
	*pairp = NULL;
	EXPECT('}');
	return (struct node *)n;
}

/*
 *	-- 11.2
 *
 *	MemberExpression
 *	:	PrimaryExpression
 *	|	FunctionExpression				-- 11.2.5
 *	|	MemberExpression '[' Expression ']'		-- 11.2.1
 *	|	MemberExpression '.' tIDENT			-- 11.2.1
 *	|	tNEW MemberExpression Arguments			-- 11.2.2
 *	;
 *
 *	NewExpression
 *	:	MemberExpression
 *	|	tNEW NewExpression				-- 11.2.2
 *	;
 *
 *	CallExpression
 *	:	MemberExpression Arguments			-- 11.2.3
 *	|	CallExpression Arguments			-- 11.2.3
 *	|	CallExpression '[' Expression ']'		-- 11.2.1
 *	|	CallExpression '.' tIDENT			-- 11.2.1
 *	;
 *
 *	Arguments
 *	:	'(' ')'						-- 11.2.4
 *	|	'(' ArgumentList ')'				-- 11.2.4
 *	;
 *
 *	ArgumentList
 *	:	AssignmentExpression				-- 11.2.4
 *	|	ArgumentList ',' AssignmentExpression		-- 11.2.4
 *	;
 *
 *	LeftHandSideExpression
 *	:	NewExpression
 *	|	CallExpression
 *	;
 *
 * NOTE:  The standard grammar is complicated in order to resolve an 
 *        ambiguity in parsing 'new expr ( args )' as either
 *	  '(new  expr)(args)' or as 'new (expr(args))'. In fact, 'new'
 *	  is acting as both a unary and a binary operator. Yucky.
 *
 *	  Since recursive descent is single-token lookahead, we
 *	  can rewrite the above as the following equivalent grammar:
 *
 *	MemberExpression
 *	:	PrimaryExpression
 *	|	FunctionExpression		    -- lookahead == tFUNCTION
 *	|	MemberExpression '[' Expression ']'
 *	|	MemberExpression '.' tIDENT
 *	|	tNEW MemberExpression Arguments	    -- lookahead == tNEW
 *	|	tNEW MemberExpression 	            -- lookahead == tNEW
 *
 *	LeftHandSideExpression
 *	:	PrimaryExpression
 *	|	FunctionExpression		    -- lookahead == tFUNCTION
 *	|	LeftHandSideExpression '[' Expression ']'
 *	|	LeftHandSideExpression '.' tIDENT
 *	|	LeftHandSideExpression Arguments
 *	|	MemberExpression		    -- lookahead == tNEW
 *
 */

/* 11.2.4 */
#if WITH_PARSER_EVAL
static void
Arguments_eval(na, context, res)
	struct node *na; /* (struct Arguments_node) */
	struct SEE_context *context;
	struct SEE_value *res;		/* Assumed pointer to array */
{
	struct Arguments_node *n = CAST_NODE(na, Arguments);
	struct Arguments_arg *arg;
	struct SEE_value v;

	for (arg = n->first; arg; arg = arg->next) {
		EVAL(arg->expr, context, &v);
		GetValue(context, &v, res);
		res++;
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
Arguments_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Arguments_node *n = CAST_NODE(na, Arguments);
	struct Arguments_arg *arg;
	unsigned int maxstack = 0;
	unsigned int onstack = 0;

	for (arg = n->first; arg; arg = arg->next) {
					    /* ... */
		CODEGEN(arg->expr);	    /* ... ref */
		maxstack = MAX(maxstack, onstack + arg->expr->maxstack);
		if (!CG_IS_VALUE(arg->expr))
		    CG_GETVALUE();	    /* ... val */
		onstack++;
	}
	n->node.maxstack = maxstack;
}
#endif


static int
Arguments_isconst(na, interp)
	struct node *na; /* (struct Arguments_node) */
	struct SEE_interpreter *interp;
{
	struct Arguments_node *n = CAST_NODE(na, Arguments);
	struct Arguments_arg *arg;

	for (arg = n->first; arg; arg = arg->next)
		if (!ISCONST(arg->expr, interp))
			return 0;
	return 1;
}


static struct Arguments_node *
Arguments_parse(parser)
	struct parser *parser;
{
	struct Arguments_node *n;
	struct Arguments_arg **argp;

	n = NEW_NODE(struct Arguments_node,
			NODECLASS_Arguments);
	argp = &n->first;
	n->argc = 0;

	EXPECT('(');
	while (NEXT != ')') {
		n->argc++;
		*argp = SEE_NEW(parser->interpreter, struct Arguments_arg);
		(*argp)->expr = PARSE(AssignmentExpression);
		argp = &(*argp)->next;
		if (NEXT != ')')
			EXPECTX(',', "',' or ')'");
	}
	*argp = NULL;
	EXPECT(')');
	return n;
}


/* 11.2.2 */
#if WITH_PARSER_EVAL
static void
MemberExpression_new_eval(na, context, res)
	struct node *na; /* (struct MemberExpression_new_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct MemberExpression_new_node *n = 
		CAST_NODE(na, MemberExpression_new);
	struct SEE_value r1, r2, *args, **argv;
	struct SEE_interpreter *interp = context->interpreter;
	int argc, i;
	struct SEE_traceback *tb;

	EVAL(n->mexp, context, &r1);
	GetValue(context, &r1, &r2);
	if (n->args) {
		argc = n->args->argc;
		args = SEE_ALLOCA(interp, struct SEE_value, argc);
		argv = SEE_ALLOCA(interp, struct SEE_value *, argc);
		Arguments_eval((struct node *)n->args, context, args);
		for (i = 0; i < argc; i++)
			argv[i] = &args[i];
	} else {
		argc = 0;
		argv = NULL;
	}
	if (SEE_VALUE_GET_TYPE(&r2) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
			STR(new_not_an_object));
	if (!SEE_OBJECT_HAS_CONSTRUCT(r2.u.object))
		SEE_error_throw_string(interp, interp->TypeError,
			STR(not_a_constructor));
        tb = traceback_enter(interp, r2.u.object, &n->node.location,
		SEE_CALLTYPE_CONSTRUCT);
	TRACE(&na->location, context, SEE_TRACE_CALL);
	SEE_OBJECT_CONSTRUCT(interp, r2.u.object, NULL, 
		argc, argv, res);
	TRACE(&na->location, context, SEE_TRACE_RETURN);
	traceback_leave(interp, tb);
}
#endif

#if WITH_PARSER_CODEGEN
static void
MemberExpression_new_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct MemberExpression_new_node *n = 
		CAST_NODE(na, MemberExpression_new);
	int argc;
	int maxstack = 0;

	CODEGEN(n->mexp);		/* ref */
	maxstack = n->mexp->maxstack;
	if (!CG_IS_VALUE(n->mexp))
	    CG_GETVALUE();		/* val */
	if (n->args) {
		Arguments_codegen((struct node *)n->args, cc);
					/* val arg1..argn */
		argc = n->args->argc;
		maxstack = MAX(maxstack, 1+((struct node *)n->args)->maxstack);
	} else
		argc = 0;
	CG_NEW(argc);			/* obj */

	/* Assume that 'new' always yields an object by s8.6.2 */
	n->node.is = CG_TYPE_OBJECT;	
	n->node.maxstack = maxstack;
}
#endif




/* 11.2.1 */
#if WITH_PARSER_EVAL
static void
MemberExpression_dot_eval(na, context, res)
	struct node *na; /* (struct MemberExpression_dot_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct MemberExpression_dot_node *n = 
		CAST_NODE(na, MemberExpression_dot);
	struct SEE_value r1, r2, r5;
	struct SEE_interpreter *interp = context->interpreter;

	EVAL(n->mexp, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToObject(interp, &r2, &r5);
	_SEE_SET_REFERENCE(res, r5.u.object, n->name);
}
#endif

#if WITH_PARSER_CODEGEN
static void
MemberExpression_dot_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct MemberExpression_dot_node *n = 
		CAST_NODE(na, MemberExpression_dot);

	CODEGEN(n->mexp);	    /* ref */
	if (!CG_IS_VALUE(n->mexp))
	    CG_GETVALUE();	    /* val */
	if (!CG_IS_OBJECT(n->mexp))
	    CG_TOOBJECT();	    /* obj */
	CG_STRING(n->name);	    /* obj "name" */
	CG_REF();		    /* ref */

	n->node.is = CG_TYPE_REFERENCE;
	n->node.maxstack = MAX(2, n->mexp->maxstack);
}
#endif




/* 11.2.1 */
#if WITH_PARSER_EVAL
static void
MemberExpression_bracket_eval(na, context, res)
	struct node *na; /* (struct MemberExpression_bracket_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct MemberExpression_bracket_node *n = 
		CAST_NODE(na, MemberExpression_bracket);
	struct SEE_value r1, r2, r3, r4, r5, r6;
	struct SEE_interpreter *interp = context->interpreter;

	EVAL(n->mexp, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->name, context, &r3);
	GetValue(context, &r3, &r4);
	SEE_ToObject(interp, &r2, &r5);
	SEE_ToString(interp, &r4, &r6);
	_SEE_SET_REFERENCE(res, r5.u.object, SEE_intern(interp, r6.u.string));
}
#endif

#if WITH_PARSER_CODEGEN
static void
MemberExpression_bracket_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct MemberExpression_bracket_node *n = 
		CAST_NODE(na, MemberExpression_bracket);

	CODEGEN(n->mexp);	    /* ref1 */
	if (!CG_IS_VALUE(n->mexp))
	    CG_GETVALUE();	    /* val1 */
	CODEGEN(n->name);	    /* val1 ref2 */
	if (!CG_IS_VALUE(n->name))
	    CG_GETVALUE();	    /* val1 val2 */
	/* Note: we have to fritz with EXCH to match
	 * the semantics of 11.2.1 */
	if (!CG_IS_OBJECT(n->mexp)) {
	    CG_EXCH();		    /* val2 val1 */
	    CG_TOOBJECT();	    /* val2 obj1 */
	    CG_EXCH();		    /* obj1 val2 */
	}
	if (!CG_IS_STRING(n->name))
	    CG_TOSTRING();	    /* obj1 str2 */
	CG_REF();		    /* ref */

	n->node.is = CG_TYPE_REFERENCE;
	n->node.maxstack = MAX(n->mexp->maxstack, 1 + n->name->maxstack);
}
#endif




static struct node *
MemberExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct MemberExpression_new_node *m;
	struct MemberExpression_dot_node *dn;
	struct MemberExpression_bracket_node *bn;

	switch (NEXT) {
        case tFUNCTION:
	    n = PARSE(FunctionExpression);
	    break;
        case tNEW:
	    m = NEW_NODE(struct MemberExpression_new_node,
	    	NODECLASS_MemberExpression_new);
	    SKIP;
	    m->mexp = PARSE(MemberExpression);
	    if (NEXT == '(')
		m->args = PARSE(Arguments);
	    else
		m->args = NULL;
	    n = (struct node *)m;
	    break;
	default:
	    n = PARSE(PrimaryExpression);
	}

	for (;;)
	    switch (NEXT) {
	    case '.':
		dn = NEW_NODE(struct MemberExpression_dot_node,
			NODECLASS_MemberExpression_dot);
		SKIP;
		if (NEXT == tIDENT) {
		    dn->mexp = n;
		    dn->name = NEXT_VALUE->u.string;
		    n = (struct node *)dn;
		}
	        EXPECT(tIDENT);
		break;
	    case '[':
		bn = NEW_NODE(struct MemberExpression_bracket_node,
			NODECLASS_MemberExpression_bracket);
		SKIP;
		bn->mexp = n;
		bn->name = PARSE(Expression);
		n = (struct node *)bn;
		EXPECT(']');
		break;
	    default:
		return n;
	    }
}


/* 11.2.3 */
#if WITH_PARSER_EVAL
static void CallExpression_eval_common(struct SEE_context *, 
	struct SEE_throw_location *, struct SEE_value *, int, 
	struct SEE_value **, struct SEE_value *);

static void
CallExpression_eval(na, context, res)
	struct node *na; /* (struct CallExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct CallExpression_node *n = CAST_NODE(na, CallExpression);
	struct SEE_value r1, *args, **argv;
	int argc, i;

	EVAL(n->exp, context, &r1);
	argc = n->args->argc;
	if (argc) {
		args = SEE_ALLOCA(context->interpreter, 
			struct SEE_value, argc);
		argv = SEE_ALLOCA(context->interpreter, 
			struct SEE_value *, argc);
		Arguments_eval((struct node *)n->args, context, args);
		for (i = 0; i < argc; i++)
			argv[i] = &args[i];
	} else 
		argv = NULL;
	CallExpression_eval_common(context, &na->location, &r1, 
		argc, argv, res);
}

static void
CallExpression_eval_common(context, loc, r1, argc, argv, res)
	struct SEE_context *context;
	struct SEE_throw_location *loc;
	struct SEE_value *r1;
	int argc;
	struct SEE_value **argv;
	struct SEE_value *res;
{
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_value r3;
	struct SEE_object *r6, *r7;
	struct SEE_traceback *tb;

	GetValue(context, r1, &r3);
	if (SEE_VALUE_GET_TYPE(&r3) == SEE_UNDEFINED)	/* nonstandard */
		SEE_error_throw_string(interp, interp->TypeError,
			STR(no_such_function));
	if (SEE_VALUE_GET_TYPE(&r3) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
			STR(not_a_function));
	if (!SEE_OBJECT_HAS_CALL(r3.u.object))
		SEE_error_throw_string(interp, interp->TypeError,
			STR(not_callable));
	if (SEE_VALUE_GET_TYPE(r1) == SEE_REFERENCE)
		r6 = r1->u.reference.base;
	else
		r6 = NULL;
	if (r6 != NULL && IS_ACTIVATION_OBJECT(r6))
		r7 = NULL;
	else
		r7 = r6;
        tb = traceback_enter(interp, r3.u.object, loc,
		SEE_CALLTYPE_CALL);
	TRACE(loc, context, SEE_TRACE_CALL);
	if (r3.u.object == interp->Global_eval) {
	    /* The special 'eval' function' */
	    eval(context, r7, argc, argv, res);
	} else {
#ifndef NDEBUG
	    SEE_SET_STRING(res, STR(internal_error));
#endif
	    if (!r7)
		r7 = interp->Global;
	    SEE_OBJECT_CALL(interp, r3.u.object, r7, argc, argv, res);
	}
	TRACE(loc, context, SEE_TRACE_RETURN);
        traceback_leave(interp, tb);
}
#endif /* WITH_PARSER_EVAL */

#if WITH_PARSER_CODEGEN
static void
CallExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct CallExpression_node *n = CAST_NODE(na, CallExpression);

	CODEGEN(n->exp);		/* ref */
	Arguments_codegen((struct node *)n->args, cc);	/* ref arg1 .. argn */
	CG_CALL(n->args->argc);		/* val */

	/* Called functions only return values */
	n->node.is = CG_TYPE_VALUE;
	n->node.maxstack = MAX(n->exp->maxstack,
	    1 + ((struct node *)n->args)->maxstack);
}
#endif



static struct node *
LeftHandSideExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct CallExpression_node *cn;
	struct MemberExpression_dot_node *dn;
	struct MemberExpression_bracket_node *bn;

	switch (NEXT) {
        case tFUNCTION:
	    n = PARSE(FunctionExpression);	/* 11.2.5 */
	    break;
        case tNEW:
	    n = PARSE(MemberExpression);
	    break;
	default:
	    n = PARSE(PrimaryExpression);
	}

	for (;;)  {

#ifndef NDEBUG
	    if (SEE_parse_debug)
	        dprintf("LeftHandSideExpression: islhs = %d next is %s\n",
		    parser->is_lhs, SEE_tokenname(NEXT));
#endif

	    switch (NEXT) {
	    case '.':
	        dn = NEW_NODE(struct MemberExpression_dot_node,
		    NODECLASS_MemberExpression_dot);
		SKIP;
		if (NEXT == tIDENT) {
		    dn->mexp = n;
		    dn->name = NEXT_VALUE->u.string;
		    n = (struct node *)dn;
		}
	        EXPECT(tIDENT);
		break;
	    case '[':
		bn = NEW_NODE(struct MemberExpression_bracket_node,
			NODECLASS_MemberExpression_bracket);
		SKIP;
		bn->mexp = n;
		bn->name = PARSE(Expression);
		n = (struct node *)bn;
		EXPECT(']');
		break;
	    case '(':
		cn = NEW_NODE(struct CallExpression_node,
			NODECLASS_CallExpression);
		cn->exp = n;
		cn->args = PARSE(Arguments);
		n = (struct node *)cn;
		break;
	    default:
		/* Eventually we leave via this clause */
		parser->is_lhs = 1;
		return n;
	    }
	}
}

/*
 *	-- 11.3
 *
 *	PostfixExpression
 *	:	LeftHandSideExpression
 *	|	LeftHandSideExpression { NOLINETERM; } tPLUSPLUS    -- 11.3.1
 *	|	LeftHandSideExpression { NOLINETERM; } tMINUSMINUS  -- 11.3.2
 *	;
 */



static int
Unary_isconst(na, interp)
	struct node *na; /* (struct Unary_node) */
	struct SEE_interpreter *interp;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	return ISCONST(n->a, interp);
}

/* 11.3.1 */
#if WITH_PARSER_EVAL
static void
PostfixExpression_inc_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2, r3;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToNumber(context->interpreter, &r2, res);
	SEE_SET_NUMBER(&r3, res->u.number + 1);
	PutValue(context, &r1, &r3);
}
#endif

#if WITH_PARSER_CODEGEN
static void
PostfixExpression_inc_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CODEGEN(n->a);		/* ref */
	CG_DUP();		/* ref ref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	/* ref val */
	if (!CG_IS_NUMBER(n->a))
	    CG_TONUMBER();	/* ref num */
	CG_DUP();		/* ref num num */
	CG_ROLL3();		/* num ref num */
	CG_NUMBER(1);		/* num ref num   1 */
	CG_ADD();		/* num ref num+1 */
	CG_PUTVALUE();		/* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 4);

	/*
	 * Peephole optimisation note:
	 *		ref num
	 *  DUP		ref num num
	 *  ROLL3       num ref num
	 *  LITERAL,?   num ref num ?
	 *  ADD|SUB     num ref ?
	 *  PUTVALUE    num
	 *  POP         -
	 *
	 * is equivalent to:
	 *              ref num
	 *  LITERAL,?   ref num ?
	 *  ADD|SUB     ref ?
	 *  PUTVALUE	-
	 */

	/*
	 * Peephole optimisation note:
	 *		ref num
	 *  DUP		ref num num
	 *  ROLL3       num ref num
	 *  LITERAL,?   num ref num ?
	 *  ADD|SUB     num ref ?
	 *  PUTVALUE    num
	 *  SETC        -
	 *
	 * is equivalent to:
	 *              ref num
	 *  LITERAL,?   ref num ?
	 *  ADD|SUB     ref ?
	 *  DUP		ref ? ?
	 *  SETC	ref ?
	 *  PUTVALUE	-
	 */

}
#endif



/* 11.3.2 */
#if WITH_PARSER_EVAL
static void
PostfixExpression_dec_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2, r3;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToNumber(context->interpreter, &r2, res);
	SEE_SET_NUMBER(&r3, res->u.number - 1);
	PutValue(context, &r1, &r3);
}
#endif

#if WITH_PARSER_CODEGEN
static void
PostfixExpression_dec_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CODEGEN(n->a);		/* aref */
	CG_DUP();		/* aref aref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	/* aref aval */
	if (!CG_IS_NUMBER(n->a))
	    CG_TONUMBER();	/* aref anum */
	CG_DUP();		/* aref anum anum */
	CG_ROLL3();		/* anum aref anum */
	CG_NUMBER(1);		/* anum aref anum   1 */
	CG_SUB();		/* anum aref anum-1 */
	CG_PUTVALUE();		/* anum */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 4);
}
#endif



static struct node *
PostfixExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Unary_node *pen;

	n = PARSE(LeftHandSideExpression);
	if (!NEXT_FOLLOWS_NL && 
	    (NEXT == tPLUSPLUS || NEXT == tMINUSMINUS))
	{
		pen = NEW_NODE(struct Unary_node,
			NEXT == tPLUSPLUS
			    ? NODECLASS_PostfixExpression_inc
			    : NODECLASS_PostfixExpression_dec);
		pen->a = n;
		n = (struct node *)pen;
		SKIP;
		parser->is_lhs = 0;
	}
	return n;
}

/*
 *	-- 11.4
 *
 *	UnaryExpression
 *	:	PostfixExpression
 *	|	tDELETE UnaryExpression				-- 11.4.1
 *	|	tVOID UnaryExpression				-- 11.4.2
 *	|	tTYPEOF UnaryExpression				-- 11.4.3
 *	|	tPLUSPLUS UnaryExpression			-- 11.4.4
 *	|	tMINUSMINUS UnaryExpression			-- 11.4.5
 *	|	'+' UnaryExpression				-- 11.4.6
 *	|	'-' UnaryExpression				-- 11.4.7
 *	|	'~' UnaryExpression				-- 11.4.8
 *	|	'!' UnaryExpression				-- 11.4.9
 *	;
 */

/* 11.4.1 */
#if WITH_PARSER_EVAL
static void UnaryExpression_delete_eval_common(struct SEE_context *,
	struct SEE_value *, struct SEE_value *);

static void
UnaryExpression_delete_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1;

	EVAL(n->a, context, &r1);
	UnaryExpression_delete_eval_common(context, &r1, res);
}

static void
UnaryExpression_delete_eval_common(context, r1, res)
	struct SEE_context *context;
	struct SEE_value *r1, *res;
{
	struct SEE_interpreter *interp = context->interpreter;

	if (SEE_VALUE_GET_TYPE(r1) != SEE_REFERENCE) {
		SEE_SET_BOOLEAN(res, 0);
		return;
	}
	/*
	 * spec bug: if the base is null, it isn't clear what is meant 
	 * to happen. We return true as if the fictitous property 
	 * owner existed.
	 */
	if (!r1->u.reference.base || 
	    SEE_OBJECT_DELETE(interp, r1->u.reference.base, 
	    		      r1->u.reference.property))
		SEE_SET_BOOLEAN(res, 1);
	else
		SEE_SET_BOOLEAN(res, 0);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_delete_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CODEGEN(n->a);	/* ref */
	CG_DELETE();	/* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = n->a->maxstack;
}
#endif



/* 11.4.2 */
#if WITH_PARSER_EVAL
static void
UnaryExpression_void_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_SET_UNDEFINED(res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_void_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	static const struct SEE_value cg_undefined = { SEE_UNDEFINED };

	CODEGEN(n->a);		    /* ref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	    /* val */
	CG_POP();		    /* - */
	CG_LITERAL(&cg_undefined);  /* undef */

	n->node.is = CG_TYPE_UNDEFINED;
	n->node.maxstack = n->a->maxstack;
}
#endif



/* 11.4.3 */
#if WITH_PARSER_EVAL
static void UnaryExpression_typeof_eval_common(struct SEE_context *,
	struct SEE_value *, struct SEE_value *);

static void
UnaryExpression_typeof_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1;

	EVAL(n->a, context, &r1);
	UnaryExpression_typeof_eval_common(context, &r1, res);
}

static void
UnaryExpression_typeof_eval_common(context, r1, res)
	struct SEE_context *context;
	struct SEE_value *r1, *res;
{
	struct SEE_value r4;
	struct SEE_string *s;

	if (SEE_VALUE_GET_TYPE(r1) == SEE_REFERENCE && 
	    r1->u.reference.base == NULL) 
	{
		SEE_SET_STRING(res, STR(undefined));
		return;
	}
	GetValue(context, r1, &r4);
	switch (SEE_VALUE_GET_TYPE(&r4)) {
	case SEE_UNDEFINED:	s = STR(undefined); break;
	case SEE_NULL:		s = STR(object); break;
	case SEE_BOOLEAN:	s = STR(boolean); break;
	case SEE_NUMBER:	s = STR(number); break;
	case SEE_STRING:	s = STR(string); break;
	case SEE_OBJECT:	s = SEE_OBJECT_HAS_CALL(r4.u.object)
				  ? STR(function)
				  : STR(object);
				break;
	default:		s = STR(unknown);
	}
	SEE_SET_STRING(res, s);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_typeof_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	
	CODEGEN(n->a);	    /* ref */
	CG_TYPEOF();	    /* str */

	n->node.is = CG_TYPE_STRING;
	n->node.maxstack = n->a->maxstack;
}
#endif



/* 11.4.4 */
#if WITH_PARSER_EVAL
static void
UnaryExpression_preinc_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToNumber(context->interpreter, &r2, res);
	res->u.number++;
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_preinc_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	/* Note: Makes no sense to check n->a is already a value */
	CODEGEN(n->a);	/* aref */
	CG_DUP();	/* aref aref */
	CG_GETVALUE();	/* aref aval */
	CG_TONUMBER();	/* aref anum */
	CG_NUMBER(1);	/* aref anum 1 */
	CG_ADD();	/* aref anum+1 */
	CG_DUP();	/* aref anum+1 anum+1 */
	CG_ROLL3();	/* anum+1 aref anum+1 */
	CG_PUTVALUE();	/* anum+1 */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 3);
}
#endif




/* 11.4.5 */
#if WITH_PARSER_EVAL
static void
UnaryExpression_predec_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToNumber(context->interpreter, &r2, res);
	res->u.number--;
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_predec_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	/* Note: Makes no sense to check n->a is already a value */
	CODEGEN(n->a);	/* aref */
	CG_DUP();	/* aref aref */
	CG_GETVALUE();	/* aref aval */
	CG_TONUMBER();	/* aref anum */
	CG_NUMBER(1);	/* aref anum 1 */
	CG_SUB();	/* aref anum-1 */
	CG_DUP();	/* aref anum-1 anum-1 */
	CG_ROLL3();   	/* anum-1 aref anum-1 */
	CG_PUTVALUE();	/* anum-1 */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 3);
}
#endif




/* 11.4.6 */
#if WITH_PARSER_EVAL
static void
UnaryExpression_plus_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToNumber(context->interpreter, &r2, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_plus_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CODEGEN(n->a);		/* aref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	/* aval */
	if (!CG_IS_NUMBER(n->a))
	    CG_TONUMBER();	/* anum */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = n->a->maxstack;
}
#endif




/* 11.4.7 */
#if WITH_PARSER_EVAL
static void
UnaryExpression_minus_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToNumber(context->interpreter, &r2, res);
	res->u.number = -(res->u.number);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_minus_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CODEGEN(n->a);		/* aref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	/* aval */
	if (!CG_IS_NUMBER(n->a))
	    CG_TONUMBER();	/* anum */
	CG_NEG();		/* -anum */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = n->a->maxstack;
}
#endif



/* 11.4.8 */
#if WITH_PARSER_EVAL
static void UnaryExpression_inv_eval_common(struct SEE_context *,
	struct SEE_value *, struct SEE_value *);

static void
UnaryExpression_inv_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	UnaryExpression_inv_eval_common(context, &r2, res);
}

static void
UnaryExpression_inv_eval_common(context, r2, res)
	struct SEE_context *context;
	struct SEE_value *r2, *res;
{
	SEE_int32_t r3;

	r3 = SEE_ToInt32(context->interpreter, r2);
	SEE_SET_NUMBER(res, ~r3);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_inv_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CODEGEN(n->a);		/* aref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	/* aval */
	CG_INV();		/* ~aval */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = n->a->maxstack;
}
#endif




/* 11.4.9 */
#if WITH_PARSER_EVAL
static void
UnaryExpression_not_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2, r3;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToBoolean(context->interpreter, &r2, &r3);
	SEE_SET_BOOLEAN(res, !r3.u.boolean);
}
#endif

#if WITH_PARSER_CODEGEN
static void
UnaryExpression_not_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CODEGEN(n->a);		    /* aref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	    /* aval */
	if (!CG_IS_BOOLEAN(n->a))
	    CG_TOBOOLEAN();	    /* abool */
	CG_NOT();		    /* !abool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = n->a->maxstack;
}
#endif



static struct node *
UnaryExpression_parse(parser)
	struct parser *parser;
{
	struct Unary_node *n;
	enum nodeclass_enum nc;

	switch (NEXT) {
	case tDELETE:
		nc = NODECLASS_UnaryExpression_delete;
		break;
	case tVOID:
		nc = NODECLASS_UnaryExpression_void;
		break;
	case tTYPEOF:
		nc = NODECLASS_UnaryExpression_typeof;
		break;
	case tPLUSPLUS:
		nc = NODECLASS_UnaryExpression_preinc;
		break;
	case tMINUSMINUS:
		nc = NODECLASS_UnaryExpression_predec;
		break;
	case '+':
		nc = NODECLASS_UnaryExpression_plus;
		break;
	case '-':
		nc = NODECLASS_UnaryExpression_minus;
		break;
	case '~':
		nc = NODECLASS_UnaryExpression_inv;
		break;
	case '!':
		nc = NODECLASS_UnaryExpression_not;
		break;
	default:
		return PARSE(PostfixExpression);
	}
	n = NEW_NODE(struct Unary_node, nc);
	SKIP;
	n->a = PARSE(UnaryExpression);
	parser->is_lhs = 0;
	return (struct node *)n;
}

/*
 *	-- 11.5
 *
 *	MultiplicativeExpression
 *	:	UnaryExpression
 *	|	MultiplicativeExpression '*' UnaryExpression	-- 11.5.1
 *	|	MultiplicativeExpression '/' UnaryExpression	-- 11.5.2
 *	|	MultiplicativeExpression '%' UnaryExpression	-- 11.5.3
 *	;
 */




static int
Binary_isconst(na, interp)
	struct node *na; /* (struct Binary_node) */
	struct SEE_interpreter *interp;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	return ISCONST(n->a, interp) && ISCONST(n->b, interp);
}

/* 11.5.1 */
#if WITH_PARSER_EVAL
static void
MultiplicativeExpression_mul_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	struct SEE_value r5, r6;

	SEE_ToNumber(context->interpreter, r2, &r5);
	SEE_ToNumber(context->interpreter, r4, &r6);
	SEE_SET_NUMBER(res, r5.u.number * r6.u.number);
}

static void
MultiplicativeExpression_mul_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	MultiplicativeExpression_mul_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
Binary_common_codegen(n, cc)
	struct Binary_node *n;
	struct code_context *cc;
{
	CODEGEN(n->a);	    /* aref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();  /* aval */
	CODEGEN(n->b);	    /* aval bref */
	if (!CG_IS_VALUE(n->b))
	    CG_GETVALUE();  /* aval bval */
}

static void
MultiplicativeExpression_common_codegen(n, cc)
	struct Binary_node *n;
	struct code_context *cc;
{
	Binary_common_codegen(n, cc); /* val val */
	if (!CG_IS_NUMBER(n->a)) {
	    /* Exchanges needed to match spec semantics */
	    CG_EXCH();	    /* bval aval */
	    CG_TONUMBER();  /* bval anum */
	    CG_EXCH();	    /* anum bval */
	}
	if (!CG_IS_NUMBER(n->b))
	    CG_TONUMBER();  /* anum bnum */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}

static void
MultiplicativeExpression_mul_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	MultiplicativeExpression_common_codegen(n, cc); /* num num */
	CG_MUL();	    /* num */
}
#endif




/* 11.5.2 */
#if WITH_PARSER_EVAL
static void
MultiplicativeExpression_div_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	struct SEE_value r5, r6;

	SEE_ToNumber(context->interpreter, r2, &r5);
	SEE_ToNumber(context->interpreter, r4, &r6);
	SEE_SET_NUMBER(res, r5.u.number / r6.u.number);
}

static void
MultiplicativeExpression_div_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
        MultiplicativeExpression_div_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
MultiplicativeExpression_div_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	MultiplicativeExpression_common_codegen(n, cc); /* num num */
	CG_DIV();	    /* num */
}
#endif




/* 11.5.3 */
#if WITH_PARSER_EVAL
static void
MultiplicativeExpression_mod_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	struct SEE_value r5, r6;

	SEE_ToNumber(context->interpreter, r2, &r5);
	SEE_ToNumber(context->interpreter, r4, &r6);
	SEE_SET_NUMBER(res, NUMBER_fmod(r5.u.number, r6.u.number));
}

static void
MultiplicativeExpression_mod_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	MultiplicativeExpression_mod_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
MultiplicativeExpression_mod_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	MultiplicativeExpression_common_codegen(n, cc); /* num num */
	CG_MOD();	    /* num */
}
#endif


static struct node *
MultiplicativeExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	enum nodeclass_enum nc;
	struct Binary_node *m;

	n = PARSE(UnaryExpression);
	for (;;) {
	    /* Left-to-right associative */
	    switch (NEXT) {
	    case '*':
		nc = NODECLASS_MultiplicativeExpression_mul;
		break;
	    case '/':
		nc = NODECLASS_MultiplicativeExpression_div;
		break;
	    case '%':
		nc = NODECLASS_MultiplicativeExpression_mod;
		break;
	    default:
		return n;
	    }
	    SKIP;
	    m = NEW_NODE(struct Binary_node, nc);
	    m->a = n;
	    m->b = PARSE(UnaryExpression);
	    parser->is_lhs = 0;
	    n = (struct node *)m;
	}
}

/*
 *	-- 11.6
 *
 *	AdditiveExpression
 *	:	MultiplicativeExpression
 *	|	AdditiveExpression '+' MultiplicativeExpression	-- 11.6.1
 *	|	AdditiveExpression '-' MultiplicativeExpression	-- 11.6.2
 *	;
 */

/* 11.6.1 */
#if WITH_PARSER_EVAL
static void
AdditiveExpression_add_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	struct SEE_value r5, r6,
			 r8, r9, r12, r13;
	struct SEE_string *s;

	SEE_ToPrimitive(context->interpreter, r2, NULL, &r5);
	SEE_ToPrimitive(context->interpreter, r4, NULL, &r6);
	if (!(SEE_VALUE_GET_TYPE(&r5) == SEE_STRING || 
	      SEE_VALUE_GET_TYPE(&r6) == SEE_STRING)) 
	{
		SEE_ToNumber(context->interpreter, &r5, &r8);
		SEE_ToNumber(context->interpreter, &r6, &r9);
		SEE_SET_NUMBER(res, r8.u.number + r9.u.number);
	} else {
		SEE_ToString(context->interpreter, &r5, &r12);
		SEE_ToString(context->interpreter, &r6, &r13);
		s = SEE_string_concat(context->interpreter, 
			r12.u.string, r13.u.string);
		SEE_SET_STRING(res, s);
	}
}

static void
AdditiveExpression_add_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	AdditiveExpression_add_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AdditiveExpression_add_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* val val */
	if (!CG_IS_PRIMITIVE(n->a)) {
	    CG_EXCH();		/* bval aval */
	    CG_TOPRIMITIVE();	/* bval aprim */
	    CG_EXCH();		/* aprim bval */
	}
	if (!CG_IS_PRIMITIVE(n->b))
	    CG_TOPRIMITIVE();	/* aprim bprim */
	CG_ADD();		/* val */

	/* Carefully figure out if the result type can be restricted */
	if (CG_IS_STRING(n->a) || CG_IS_STRING(n->b))
	    n->node.is = CG_TYPE_STRING;
	else if (CG_IS_PRIMITIVE(n->a) && CG_IS_PRIMITIVE(n->b))
	    n->node.is = CG_TYPE_NUMBER;
	else
	    n->node.is = CG_TYPE_STRING | CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif



/* 11.6.2 */
#if WITH_PARSER_EVAL
static void
AdditiveExpression_sub_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	struct SEE_value r5, r6;

	SEE_ToNumber(context->interpreter, r2, &r5);
	SEE_ToNumber(context->interpreter, r4, &r6);
	SEE_SET_NUMBER(res, r5.u.number - r6.u.number);
}

static void
AdditiveExpression_sub_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	AdditiveExpression_sub_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AdditiveExpression_sub_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	if (!CG_IS_NUMBER(n->a)) {
	    CG_EXCH();	    /* bval aval */
	    CG_TONUMBER();  /* bval anum */
	    CG_EXCH();	    /* anum bval */
	}
	if (!CG_IS_NUMBER(n->b))
	    CG_TONUMBER();  /* anum bnum */
	CG_SUB();	    /* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif


static struct node *
AdditiveExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	enum nodeclass_enum nc;
	struct Binary_node *m;

	n = PARSE(MultiplicativeExpression);
	for (;;) {
	    switch (NEXT) {
	    case '+':
		nc = NODECLASS_AdditiveExpression_add;
		break;
	    case '-':
		nc = NODECLASS_AdditiveExpression_sub;
		break;
	    default:
		return n;
	    }
	    parser->is_lhs = 0;
	    SKIP;
	    m = NEW_NODE(struct Binary_node, nc);
	    m->a = n;
	    m->b = PARSE(MultiplicativeExpression);
	    n = (struct node *)m;
	}
	return n;
}

/*
 *	-- 11.7
 *
 *	ShiftExpression
 *	:	AdditiveExpression
 *	|	ShiftExpression tLSHIFT AdditiveExpression	-- 11.7.1
 *	|	ShiftExpression tRSHIFT AdditiveExpression	-- 11.7.2
 *	|	ShiftExpression tURSHIFT AdditiveExpression	-- 11.7.3
 *	;
 */

/* 11.7.1 */
#if WITH_PARSER_EVAL
static void
ShiftExpression_lshift_common(r2, bn, context, res)
	struct SEE_value *r2, *res;
	struct node *bn;
	struct SEE_context *context;
{
	struct SEE_value r3, r4;
	SEE_int32_t r5;
	SEE_uint32_t r6;

	EVAL(bn, context, &r3);
	GetValue(context, &r3, &r4);
	r5 = SEE_ToInt32(context->interpreter, r2);
	r6 = SEE_ToUint32(context->interpreter, &r4);
	SEE_SET_NUMBER(res, r5 << (r6 & 0x1f));
}

static void
ShiftExpression_lshift_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	ShiftExpression_lshift_common(&r2, n->b, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ShiftExpression_lshift_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_LSHIFT();		/* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif



/* 11.7.2 */
#if WITH_PARSER_EVAL
static void
ShiftExpression_rshift_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	SEE_int32_t r5;
	SEE_uint32_t r6;

	r5 = SEE_ToInt32(context->interpreter, r2);
	r6 = SEE_ToUint32(context->interpreter, r4);
	SEE_SET_NUMBER(res, r5 >> (r6 & 0x1f));
}

static void
ShiftExpression_rshift_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	ShiftExpression_rshift_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ShiftExpression_rshift_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_RSHIFT();		/* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




/* 11.7.3 */
#if WITH_PARSER_EVAL
static void
ShiftExpression_urshift_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	SEE_uint32_t r5, r6;

	r5 = SEE_ToUint32(context->interpreter, r2);
	r6 = SEE_ToUint32(context->interpreter, r4);
	SEE_SET_NUMBER(res, r5 >> (r6 & 0x1f));
}

static void
ShiftExpression_urshift_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	ShiftExpression_urshift_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ShiftExpression_urshift_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_URSHIFT();		      /* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




static struct node *
ShiftExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	enum nodeclass_enum nc;
	struct Binary_node *sn;

	n = PARSE(AdditiveExpression);
	for (;;) {
	    /* Left associative */
	    switch (NEXT) {
	    case tLSHIFT:
		nc = NODECLASS_ShiftExpression_lshift;
		break;
	    case tRSHIFT:
		nc = NODECLASS_ShiftExpression_rshift;
		break;
	    case tURSHIFT:
		nc = NODECLASS_ShiftExpression_urshift;
		break;
	    default:
		return n;
	    }
	    sn = NEW_NODE(struct Binary_node, nc);
	    SKIP;
	    sn->a = n;
	    sn->b = PARSE(AdditiveExpression);
	    parser->is_lhs = 0;
	    n = (struct node *)sn;
	}
}

/*
 *	-- 11.8
 *
 *	RelationalExpression
 *	:	ShiftExpression
 *	|	RelationalExpression '<' ShiftExpression	 -- 11.8.1
 *	|	RelationalExpression '>' ShiftExpression	 -- 11.8.2
 *	|	RelationalExpression tLE ShiftExpression	 -- 11.8.3
 *	|	RelationalExpression tGT ShiftExpression	 -- 11.8.4
 *	|	RelationalExpression tINSTANCEOF ShiftExpression -- 11.8.6
 *	|	RelationalExpression tIN ShiftExpression	 -- 11.8.7
 *	;
 *
 *	RelationalExpressionNoIn
 *	:	ShiftExpression
 *	|	RelationalExpressionNoIn '<' ShiftExpression	 -- 11.8.1
 *	|	RelationalExpressionNoIn '>' ShiftExpression	 -- 11.8.2
 *	|	RelationalExpressionNoIn tLE ShiftExpression	 -- 11.8.3
 *	|	RelationalExpressionNoIn tGT ShiftExpression	 -- 11.8.4
 *	|	RelationalExpressionNoIn tINSTANCEOF ShiftExpression -- 11.8.6
 *	;
 *
 * The *NoIn productions are implemented by the 'noin' boolean field
 * in the parser state.
 */

/* 
 * 11.8.5 Abstract relational comparison function.
 */
static void
RelationalExpression_sub(interp, x, y, res)
	struct SEE_interpreter *interp;
	struct SEE_value *x, *y, *res;
{
	struct SEE_value r1, r2, r4, r5;
	struct SEE_value hint;
	int k;

	SEE_SET_OBJECT(&hint, interp->Number);

	SEE_ToPrimitive(interp, x, &hint, &r1);
	SEE_ToPrimitive(interp, y, &hint, &r2);
	if (!(SEE_VALUE_GET_TYPE(&r1) == SEE_STRING && 
	      SEE_VALUE_GET_TYPE(&r2) == SEE_STRING)) 
	{
	    SEE_ToNumber(interp, &r1, &r4);
	    SEE_ToNumber(interp, &r2, &r5);
	    if (SEE_NUMBER_ISNAN(&r4) || SEE_NUMBER_ISNAN(&r5))
		SEE_SET_UNDEFINED(res);
	    else if (r4.u.number == r5.u.number)
		SEE_SET_BOOLEAN(res, 0);
	    else if (SEE_NUMBER_ISPINF(&r4))
		SEE_SET_BOOLEAN(res, 0);
	    else if (SEE_NUMBER_ISPINF(&r5))
		SEE_SET_BOOLEAN(res, 1);
	    else if (SEE_NUMBER_ISNINF(&r5))
		SEE_SET_BOOLEAN(res, 0);
	    else if (SEE_NUMBER_ISNINF(&r4))
		SEE_SET_BOOLEAN(res, 1);
	    else 
	        SEE_SET_BOOLEAN(res, r4.u.number < r5.u.number);
	} else {
	    for (k = 0; 
		 k < r1.u.string->length && k < r2.u.string->length;
		 k++)
		if (r1.u.string->data[k] != r2.u.string->data[k])
			break;
	    if (k == r2.u.string->length)
		SEE_SET_BOOLEAN(res, 0);
	    else if (k == r1.u.string->length)
		SEE_SET_BOOLEAN(res, 1);
	    else
		SEE_SET_BOOLEAN(res, r1.u.string->data[k] < 
				 r2.u.string->data[k]);
	}
}

/* 11.8.1 < */
#if WITH_PARSER_EVAL
static void
RelationalExpression_lt_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	RelationalExpression_sub(context->interpreter, &r2, &r4, res);
	if (SEE_VALUE_GET_TYPE(res) == SEE_UNDEFINED)
		SEE_SET_BOOLEAN(res, 0);
}
#endif

#if WITH_PARSER_CODEGEN
static void
RelationalExpression_lt_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_LT();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif



/* 11.8.2 > */
#if WITH_PARSER_EVAL
static void
RelationalExpression_gt_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	RelationalExpression_sub(context->interpreter, &r4, &r2, res);
	if (SEE_VALUE_GET_TYPE(res) == SEE_UNDEFINED)
		SEE_SET_BOOLEAN(res, 0);
}
#endif

#if WITH_PARSER_CODEGEN
static void
RelationalExpression_gt_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_GT();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




/* 11.8.3 <= */
#if WITH_PARSER_EVAL
static void
RelationalExpression_le_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4, r5;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	RelationalExpression_sub(context->interpreter, &r4, &r2, &r5);
	if (SEE_VALUE_GET_TYPE(&r5) == SEE_UNDEFINED)
		SEE_SET_BOOLEAN(res, 0);
	else
		SEE_SET_BOOLEAN(res, !r5.u.boolean);
}
#endif

#if WITH_PARSER_CODEGEN
static void
RelationalExpression_le_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_LE();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




/* 11.8.4 >= */
#if WITH_PARSER_EVAL
static void
RelationalExpression_ge_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4, r5;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	RelationalExpression_sub(context->interpreter, &r2, &r4, &r5);
	if (SEE_VALUE_GET_TYPE(&r5) == SEE_UNDEFINED)
		SEE_SET_BOOLEAN(res, 0);
	else
		SEE_SET_BOOLEAN(res, !r5.u.boolean);
}
#endif

#if WITH_PARSER_CODEGEN
static void
RelationalExpression_ge_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_GE();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




/* 11.8.6 */
#if WITH_PARSER_EVAL
static void
RelationalExpression_instanceof_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_value r1, r2, r3, r4;
	int r7;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	if (SEE_VALUE_GET_TYPE(&r4) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(instanceof_not_object));
	r7 = SEE_object_instanceof(interp, &r2, r4.u.object);
	SEE_SET_BOOLEAN(res, r7);
}
#endif

#if WITH_PARSER_CODEGEN
static void
RelationalExpression_instanceof_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_INSTANCEOF();	      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




/* 11.8.7 */
#if WITH_PARSER_EVAL
static void
RelationalExpression_in_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_value r1, r2, r3, r4, r6;
	int r7;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	if (SEE_VALUE_GET_TYPE(&r4) != SEE_OBJECT)
		SEE_error_throw_string(interp, interp->TypeError,
		    STR(in_not_object));
	SEE_ToString(interp, &r2, &r6);
	r7 = SEE_OBJECT_HASPROPERTY(interp, r4.u.object, 
		SEE_intern(interp, r6.u.string));
	SEE_SET_BOOLEAN(res, r7);
}
#endif

#if WITH_PARSER_CODEGEN
static void
RelationalExpression_in_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	/* Binary_common_codegen(n, cc); */
	CODEGEN(n->a);			/* aref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();		/* aval */
	if (!CG_IS_STRING(n->a))
	    CG_TOSTRING();		/* astr */
	CODEGEN(n->b);			/* astr bref */
	if (!CG_IS_VALUE(n->b))
	    CG_GETVALUE();		/* aval bval */
	CG_IN();		        /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




static struct node *
RelationalExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	enum nodeclass_enum nc;
	struct Binary_node *rn;

	n = PARSE(ShiftExpression);
	for (;;) {
	    /* Left associative */
	    switch (NEXT) {
	    case '<':
		nc = NODECLASS_RelationalExpression_lt;
		break;
	    case '>':
		nc = NODECLASS_RelationalExpression_gt;
		break;
	    case tLE:
		nc = NODECLASS_RelationalExpression_le;
		break;
	    case tGE:
		nc = NODECLASS_RelationalExpression_ge;
		break;
	    case tINSTANCEOF:
		nc = NODECLASS_RelationalExpression_instanceof;
		break;
	    case tIN:
		if (!parser->noin) {
		    nc = NODECLASS_RelationalExpression_in;
		    break;
		} /* else Fallthrough */
	    default:
		return n;
	    }
	    rn = NEW_NODE(struct Binary_node, nc);
	    SKIP;
	    rn->a = n;
	    rn->b = PARSE(RelationalExpression);
	    parser->is_lhs = 0;
	    n = (struct node *)rn;
	}
}

/*
 *	-- 11.9
 *
 *	EqualityExpression
 *	:	RelationalExpression
 *	|	EqualityExpression tEQ RelationalExpression	-- 11.9.1
 *	|	EqualityExpression tNE RelationalExpression	-- 11.9.2
 *	|	EqualityExpression tSEQ RelationalExpression	-- 11.9.4
 *	|	EqualityExpression tSNE RelationalExpression	-- 11.9.5
 *	;
 *
 *	EqualityExpressionNoIn
 *	:	RelationalExpressionNoIn
 *	|	EqualityExpressionNoIn tEQ RelationalExpressionNoIn  -- 11.9.1
 *	|	EqualityExpressionNoIn tNE RelationalExpressionNoIn  -- 11.9.2
 *	|	EqualityExpressionNoIn tSEQ RelationalExpressionNoIn -- 11.9.4
 *	|	EqualityExpressionNoIn tSNE RelationalExpressionNoIn -- 11.9.5
 *	;
 */

/* 
 * 11.9.3 Abstract equality function.
 */
static void
EqualityExpression_eq(interp, x, y, res)
	struct SEE_interpreter *interp;
	struct SEE_value *x, *y, *res;
{
	struct SEE_value tmp;
	int xtype, ytype;

	if (SEE_VALUE_GET_TYPE(x) == SEE_VALUE_GET_TYPE(y))
	    switch (SEE_VALUE_GET_TYPE(x)) {
	    case SEE_UNDEFINED:
	    case SEE_NULL:
		SEE_SET_BOOLEAN(res, 1);
		return;
	    case SEE_NUMBER:
		if (SEE_NUMBER_ISNAN(x) || SEE_NUMBER_ISNAN(y))
		    SEE_SET_BOOLEAN(res, 0);
		else
		    SEE_SET_BOOLEAN(res, x->u.number == y->u.number);
		return;
	    case SEE_STRING:
		SEE_SET_BOOLEAN(res, SEE_string_cmp(x->u.string,
		    y->u.string) == 0);
		return;
	    case SEE_BOOLEAN:
		SEE_SET_BOOLEAN(res, !x->u.boolean == !y->u.boolean);
		return;
	    case SEE_OBJECT:
		SEE_SET_BOOLEAN(res, 
			SEE_OBJECT_JOINED(x->u.object, y->u.object));
		return;
	    default:
		SEE_ASSERT(interp, !"unexpected token");
	    }
	xtype = SEE_VALUE_GET_TYPE(x);
	ytype = SEE_VALUE_GET_TYPE(y);
	if (xtype == SEE_NULL && ytype == SEE_UNDEFINED)
		SEE_SET_BOOLEAN(res, 1);
	else if (xtype == SEE_UNDEFINED && ytype == SEE_NULL)
		SEE_SET_BOOLEAN(res, 1);
	else if (xtype == SEE_NUMBER && ytype == SEE_STRING) {
		SEE_ToNumber(interp, y, &tmp);
		EqualityExpression_eq(interp, x, &tmp, res);
	} else if (xtype == SEE_STRING && ytype == SEE_NUMBER) {
		SEE_ToNumber(interp, x, &tmp);
		EqualityExpression_eq(interp, &tmp, y, res);
	} else if (xtype == SEE_BOOLEAN) {
		SEE_ToNumber(interp, x, &tmp);
		EqualityExpression_eq(interp, &tmp, y, res);
	} else if (ytype == SEE_BOOLEAN) {
		SEE_ToNumber(interp, y, &tmp);
		EqualityExpression_eq(interp, x, &tmp, res);
	} else if ((xtype == SEE_STRING || xtype == SEE_NUMBER) &&
		    ytype == SEE_OBJECT) {
		SEE_ToPrimitive(interp, y, x, &tmp);
		EqualityExpression_eq(interp, x, &tmp, res);
	} else if ((ytype == SEE_STRING || ytype == SEE_NUMBER) &&
		    xtype == SEE_OBJECT) {
		SEE_ToPrimitive(interp, x, y, &tmp);
		EqualityExpression_eq(interp, &tmp, y, res);
	} else
		SEE_SET_BOOLEAN(res, 0);
}

/*
 * 19.9.6 Strict equality function
 */
#if WITH_PARSER_EVAL
static void
EqualityExpression_seq(context, x, y, res)
	struct SEE_context *context;
	struct SEE_value *x, *y, *res;
{
	if (SEE_VALUE_GET_TYPE(x) != SEE_VALUE_GET_TYPE(y))
	    SEE_SET_BOOLEAN(res, 0);
	else
	    switch (SEE_VALUE_GET_TYPE(x)) {
	    case SEE_UNDEFINED:
		SEE_SET_BOOLEAN(res, 1);
		break;
	    case SEE_NULL:
		SEE_SET_BOOLEAN(res, 1);
		break;
	    case SEE_NUMBER:
		if (SEE_NUMBER_ISNAN(x) || SEE_NUMBER_ISNAN(y))
			SEE_SET_BOOLEAN(res, 0);
		else
			SEE_SET_BOOLEAN(res, x->u.number == y->u.number);
		break;
	    case SEE_STRING:
		SEE_SET_BOOLEAN(res, SEE_string_cmp(x->u.string, 
		    y->u.string) == 0);
		break;
	    case SEE_BOOLEAN:
		SEE_SET_BOOLEAN(res, !x->u.boolean == !y->u.boolean);
		break;
	    case SEE_OBJECT:
		SEE_SET_BOOLEAN(res, 
			SEE_OBJECT_JOINED(x->u.object, y->u.object));
		break;
	    default:
		SEE_SET_BOOLEAN(res, 0);
	    }
}

static void
EqualityExpression_eq_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	EqualityExpression_eq(context->interpreter, &r4, &r2, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
EqualityExpression_eq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_EQ();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif



#if WITH_PARSER_EVAL
static void
EqualityExpression_ne_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4, t;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	EqualityExpression_eq(context->interpreter, &r4, &r2, &t);
	SEE_SET_BOOLEAN(res, !t.u.boolean);
}
#endif

#if WITH_PARSER_CODEGEN
static void
EqualityExpression_ne_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_EQ();		      /* bool */
	CG_NOT();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




#if WITH_PARSER_EVAL
static void
EqualityExpression_seq_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	EqualityExpression_seq(context, &r4, &r2, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
EqualityExpression_seq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_SEQ();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




#if WITH_PARSER_EVAL
static void
EqualityExpression_sne_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4, r5;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	EqualityExpression_seq(context, &r4, &r2, &r5);
	SEE_SET_BOOLEAN(res, !r5.u.boolean);
}
#endif

#if WITH_PARSER_CODEGEN
static void
EqualityExpression_sne_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_SEQ();		      /* bool */
	CG_NOT();		      /* bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




static struct node *
EqualityExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	enum nodeclass_enum nc;
	struct Binary_node *rn;

	n = PARSE(RelationalExpression);
	for (;;) {
	    /* Left associative */
	    switch (NEXT) {
	    case tEQ:
		nc = NODECLASS_EqualityExpression_eq;
		break;
	    case tNE:
		nc = NODECLASS_EqualityExpression_ne;
		break;
	    case tSEQ:
		nc = NODECLASS_EqualityExpression_seq;
		break;
	    case tSNE:
		nc = NODECLASS_EqualityExpression_sne;
		break;
	    default:
		return n;
	    }
	    rn = NEW_NODE(struct Binary_node, nc);
	    SKIP;
	    rn->a = n;
	    rn->b = PARSE(EqualityExpression);
	    parser->is_lhs = 0;
	    n = (struct node *)rn;
	}
}

/*
 *	-- 11.10
 *
 *	BitwiseANDExpression
 *	:	EqualityExpression
 *	|	BitwiseANDExpression '&' EqualityExpression
 *	;
 *
 *	BitwiseANDExpressionNoIn
 *	:	EqualityExpressionNoIn
 *	|	BitwiseANDExpressionNoIn '&' EqualityExpressionNoIn
 *	;
 */

/* 11.10 */
#if WITH_PARSER_EVAL
static void
BitwiseANDExpression_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	SEE_int32_t r5, r6;

	r5 = SEE_ToInt32(context->interpreter, r2);
	r6 = SEE_ToInt32(context->interpreter, r4);
	SEE_SET_NUMBER(res, r5 & r6);
}

static void
BitwiseANDExpression_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	BitwiseANDExpression_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
BitwiseANDExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_BAND();		      /* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif



static struct node *
BitwiseANDExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *m;

	n = PARSE(EqualityExpression);
	if (NEXT != '&') 
		return n;
	m = NEW_NODE(struct Binary_node,
			NODECLASS_BitwiseANDExpression);
	SKIP;
	m->a = n;
	m->b = PARSE(BitwiseANDExpression);
	parser->is_lhs = 0;
	return (struct node *)m;
}

/*
 *	BitwiseXORExpression
 *	:	BitwiseANDExpression
 *	|	BitwiseXORExpression '^' BitwiseANDExpression
 *	;
 *
 *	BitwiseXORExpressionNoIn
 *	:	BitwiseANDExpressionNoIn
 *	|	BitwiseXORExpressionNoIn '^' BitwiseANDExpressionNoIn
 *	;
 */

/* 11.10 */
#if WITH_PARSER_EVAL
static void
BitwiseXORExpression_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	SEE_int32_t r5, r6;

	r5 = SEE_ToInt32(context->interpreter, r2);
	r6 = SEE_ToInt32(context->interpreter, r4);
	SEE_SET_NUMBER(res, r5 ^ r6);
}

static void
BitwiseXORExpression_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	BitwiseXORExpression_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
BitwiseXORExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_BXOR();		      /* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




static struct node *
BitwiseXORExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *m;

	n = PARSE(BitwiseANDExpression);
	if (NEXT != '^') 
		return n;
	m = NEW_NODE(struct Binary_node,
			NODECLASS_BitwiseXORExpression);
	SKIP;
	m->a = n;
	m->b = PARSE(BitwiseXORExpression);
	parser->is_lhs = 0;
	return (struct node *)m;
}

/*
 *	BitwiseORExpression
 *	:	BitwiseXORExpression
 *	|	BitwiseORExpression '|' BitwiseXORExpression
 *	;
 *
 *	BitwiseORExpressionNoIn
 *	:	BitwiseXORExpressionNoIn
 *	|	BitwiseORExpressionNoIn '|' BitwiseXORExpressionNoIn
 *	;
 */

/* 11.10 */
#if WITH_PARSER_EVAL
static void
BitwiseORExpression_common(r2, r4, context, res)
	struct SEE_value *r2, *r4, *res;
	struct SEE_context *context;
{
	SEE_int32_t r5, r6;

	r5 = SEE_ToInt32(context->interpreter, r2);
	r6 = SEE_ToInt32(context->interpreter, r4);
	SEE_SET_NUMBER(res, r5 | r6);
}

static void
BitwiseORExpression_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, &r4);
	BitwiseORExpression_common(&r2, &r4, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
BitwiseORExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	Binary_common_codegen(n, cc); /* aval bval */
	CG_BOR();		      /* num */

	n->node.is = CG_TYPE_NUMBER;
	n->node.maxstack = MAX(n->a->maxstack, 1 + n->b->maxstack);
}
#endif




static struct node *
BitwiseORExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *m;

	n = PARSE(BitwiseXORExpression);
	if (NEXT != '|') 
		return n;
	m = NEW_NODE(struct Binary_node,
			NODECLASS_BitwiseORExpression);
	SKIP;
	m->a = n;
	m->b = PARSE(BitwiseORExpression);
	parser->is_lhs = 0;
	return (struct node *)m;
}

/*
 *	-- 11.11
 *
 *	LogicalANDExpression
 *	:	BitwiseORExpression
 *	|	LogicalANDExpression tANDAND BitwiseORExpression
 *	;
 *
 *	LogicalANDExpressionNoIn
 *	:	BitwiseORExpressionNoIn
 *	|	LogicalANDExpressionNoIn tANDAND BitwiseORExpressionNoIn
 *	;
 */

#if WITH_PARSER_EVAL
static void
LogicalANDExpression_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r3, r5;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, res);
	SEE_ToBoolean(context->interpreter, res, &r3);
	if (!r3.u.boolean)
		return;
	EVAL(n->b, context, &r5);
	GetValue(context, &r5, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
LogicalANDExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	SEE_code_patchable_t L1, L2;

	CODEGEN(n->a);				/* ref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();			/* val */
	if (!CG_IS_BOOLEAN(n->a))
	    CG_TOBOOLEAN();			/* bool */
	CG_B_TRUE_f(L1);			/* -  (L1)*/
	CG_FALSE();				/* false */
	CG_B_ALWAYS_f(L2);			/* false (2) */

	CG_LABEL(L1);				/* 1: - */
	CODEGEN(n->b);				/* ref */
	if (!CG_IS_VALUE(n->b))
	    CG_GETVALUE();			/* val */
	if (!CG_IS_BOOLEAN(n->b))
	    CG_TOBOOLEAN();			/* bool */
	CG_LABEL(L2);				/* 2: bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, n->b->maxstack);
}
#endif


/* Executes a known-constant subtree to yield its value. */
static void
const_evaluate(node, interp, res)
	struct node *node;
	struct SEE_interpreter *interp;
	struct SEE_value *res;
{
	void *body;
	struct SEE_context const_context;

#ifndef NDEBUG
	if (SEE_parse_debug) {
	    dprintf("const_evaluate: evaluating (");
#if WITH_PARSER_PRINT
            _SEE_parser_print(
                _SEE_parser_print_stdio_new(interp, stderr),
                node);
#else
	    dprintf("%p", node);
#endif
	    dprintf(")\n");
	}
#endif
	
	body = make_body(interp, 
	    FunctionBody_make(interp, 
	      SourceElements_make1(interp, 
	        ExpressionStatement_make(interp, node)), 1),
	    NO_CONST);

	/* A dummy context with the minimum we can get away with */
	memset(&const_context, 0, sizeof const_context);
	const_context.interpreter = interp;

	eval_functionbody(body, &const_context, res);

#ifndef NDEBUG
	if (SEE_parse_debug) {
	    dprintf("const_evaluate: result is ");
	    dprintv(interp, res);
	    dprintf("\n");
	}
#endif
}

static int
LogicalANDExpression_isconst(na, interp)
	struct node *na; /* (struct Binary_node) */
	struct SEE_interpreter *interp;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	if (ISCONST(n->a, interp)) {
		struct SEE_value r1, r3;
		const_evaluate(n->a, interp, &r1);
		SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(&r1) != SEE_REFERENCE);
		SEE_ToBoolean(interp, &r1, &r3);
		return r3.u.boolean ? ISCONST(n->b, interp) : 1;
	} else
		return 0;
}


static struct node *
LogicalANDExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *m;

	n = PARSE(BitwiseORExpression);
	if (NEXT != tANDAND) 
		return n;
	m = NEW_NODE(struct Binary_node,
			NODECLASS_LogicalANDExpression);
	SKIP;
	m->a = n;
	m->b = PARSE(LogicalANDExpression);
	parser->is_lhs = 0;
	return (struct node *)m;
}

/*
 *	LogicalORExpression
 *	:	LogicalANDExpression
 *	|	LogicalORExpression tOROR LogicalANDExpression
 *	;
 *
 *	LogicalORExpressionNoIn
 *	:	LogicalANDExpressionNoIn
 *	|	LogicalORExpressionNoIn tOROR LogicalANDExpressionNoIn
 *	;
 */

#if WITH_PARSER_EVAL
static void
LogicalORExpression_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r3, r5;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, res);
	SEE_ToBoolean(context->interpreter, res, &r3);
	if (r3.u.boolean)
		return;
	EVAL(n->b, context, &r5);
	GetValue(context, &r5, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
LogicalORExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	SEE_code_patchable_t L1, L2;

	CODEGEN(n->a);				/* ref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();			/* val */
	if (!CG_IS_BOOLEAN(n->a))
	    CG_TOBOOLEAN();			/* bool */
	CG_B_TRUE_f(L1);		 	/* -  (1)*/

	CODEGEN(n->b);				/* ref */
	if (!CG_IS_VALUE(n->b))
	    CG_GETVALUE();			/* val */
	if (!CG_IS_BOOLEAN(n->b))
	    CG_TOBOOLEAN();			/* bool */

	CG_B_ALWAYS_f(L2);

    CG_LABEL(L1);				/* 1: - */
	CG_TRUE();				/* true */
    CG_LABEL(L2);				/* 2: bool */

	n->node.is = CG_TYPE_BOOLEAN;
	n->node.maxstack = MAX(n->a->maxstack, n->b->maxstack);
}
#endif


static int
LogicalORExpression_isconst(na, interp)
	struct node *na; /* (struct Binary_node) */
	struct SEE_interpreter *interp;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	if (ISCONST(n->a, interp)) {
		struct SEE_value r1, r3;
		const_evaluate(n->a, interp, &r1);
		SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(&r1) != SEE_REFERENCE);
		SEE_ToBoolean(interp, &r1, &r3);
		return r3.u.boolean ? 1: ISCONST(n->b, interp);
	} else
		return 0;
}



static struct node *
LogicalORExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *m;

	n = PARSE(LogicalANDExpression);
	if (NEXT != tOROR) 
		return n;
	m = NEW_NODE(struct Binary_node,
			NODECLASS_LogicalORExpression);
	SKIP;
	m->a = n;
	m->b = PARSE(LogicalORExpression);
	parser->is_lhs = 0;
	return (struct node *)m;
}

/*
 *	-- 11.12
 *
 *	ConditionalExpression
 *	:	LogicalORExpression
 *	|	LogicalORExpression '?' 
 *			AssignmentExpression ':' AssignmentExpression
 *	;
 *
 *	ConditionalExpressionNoIn
 *	:	LogicalORExpressionNoIn
 *	|	LogicalORExpressionNoIn '?' 
 *			AssignmentExpressionNoIn ':' AssignmentExpressionNoIn
 *	;
 */

#if WITH_PARSER_EVAL
static void
ConditionalExpression_eval(na, context, res)
	struct node *na; /* (struct ConditionalExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct ConditionalExpression_node *n = 
		CAST_NODE(na, ConditionalExpression);
	struct SEE_value r1, r2, r3, t;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToBoolean(context->interpreter, &r2, &r3);
	if (r3.u.boolean)
		EVAL(n->b, context, &t);
	else
		EVAL(n->c, context, &t);
	GetValue(context, &t, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ConditionalExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct ConditionalExpression_node *n = 
		CAST_NODE(na, ConditionalExpression);
	SEE_code_patchable_t L1, L2;

	CODEGEN(n->a);				/*     ref      */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();			/*     val      */
	if (!CG_IS_BOOLEAN(n->a))
	    CG_TOBOOLEAN();			/*     bool     */
	CG_B_TRUE_f(L1);			/*     -    (1) */

	/* The false branch */
	CODEGEN(n->c);				/*     ref      */
	if (!CG_IS_VALUE(n->c))
	    CG_GETVALUE();			/*     val      */
	CG_B_ALWAYS_f(L2);			/*     val  (2) */

	/* The true branch */
	CG_LABEL(L1);				/* 1:  -        */
	CODEGEN(n->b);				/*     ref      */
	if (!CG_IS_VALUE(n->b))
	    CG_GETVALUE();			/*     val      */

	CG_LABEL(L2);				/* 2:  val      */

	if (!CG_IS_VALUE(n->b) || !CG_IS_VALUE(n->c))
	    n->node.is = CG_TYPE_VALUE;
	else
	    n->node.is = n->b->is | n->c->is;
	n->node.maxstack = MAX3(n->a->maxstack, n->b->maxstack, n->c->maxstack);
}
#endif



static int
ConditionalExpression_isconst(na, interp)
	struct node *na; /* (struct ConditionalExpression_node) */
	struct SEE_interpreter *interp;
{
	struct ConditionalExpression_node *n = 
		CAST_NODE(na, ConditionalExpression);
	if (ISCONST(n->a, interp)) {
		struct SEE_value r1, r3;
		const_evaluate(n->a, interp, &r1);
		SEE_ASSERT(interp, SEE_VALUE_GET_TYPE(&r1) != SEE_REFERENCE);
		SEE_ToBoolean(interp, &r1, &r3);
		return r3.u.boolean 
		    ? ISCONST(n->b, interp) 
		    : ISCONST(n->c, interp);
	} else
		return 0;
}

static struct node *
ConditionalExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct ConditionalExpression_node *m;

	n = PARSE(LogicalORExpression);
	if (NEXT != '?') 
		return n;
	m = NEW_NODE(struct ConditionalExpression_node,
			NODECLASS_ConditionalExpression);
	SKIP;
	m->a = n;
	m->b = PARSE(AssignmentExpression);
	EXPECT(':');
	m->c = PARSE(AssignmentExpression);
	parser->is_lhs = 0;
	return (struct node *)m;
}

/*
 *	-- 11.13
 *
 *	AssignmentExpression
 *	:	ConditionalExpression
 *	|	LeftHandSideExpression AssignmentOperator AssignmentExpression
 *	;
 *
 *	AssignmentExpressionNoIn
 *	:	ConditionalExpressionNoIn
 *	|	LeftHandSideExpression AssignmentOperator 
 *						AssignmentExpressionNoIn
 *	;
 *
 *	AssignmentOperator
 *	:	'='				-- 11.13.1
 *	|	tSTAREQ				-- 11.13.2
 *	|	tDIVEQ
 *	|	tMODEQ
 *	|	tPLUSEQ
 *	|	tMINUSEQ
 *	|	tLSHIFTEQ
 *	|	tRSHIFTEQ
 *	|	tURSHIFTEQ
 *	|	tANDEQ
 *	|	tXOREQ
 *	|	tOREQ
 *	;
 */


#if WITH_PARSER_EVAL
static void
AssignmentExpression_simple_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2;

	EVAL(n->lhs, context, &r1);
	EVAL(n->expr, context, &r2);
	GetValue(context, &r2, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_common_codegen_pre(n, cc)	/* - | ref num num */
	struct AssignmentExpression_node *n;
	struct code_context *cc;
{
	CODEGEN(n->lhs);	/* ref */
	CG_DUP();		/* ref ref */
	CG_GETVALUE();		/* ref val */
	CG_TONUMBER();		/* ref num */
	CODEGEN(n->expr);	/* ref num ref */
	if (!CG_IS_VALUE(n->expr))
	    CG_GETVALUE();	/* ref num val */
	if (!CG_IS_NUMBER(n->expr))
	    CG_TONUMBER();	/* ref num num */
}

static void
AssignmentExpression_common_codegen_shiftpre(n, cc)	/* - | ref val val */
	struct AssignmentExpression_node *n;
	struct code_context *cc;
{
	CODEGEN(n->lhs);	/* ref */
	CG_DUP();		/* ref ref */
	CG_GETVALUE();		/* ref val */
	CODEGEN(n->expr);	/* ref num ref */
	if (!CG_IS_VALUE(n->expr))
	    CG_GETVALUE();	/* ref num val */
}

static void
AssignmentExpression_common_codegen_post(n, cc) /* ref val | val */
	struct AssignmentExpression_node *n;
	struct code_context *cc;
{
	CG_DUP();		/* ref val val */
	CG_ROLL3();   		/* val ref val */
	CG_PUTVALUE();		/* val */
	n->node.maxstack = MAX(n->lhs->maxstack, 2 + n->expr->maxstack);

	/* Peephole optimisation note:
	 *                  ref val
	 *   DUP	    ref val val
	 *   ROLL3   	    val ref val
	 *   PUTVALUE	    val
	 *   POP	    -
	 *
	 * is equivalent to:
	 *                  ref val
	 *   PUTVALUE       -
	 * 
	 * The backend could check for this when the 'POP' instruction
	 * is generated.
	 */

	/* Peephole optimisation note:
	 *                  ref val
	 *   DUP	    ref val val
	 *   ROLL3   	    val ref val
	 *   PUTVALUE	    val
	 *   SETC	    -
	 *
	 * is equivalent to:
	 *                  ref val
	 *   DUP	    ref val val
	 *   SETC	    ref val
	 *   PUTVALUE       -
	 * 
	 * The backend could check for this when the 'SETC' instruction
	 * is generated.
	 */

}


static void
AssignmentExpression_simple_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	CODEGEN(n->lhs);	/* ref */
	CODEGEN(n->expr);	/* ref ref */
	if (!CG_IS_VALUE(n->expr))
	    CG_GETVALUE();	/* ref val */
	AssignmentExpression_common_codegen_post(n, cc);/* val */
	n->node.is = !CG_IS_VALUE(n->expr) ?  CG_TYPE_VALUE : n->expr->is;
}
#endif



/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_muleq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	MultiplicativeExpression_mul_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_muleq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_pre(n, cc);	/* ref num num */
	CG_MUL();
	AssignmentExpression_common_codegen_post(n, cc);/* val */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_diveq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	MultiplicativeExpression_div_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_diveq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_pre(n, cc);	/* ref num num */
	CG_DIV();					/* ref num */
	AssignmentExpression_common_codegen_post(n, cc);/* val */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_modeq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	MultiplicativeExpression_mod_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_modeq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_pre(n, cc);	/* ref num num */
	CG_MOD();					/* ref num */
	AssignmentExpression_common_codegen_post(n, cc);/* val */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_addeq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	AdditiveExpression_add_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_addeq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	CODEGEN(n->lhs);	/* ref1 */
	CG_DUP();		/* ref1 ref1 */
	CG_GETVALUE();		/* ref1 val1 */
	CODEGEN(n->expr);	/* ref1 val1 ref2 */
	if (!CG_IS_VALUE(n->expr))
	    CG_GETVALUE();	/* ref1 val1 val2 */
	CG_EXCH();		/* ref1 val2 val1 */
	CG_TOPRIMITIVE();	/* ref1 val2 prim1 */
	CG_EXCH();		/* ref1 prim1 val2 */
	if (!CG_IS_PRIMITIVE(n->expr))
	    CG_TOPRIMITIVE();	/* ref1 prim1 prim2 */
	CG_ADD();		/* ref1 prim3 */
	AssignmentExpression_common_codegen_post(n, cc);/* prim3 */

	if (CG_IS_STRING(n->expr))
	    n->node.is = CG_TYPE_STRING;
	else 
	    n->node.is = CG_TYPE_STRING | CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_subeq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	AdditiveExpression_sub_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_subeq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_pre(n, cc);	/* ref num num */
	CG_SUB();
	AssignmentExpression_common_codegen_post(n, cc);/* val */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_lshifteq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	ShiftExpression_lshift_common(&r2, n->expr, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_lshifteq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_shiftpre(n, cc);/* ref val val */
	CG_LSHIFT();					    /* ref num */
	AssignmentExpression_common_codegen_post(n, cc);    /* num */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_rshifteq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	ShiftExpression_rshift_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_rshifteq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_shiftpre(n, cc);/* ref val val */
	CG_RSHIFT();					    /* ref num */
	AssignmentExpression_common_codegen_post(n, cc);    /* num */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_urshifteq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	ShiftExpression_urshift_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_urshifteq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_shiftpre(n, cc);/* ref val val */
	CG_URSHIFT();					    /* ref num */
	AssignmentExpression_common_codegen_post(n, cc);    /* num */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_andeq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	BitwiseANDExpression_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_andeq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_pre(n, cc);	    /* ref num num */
	CG_BAND();					    /* ref num */
	AssignmentExpression_common_codegen_post(n, cc);    /* num */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_xoreq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	BitwiseXORExpression_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_xoreq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_pre(n, cc);	    /* ref num num */
	CG_BXOR();					    /* ref num */
	AssignmentExpression_common_codegen_post(n, cc);    /* num */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




/* 11.13.2 */
#if WITH_PARSER_EVAL
static void
AssignmentExpression_oreq_eval(na, context, res)
	struct node *na; /* (struct AssignmentExpression_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	struct SEE_value r1, r2, r3, r4;

	EVAL(n->lhs, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->expr, context, &r3);
	GetValue(context, &r3, &r4);
	BitwiseORExpression_common(&r2, &r4, context, res);
	PutValue(context, &r1, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
AssignmentExpression_oreq_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);

	AssignmentExpression_common_codegen_pre(n, cc);	    /* ref num num */
	CG_BOR();					    /* ref num */
	AssignmentExpression_common_codegen_post(n, cc);    /* num */
	n->node.is = CG_TYPE_NUMBER;
}
#endif




static struct node *
AssignmentExpression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	enum nodeclass_enum nc;
	struct AssignmentExpression_node *an;

	/*
	 * If, while recursing we parse LeftHandSideExpression,
	 * then is_lhs will be set to 1. Otherwise, it is just a 
	 * ConditionalExpression, and we cannot derive the second
	 * production in this rule. So we just return.
	 */
	n = PARSE(ConditionalExpression);
	if (!parser->is_lhs)
		return n;

	switch (NEXT) {
	case '=':
		nc = NODECLASS_AssignmentExpression_simple;
		break;
	case tSTAREQ:
		nc = NODECLASS_AssignmentExpression_muleq;
		break;
	case tDIVEQ:
		nc = NODECLASS_AssignmentExpression_diveq;
		break;
	case tMODEQ:
		nc = NODECLASS_AssignmentExpression_modeq;
		break;
	case tPLUSEQ:
		nc = NODECLASS_AssignmentExpression_addeq;
		break;
	case tMINUSEQ:
		nc = NODECLASS_AssignmentExpression_subeq;
		break;
	case tLSHIFTEQ:
		nc = NODECLASS_AssignmentExpression_lshifteq;
		break;
	case tRSHIFTEQ:
		nc = NODECLASS_AssignmentExpression_rshifteq;
		break;
	case tURSHIFTEQ:
		nc = NODECLASS_AssignmentExpression_urshifteq;
		break;
	case tANDEQ:
		nc = NODECLASS_AssignmentExpression_andeq;
		break;
	case tXOREQ:
		nc = NODECLASS_AssignmentExpression_xoreq;
		break;
	case tOREQ:
		nc = NODECLASS_AssignmentExpression_oreq;
		break;
	default:
		return n;
	}
	an = NEW_NODE(struct AssignmentExpression_node, nc);
	an->lhs = n;
	SKIP;
	an->expr = PARSE(AssignmentExpression);
	parser->is_lhs = 0;
	return (struct node *)an;
}

/*
 *	-- 11.14
 *
 *	Expression
 *	:	AssignmentExpression
 *	|	Expression ',' AssignmentExpression
 *	;
 *
 *	ExpressionNoIn
 *	:	AssignmentExpressionNoIn
 *	|	ExpressionNoIn ',' AssignmentExpressionNoIn
 *	;
 *
 * Codgen notes:
 * All Expression nodes leave a val on the stack; i.e.   - | val
 *
 */

/* 11.14 */
#if WITH_PARSER_EVAL
static void
Expression_comma_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value r1, r2, r3;

	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	EVAL(n->b, context, &r3);
	GetValue(context, &r3, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
Expression_comma_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	CODEGEN(n->a);		/* ref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	/* val */
	CG_POP();		/* -   */
	CODEGEN(n->b);		/* ref */
	if (!CG_IS_VALUE(n->b))
	    CG_GETVALUE();	/* val */

	n->node.is = CG_IS_VALUE(n->b) ? n->b->is : CG_TYPE_VALUE;
	n->node.maxstack = MAX(n->a->maxstack, n->b->maxstack);
}
#endif


static struct node *
Expression_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *cn;

	n = PARSE(AssignmentExpression);
	if (NEXT != ',')
		return n;
	cn = NEW_NODE(struct Binary_node, NODECLASS_Expression_comma);
	SKIP;
	cn->a = n;
	cn->b = PARSE(Expression);
	parser->is_lhs = 0;
	return (struct node *)cn;
}

/*
 *
 * -- 12
 *
 *	Statement
 *	:	Block
 *	|	VariableStatement
 *	|	EmptyStatement
 *	|	ExpressionStatement
 *	|	IfStatement
 *	|	IterationStatement
 *	|	ContinueStatement
 *	|	BreakStatement
 *	|	ReturnStatement
 *	|	WithStatement
 *	|	LabelledStatement
 *	|	SwitchStatement
 *	|	ThrowStatement
 *	|	TryStatement
 *	;
 *
 * Codegen note:
 *   All statements expect to have a NORMAL completion value on the stack
 *   already. At the end of the statement, the value is replaced as needed.
 */

static struct node *
Statement_parse(parser)
	struct parser *parser;
{
	struct node *n;

	parser->current_labelset = NULL;

	switch (NEXT) {
	case '{':
		return PARSE(Block);
	case tVAR:
		return PARSE(VariableStatement);
	case ';':
		return PARSE(EmptyStatement);
	case tIF:
		return PARSE(IfStatement);
	case tDO:
	case tWHILE:
	case tFOR:
		n = PARSE(IterationStatement);
		return n;
	case tCONTINUE:
		return PARSE(ContinueStatement);
	case tBREAK:
		return PARSE(BreakStatement);
	case tRETURN:
		return PARSE(ReturnStatement);
	case tWITH:
		return PARSE(WithStatement);
	case tSWITCH:
		n = PARSE(SwitchStatement);
		return n;
	case tTHROW:
		return PARSE(ThrowStatement);
	case tTRY:
		return PARSE(TryStatement);
	case tFUNCTION:
		/* Conditional functions for JS1.5 compatibility */
		if (SEE_COMPAT_JS(parser->interpreter, >=, JS15) &&
		    lookahead(parser, 1) != '(')
			return PARSE(FunctionStatement);
		ERRORm("function keyword not allowed here");
	case tIDENT:
		if (lookahead(parser, 1) == ':')
			return PARSE(LabelledStatement);
		/* FALLTHROUGH */
	default:
		return PARSE(ExpressionStatement);
	}
}

/*
 *	-- 12.1
 *
 *	Block
 *	:	'{' '}'					-- 12.1
 *	|	'{' StatementList '}'			-- 12.1
 *	;
 */

/* 12.1 */
#if WITH_PARSER_EVAL
static void
Block_empty_eval(n, context, res)
	struct node *n;
	struct SEE_context *context;
	struct SEE_value *res;
{
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, NULL, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
Block_empty_codegen(n, cc)
	struct node *n;
	struct code_context *cc;
{
						/* - | - */
	n->maxstack = 0;
}
#endif


static struct node *
Block_parse(parser)
	struct parser *parser;
{
	struct node *n;

	EXPECT('{');
	if (NEXT == '}')
		n = NEW_NODE(struct node, NODECLASS_Block_empty);
	else
		n = PARSE(StatementList);
	EXPECT('}');
	return n;
}

/*
 *	StatementList
 *	:	Statement				-- 12.1
 *	|	StatementList Statement			-- 12.1
 *	;
 */

/* 12.1 */
#if WITH_PARSER_EVAL
static void
StatementList_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	struct SEE_value *val;

	EVAL(n->a, context, res);
	if (res->u.completion.type == SEE_COMPLETION_NORMAL) {
		val = res->u.completion.value;
		EVAL(n->b, context, res);
		if (res->u.completion.value == NULL)
			res->u.completion.value = val;
		else 
			SEE_free(context->interpreter, (void **)&val);
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
StatementList_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	CODEGEN(n->a);				 /*    -      */
	CODEGEN(n->b);				 /*    -      */
	n->node.maxstack = MAX(n->a->maxstack, n->b->maxstack);
}
#endif

static struct node *
StatementList_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *ln;

	n = PARSE(Statement);
	switch (NEXT) {
	case tFUNCTION:
	    if (SEE_COMPAT_JS(parser->interpreter, >=, JS15))
		break;
	    /* else fallthrough */
	case '}':
	case tEND:
	case tCASE:
	case tDEFAULT:
		return n;
	}
	ln = NEW_NODE(struct Binary_node, NODECLASS_StatementList);
	ln->a = n;
	ln->b = PARSE(StatementList);
	return (struct node *)ln;
}

/*
 *	-- 12.2
 *
 *	VariableStatement
 *	:	tVAR VariableDeclarationList ';'
 *	;
 *
 *	VariableDeclarationList
 *	:	VariableDeclaration
 *	|	VariableDeclarationList ',' VariableDeclaration
 *	;
 *
 *	VariableDeclarationListNoIn
 *	:	VariableDeclarationNoIn
 *	|	VariableDeclarationListNoIn ',' VariableDeclarationNoIn
 *	;
 *
 *	VariableDeclaration
 *	:	tIDENT
 *	|	tIDENT Initialiser
 *	;
 *
 *	VariableDeclarationNoIn
 *	:	tIDENT
 *	|	tIDENT InitialiserNoIn
 *	;
 *
 *	Initialiser
 *	:	'=' AssignmentExpression
 *	;
 *
 *	InitialiserNoIn
 *	:	'=' AssignmentExpressionNoIn
 *	;
 */

#if WITH_PARSER_EVAL
static void
VariableStatement_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value v;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->a, context, &v);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, NULL, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
VariableStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	/* Note: VariableDeclaration leaves nothing on the stack */
	CG_LOC(&na->location);
	CODEGEN(n->a);	    /* - */
	n->node.maxstack = n->a->maxstack;
}
#endif


static struct node *
VariableStatement_parse(parser)
	struct parser *parser;
{
	struct Unary_node *n;

	n = NEW_NODE(struct Unary_node, NODECLASS_VariableStatement);
	EXPECT(tVAR);
	n->a = PARSE(VariableDeclarationList);
	EXPECT_SEMICOLON;
	return (struct node *)n;
}


/* 12.2 */
#if WITH_PARSER_EVAL
static void
VariableDeclarationList_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;		/* unused */
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	EVAL(n->a, context, res);
	EVAL(n->b, context, res);
}
#endif

#if WITH_PARSER_CODEGEN
static void
VariableDeclarationList_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);

	CODEGEN(n->a);			/* - */
	CODEGEN(n->b);			/* - */
	n->node.maxstack = MAX(n->a->maxstack, n->b->maxstack);
}
#endif


static struct node *
VariableDeclarationList_parse(parser)
	struct parser *parser;
{
	struct node *n;
	struct Binary_node *ln;

	n = PARSE(VariableDeclaration);
	if (NEXT != ',') 
		return n;
	ln = NEW_NODE(struct Binary_node, NODECLASS_VariableDeclarationList);
	SKIP;
	/* NB: IterationStatement_parse() also constructs a VarDeclList */
	ln->a = n;
	ln->b = PARSE(VariableDeclarationList);
	return (struct node *)ln;
}


#if WITH_PARSER_EVAL
static void
VariableDeclaration_eval(na, context, res)
	struct node *na; /* (struct VariableDeclaration_node) */
	struct SEE_context *context;
	struct SEE_value *res;		/* unused */
{
	struct VariableDeclaration_node *n = 
		CAST_NODE(na, VariableDeclaration);
	struct SEE_value r1, r2, r3;

	if (n->init) {
		SEE_scope_lookup(context->interpreter, context->scope, 
			n->var->name, &r1);
		EVAL(n->init, context, &r2);
		GetValue(context, &r2, &r3);
		PutValue(context, &r1, &r3);
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
VariableDeclaration_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct VariableDeclaration_node *n = 
		CAST_NODE(na, VariableDeclaration);
	if (n->init) {
		if (cg_var_is_in_scope(cc, n->var->name)) 
		    CG_VREF(cg_var_id(cc, n->var->name));    /* ref */
		else {
		    CG_STRING(n->var->name);		    /* str */
		    CG_LOOKUP();			    /* ref */
		}
		CODEGEN(n->init);			    /* ref ref */
		if (!CG_IS_VALUE(n->init))
		    CG_GETVALUE();			    /* ref val */
		CG_PUTVALUE();				    /* - */
	}
	n->node.maxstack = n->init ? 1 + n->init->maxstack : 0;
}
#endif

/*
 * Note: All declared vars end up attached to a function body's vars
 * list, and are set to undefined upon entry to that function.
 * See also:
 *	SEE_function_put_args()		- put args
 *	FunctionDeclaration_fproc()	- put func decls
 *	SourceElements_fproc()		- put vars
 */




static struct node *
VariableDeclaration_parse(parser)
	struct parser *parser;
{
	struct VariableDeclaration_node *v;

	v = NEW_NODE(struct VariableDeclaration_node, 
		NODECLASS_VariableDeclaration);
        v->var = SEE_NEW(parser->interpreter, struct var);
	if (NEXT == tIDENT)
		v->var->name = NEXT_VALUE->u.string;
	EXPECT(tIDENT);
	if (NEXT == '=') {
		SKIP;
		v->init = PARSE(AssignmentExpression);
	} else
		v->init = NULL;

	/* Record declared variables */
	if (parser->vars) {
		*parser->vars = v->var;
		parser->vars = &v->var->next;
	}

	return (struct node *)v;
}

/*
 *	-- 12.3
 *
 *	EmptyStatement
 *	:	';'
 *	;
 */

/* 12.3 */
#if WITH_PARSER_EVAL
static void
EmptyStatement_eval(na, context, res)
	struct node *na;
	struct SEE_context *context;
	struct SEE_value *res;
{
	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, NULL, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
EmptyStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	CG_LOC(&na->location);
	/* CG_NOP(); */		/* - */
	na->maxstack = 0;
}
#endif


static struct node *
EmptyStatement_parse(parser)
	struct parser *parser;
{
	struct node *n;

	n = NEW_NODE(struct node, NODECLASS_EmptyStatement);
	EXPECT_SEMICOLON;
	return n;
}

/*
 *	-- 12.4
 *
 *	ExpressionStatement
 *	:	Expression ';'		-- lookahead != '{' or tFUNCTION
 *	;
 */

/* 12.4 */
#if WITH_PARSER_EVAL
static void
ExpressionStatement_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1;
	struct SEE_value *v = SEE_NEW(context->interpreter, struct SEE_value);

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->a, context, &r1);
	GetValue(context, &r1, v);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ExpressionStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CG_LOC(&na->location);
	CODEGEN(n->a);			/* ref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();		/* val */
	CG_SETC();			/* -   */

	n->node.maxstack = n->a->maxstack;
}
#endif


static struct node *
ExpressionStatement_make(interp, node)
	struct SEE_interpreter *interp;
	struct node *node;
{
	struct Unary_node *n;

	n = NEW_NODE_INTERNAL(interp, struct Unary_node, 
	    NODECLASS_ExpressionStatement);
	n->a = node;
	return (struct node *)n;
}

static struct node *
ExpressionStatement_parse(parser)
	struct parser *parser;
{
        struct Unary_node *n;

	n = NEW_NODE(struct Unary_node, NODECLASS_ExpressionStatement);
	n->a = PARSE(Expression);
	EXPECT_SEMICOLON;
	return (struct node *)n;
}


/*
 *	-- 12.5
 *
 *	IfStatement
 *	:	tIF '(' Expression ')' Statement tELSE Statement
 *	|	tIF '(' Expression ')' Statement
 *	;
 */

/* 12.5 */
#if WITH_PARSER_EVAL
static void
IfStatement_eval(na, context, res)
	struct node *na; /* (struct IfStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct IfStatement_node *n = CAST_NODE(na, IfStatement);
	struct SEE_value r1, r2, r3;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->cond, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToBoolean(context->interpreter, &r2, &r3);
	if (r3.u.boolean)
		EVAL(n->btrue, context, res);
	else if (n->bfalse)
		EVAL(n->bfalse, context, res);
	else
		_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, NULL, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
IfStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct IfStatement_node *n = CAST_NODE(na, IfStatement);
	SEE_code_patchable_t L1, L2;

	CG_LOC(&na->location);
	CODEGEN(n->cond);			/*     ref      */
	if (!CG_IS_VALUE(n->cond))
	    CG_GETVALUE();			/*     val      */
	if (!CG_IS_BOOLEAN(n->cond))
	    CG_TOBOOLEAN();			/*     bool     */
	CG_B_TRUE_f(L1);			/*     -   (L1) */
	if (n->bfalse)
	    CODEGEN(n->bfalse);			/*     -        */
	CG_B_ALWAYS_f(L2);			/*     -   (L2) */
    CG_LABEL(L1);				/* L1: -        */
	CODEGEN(n->btrue);			/*     -        */
    CG_LABEL(L2);				/* L2: -        */

	n->node.maxstack = MAX3(n->cond->maxstack,
	    n->btrue->maxstack, n->bfalse ? n->bfalse->maxstack : 0);
}
#endif



static struct node *
IfStatement_parse(parser)
	struct parser *parser;
{
	struct node *cond, *btrue, *bfalse;
	struct IfStatement_node *n;

	n = NEW_NODE(struct IfStatement_node, NODECLASS_IfStatement);
	EXPECT(tIF);
	EXPECT('(');
	cond = PARSE(Expression);
	EXPECT(')');
	btrue = PARSE(Statement);
	if (NEXT != tELSE)
		bfalse = NULL;
	else {
		SKIP; /* 'else' */
		bfalse = PARSE(Statement);
	}
	n->cond = cond;
	n->btrue = btrue;
	n->bfalse = bfalse;
	return (struct node *)n;
}

/*
 *	-- 12.6
 *	IterationStatement
 *	:	tDO Statement tWHILE '(' Expression ')' ';'	-- 12.6.1
 *	|	tWHILE '(' Expression ')' Statement		-- 12.6.2
 *	|	tFOR '(' ';' ';' ')' Statement
 *	|	tFOR '(' ExpressionNoIn ';' ';' ')' Statement
 *	|	tFOR '(' ';' Expression ';' ')' Statement
 *	|	tFOR '(' ExpressionNoIn ';' Expression ';' ')' Statement
 *	|	tFOR '(' ';' ';' Expression ')' Statement
 *	|	tFOR '(' ExpressionNoIn ';' ';' Expression ')' Statement
 *	|	tFOR '(' ';' Expression ';' Expression ')' Statement
 *	|	tFOR '(' ExpressionNoIn ';' Expression ';' Expression ')'
 *			Statement
 *	|	tFOR '(' tVAR VariableDeclarationListNoIn ';' ';' ')' Statement
 *	|	tFOR '(' tVAR VariableDeclarationListNoIn ';'  
 *			Expression ';' ')' Statement
 *	|	tFOR '(' tVAR VariableDeclarationListNoIn ';' ';' 
 *			Expression ')' Statement
 *	|	tFOR '(' tVAR VariableDeclarationListNoIn ';' Expression ';' 
 *			Expression ')' Statement
 *	|	tFOR '(' LeftHandSideExpression tIN Expression ')' Statement
 *	|	tFOR '(' tVAR VariableDeclarationNoIn tIN Expression ')' 
 *			Statement
 *	;
 */

#if WITH_PARSER_EVAL
static void
IterationStatement_dowhile_eval(na, context, res)
	struct node *na; /* (struct IterationStatement_while_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct IterationStatement_while_node *n = 
		CAST_NODE(na, IterationStatement_while);
	struct SEE_value *v, r7, r8, r9;

	v = NULL;
 step2:	EVAL(n->body, context, res);
	if (res->u.completion.value)
	    v = res->u.completion.value;
	if (res->u.completion.type == SEE_COMPLETION_CONTINUE &&
	    n->target == res->u.completion.target)
	    goto step7;
	if (res->u.completion.type == SEE_COMPLETION_BREAK &&
	    n->target == res->u.completion.target)
	    goto step11;
	if (res->u.completion.type != SEE_COMPLETION_NORMAL)
	    goto out;
 step7: TRACE(&na->location, context, SEE_TRACE_STATEMENT);
 	EVAL(n->cond, context, &r7);
	GetValue(context, &r7, &r8);
	SEE_ToBoolean(context->interpreter, &r8, &r9);
	if (r9.u.boolean)
	    goto step2;
 step11:_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
 out:	;
}
#endif

#if WITH_PARSER_CODEGEN
static void
IterationStatement_dowhile_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct IterationStatement_while_node *n = 
		CAST_NODE(na, IterationStatement_while);
	SEE_code_addr_t L1, L2, L3;

	push_patchables(cc, n->target, CONTINUABLE);

    L1 = CG_HERE();
	CODEGEN(n->body);
    L2 = CG_HERE();			    /* continue point */
	CG_LOC(&na->location);
	CODEGEN(n->cond);
	if (!CG_IS_VALUE(n->cond))
	    CG_GETVALUE();
	CG_B_TRUE_b(L1);
    L3 = CG_HERE();			    /* break point */

	pop_patchables(cc, L2, L3);

	na->maxstack = MAX(n->cond->maxstack, n->body->maxstack);
}
#endif




#if WITH_PARSER_EVAL
static void
IterationStatement_while_eval(na, context, res)
	struct node *na; /* (struct IterationStatement_while_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct IterationStatement_while_node *n = 
		CAST_NODE(na, IterationStatement_while);
	struct SEE_value *v, r2, r3, r4;

	v = NULL;
 step2: TRACE(&na->location, context, SEE_TRACE_STATEMENT);
 	EVAL(n->cond, context, &r2);
	GetValue(context, &r2, &r3);
	SEE_ToBoolean(context->interpreter, &r3, &r4);
	if (!r4.u.boolean) {
	    _SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
	    return;
	}
	EVAL(n->body, context, res);
	if (res->u.completion.value)
		v = res->u.completion.value;
	if (res->u.completion.type == SEE_COMPLETION_CONTINUE &&
	    n->target == res->u.completion.target)
		goto step2;
	if (res->u.completion.type == SEE_COMPLETION_BREAK &&
	    n->target == res->u.completion.target)
	{
	    _SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
	    return;
	}
	if (res->u.completion.type != SEE_COMPLETION_NORMAL)
	    return;
	goto step2;
}
#endif

#if WITH_PARSER_CODEGEN
static void
IterationStatement_while_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct IterationStatement_while_node *n = 
		CAST_NODE(na, IterationStatement_while);
	SEE_code_patchable_t P1;
	SEE_code_addr_t L1, L2, L3;

	push_patchables(cc, n->target, CONTINUABLE);

	CG_B_ALWAYS_f(P1);
    L1 = CG_HERE();
	CODEGEN(n->body);
    CG_LABEL(P1);
    L2 = CG_HERE();			    /* continue point */
	CG_LOC(&na->location);
	CODEGEN(n->cond);
	if (!CG_IS_VALUE(n->cond))
	    CG_GETVALUE();
	CG_B_TRUE_b(L1);
    L3 = CG_HERE();			    /* break point */

	pop_patchables(cc, L2, L3);

	na->maxstack = MAX(n->cond->maxstack, n->body->maxstack);
}
#endif



/* 12.6.3 - "for (init; cond; incr) body" */
#if WITH_PARSER_EVAL
static void
IterationStatement_for_eval(na, context, res)
	struct node *na; /* (struct IterationStatement_for_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct IterationStatement_for_node *n = 
		CAST_NODE(na, IterationStatement_for);
	struct SEE_value *v, r2, r3, r6, r7, r8, r16, r17;

	if (n->init) {
	    TRACE(&n->init->location, context, SEE_TRACE_STATEMENT);
	    EVAL(n->init, context, &r2);
	    GetValue(context, &r2, &r3);		/* r3 not used */
	}
	v = NULL;
 step5:	if (n->cond) {
	    TRACE(&n->cond->location, context, SEE_TRACE_STATEMENT);
	    EVAL(n->cond, context, &r6);
	    GetValue(context, &r6, &r7);
	    SEE_ToBoolean(context->interpreter, &r7, &r8);
	    if (!r8.u.boolean) goto step19;
	} else
	    TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->body, context, res);
	if (res->u.completion.value)
	    v = res->u.completion.value;
	if (res->u.completion.type == SEE_COMPLETION_BREAK &&
	    n->target == res->u.completion.target)
		goto step19;
	if (res->u.completion.type == SEE_COMPLETION_CONTINUE &&
	    n->target == res->u.completion.target)
		goto step15;
	if (res->u.completion.type != SEE_COMPLETION_NORMAL)
		return;
step15: if (n->incr) {
	    TRACE(&n->incr->location, context, SEE_TRACE_STATEMENT);
	    EVAL(n->incr, context, &r16);
	    GetValue(context, &r16, &r17);	/* r17 not used */
	}
	goto step5;
step19:	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
IterationStatement_for_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct IterationStatement_for_node *n = 
		CAST_NODE(na, IterationStatement_for);
	SEE_code_patchable_t P1;
	SEE_code_addr_t L1, L2, L3;

	push_patchables(cc, n->target, CONTINUABLE);

	if (n->init) {
		CG_LOC(&n->init->location);
		CODEGEN(n->init);
		if (!CG_IS_VALUE(n->init))
		    CG_GETVALUE();
		CG_POP();
	}
	CG_B_ALWAYS_f(P1);
    L1 = CG_HERE();
	CODEGEN(n->body);
    L2 = CG_HERE();			    /* continue point */
	if (n->incr) {
		CG_LOC(&n->incr->location);
		CODEGEN(n->incr);
		if (!CG_IS_VALUE(n->incr))
		    CG_GETVALUE();
		CG_POP();
	}
    CG_LABEL(P1);
	if (n->cond) {
	    CG_LOC(&n->cond->location);
	    CODEGEN(n->cond);
	    if (!CG_IS_VALUE(n->cond))
		CG_GETVALUE();
	    CG_B_TRUE_b(L1);
	} else {
	    CG_LOC(&na->location);
	    CG_B_ALWAYS_b(L1);
	}
    L3 = CG_HERE();			    /* break point */

	pop_patchables(cc, L2, L3);

	na->maxstack = MAX(
	    MAX(n->incr ? n->incr->maxstack : 0,
		n->init ? n->init->maxstack : 0),
	    MAX(n->cond ? n->cond->maxstack : 0,
		n->body->maxstack));
}
#endif



/* 12.6.3 - "for (var init; cond; incr) body" */
#if WITH_PARSER_EVAL
static void
IterationStatement_forvar_eval(na, context, res)
	struct node *na; /* (struct IterationStatement_for_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct IterationStatement_for_node *n = 
		CAST_NODE(na, IterationStatement_for);
	struct SEE_value *v, r1, r4, r5, r6, r14, r15;

	TRACE(&n->init->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->init, context, &r1);
	v = NULL;
 step3: if (n->cond) {
	    TRACE(&n->cond->location, context, SEE_TRACE_STATEMENT);
	    EVAL(n->cond, context, &r4);
	    GetValue(context, &r4, &r5);
	    SEE_ToBoolean(context->interpreter, &r5, &r6);
	    if (!r6.u.boolean) goto step17; /* spec bug: says step 14 */
	} else
	    TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->body, context, res);
	if (res->u.completion.value)
	    v = res->u.completion.value;
	if (res->u.completion.type == SEE_COMPLETION_BREAK &&
	    n->target == res->u.completion.target)
		goto step17;
	if (res->u.completion.type == SEE_COMPLETION_CONTINUE &&
	    n->target == res->u.completion.target)
		goto step13;
	if (res->u.completion.type != SEE_COMPLETION_NORMAL)
		return;
step13: if (n->incr) {
	    TRACE(&n->incr->location, context, SEE_TRACE_STATEMENT);
	    EVAL(n->incr, context, &r14);
	    GetValue(context, &r14, &r15); 		/* value not used */
	}
	goto step3;
step17:	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
IterationStatement_forvar_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct IterationStatement_for_node *n = 
		CAST_NODE(na, IterationStatement_for);
	SEE_code_patchable_t P1;
	SEE_code_addr_t L1, L2, L3;

	push_patchables(cc, n->target, CONTINUABLE);

	CG_LOC(&n->init->location);
	CODEGEN(n->init);
	if (!CG_IS_VALUE(n->init))
	    CG_GETVALUE();
	CG_B_ALWAYS_f(P1);
    L1 = CG_HERE();
	CODEGEN(n->body);
    L2 = CG_HERE();			    /* continue point */
	if (n->incr) {
		CG_LOC(&n->incr->location);
		CODEGEN(n->incr);
		if (!CG_IS_VALUE(n->incr))
		    CG_GETVALUE();
		CG_POP();
	}
    CG_LABEL(P1);
	if (n->cond) {
	    CG_LOC(&n->cond->location);
	    CODEGEN(n->cond);
	    if (!CG_IS_VALUE(n->cond))
		CG_GETVALUE();
	    CG_B_TRUE_b(L1);
	} else {
	    CG_LOC(&na->location);
	    CG_B_ALWAYS_b(L1);
	}
    L3 = CG_HERE();			    /* break point */

	pop_patchables(cc, L2, L3);

	na->maxstack = MAX(
	    MAX(n->incr ? n->incr->maxstack : 0,
		n->init->maxstack),
	    MAX(n->cond ? n->cond->maxstack : 0,
		n->body->maxstack));
}
#endif


/* NB : the VarDecls of n->init are exposed through parser->vars */



/* 12.6.3 - "for (lhs in list) body" */
#if WITH_PARSER_EVAL
static void
IterationStatement_forin_eval(na, context, res)
	struct node *na; /* (struct IterationStatement_forin_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct IterationStatement_forin_node *n = 
		CAST_NODE(na, IterationStatement_forin);
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_value *v, r1, r2, r3, r5, r6;
	struct SEE_string **props0, **props;

        TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->list, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToObject(interp, &r2, &r3);
	v = NULL;
	for (props0 = props = SEE_enumerate(interp, r3.u.object); 
	     *props; 
	     props++)
	{
	    if (!SEE_OBJECT_HASPROPERTY(interp, r3.u.object, *props))
		    continue;	/* property was deleted! */
	    SEE_SET_STRING(&r5, *props);
	    EVAL(n->lhs, context, &r6);
	    PutValue(context, &r6, &r5);
	    EVAL(n->body, context, res);
	    if (res->u.completion.value)
		v = res->u.completion.value;
	    if (res->u.completion.type == SEE_COMPLETION_BREAK &&
		n->target == res->u.completion.target)
		    break;
	    if (res->u.completion.type == SEE_COMPLETION_CONTINUE &&
		n->target == res->u.completion.target)
		    continue;
	    if (res->u.completion.type != SEE_COMPLETION_NORMAL)
		    return;
	}
	SEE_enumerate_free(interp, props0);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
IterationStatement_forin_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct IterationStatement_forin_node *n = 
		CAST_NODE(na, IterationStatement_forin);
	SEE_code_patchable_t P1;
	SEE_code_addr_t L1, L2, L3;

	CG_LOC(&na->location);
	CODEGEN(n->list);		/* ref */
	if (!CG_IS_VALUE(n->list))
	    CG_GETVALUE();		/* val */
	if (!CG_IS_OBJECT(n->list))
	    CG_TOOBJECT();		/* obj */

	CG_S_ENUM();			/* -  */
	cg_block_enter(cc);

	push_patchables(cc, n->target, CONTINUABLE);

	CG_B_ALWAYS_f(P1);

    L1 = CG_HERE();
	CODEGEN(n->lhs);		/* str ref */
	CG_EXCH();			/* ref str */
	CG_PUTVALUE();			/* - */

	CODEGEN(n->body);

    L2 = CG_HERE();			/* continue point */
    CG_LABEL(P1);
	CG_B_ENUM_b(L1);

    L3 = CG_HERE();			/* break point */
	pop_patchables(cc, L2, L3);

	CG_END(cg_block_current(cc));
	cg_block_leave(cc);

	na->maxstack = MAX4(
	    2,
	    n->list->maxstack,
	    1 + n->lhs->maxstack,
	    n->body->maxstack);
}
#endif





#if WITH_PARSER_EVAL
static void
IterationStatement_forvarin_eval(na, context, res)
	struct node *na; /* (struct IterationStatement_forin_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct IterationStatement_forin_node *n = 
		CAST_NODE(na, IterationStatement_forin);
	struct SEE_interpreter *interp = context->interpreter;
	struct SEE_value *v, r2, r3, r4, r6, r7;
	struct SEE_string **props0, **props;
	struct VariableDeclaration_node *lhs 
		= CAST_NODE(n->lhs, VariableDeclaration);

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->lhs, context, NULL);
	EVAL(n->list, context, &r2);
	GetValue(context, &r2, &r3);
	SEE_ToObject(interp, &r3, &r4);
	v = NULL;
	for (props0 = props = SEE_enumerate(interp, r4.u.object);
	     *props; 
	     props++)
	{
	    if (!SEE_OBJECT_HASPROPERTY(interp, r4.u.object, *props))
		    continue;	/* property was deleted! */
	    SEE_SET_STRING(&r6, *props);
	    /* spec bug: "see 0" in step 7 */
	    SEE_scope_lookup(context->interpreter, context->scope, 
	    	lhs->var->name, &r7);
	    PutValue(context, &r7, &r6);
	    EVAL(n->body, context, res);
	    if (res->u.completion.value)
		v = res->u.completion.value;
	    if (res->u.completion.type == SEE_COMPLETION_BREAK &&
		n->target == res->u.completion.target)
		    break;
	    if (res->u.completion.type == SEE_COMPLETION_CONTINUE &&
		n->target == res->u.completion.target)
		    continue;
	    if (res->u.completion.type != SEE_COMPLETION_NORMAL)
		    return;
	}
	SEE_enumerate_free(interp, props0);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
IterationStatement_forvarin_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct IterationStatement_forin_node *n = 
		CAST_NODE(na, IterationStatement_forin);
	struct VariableDeclaration_node *lhs 
		= CAST_NODE(n->lhs, VariableDeclaration);
	SEE_code_patchable_t P1;
	SEE_code_addr_t L1, L2, L3;

	CG_LOC(&na->location);
	CODEGEN(n->lhs);		/* - */
	CODEGEN(n->list);		/* ref */
	if (!CG_IS_VALUE(n->list))
	    CG_GETVALUE();		/* val */
	if (!CG_IS_OBJECT(n->list))
	    CG_TOOBJECT();		/* obj */

	CG_S_ENUM();			/* -  */
	cg_block_enter(cc);

	push_patchables(cc, n->target, CONTINUABLE);

	CG_B_ALWAYS_f(P1);

    L1 = CG_HERE();
	if (cg_var_is_in_scope(cc, lhs->var->name)) 
	    CG_VREF(cg_var_id(cc, lhs->var->name));    /* ref */
	else {
	    CG_STRING(lhs->var->name);		    /* str */
	    CG_LOOKUP();			    /* ref */
	}
	CG_EXCH();			/* ref str */
	CG_PUTVALUE();			/* - */

	CODEGEN(n->body);

    L2 = CG_HERE();			/* continue point */
    CG_LABEL(P1);
	CG_B_ENUM_b(L1);

    L3 = CG_HERE();			/* break point */
	pop_patchables(cc, L2, L3);
	CG_END(cg_block_current(cc));
	cg_block_leave(cc);

	na->maxstack = MAX4(
	    2,
	    n->list->maxstack,
	    1 + n->lhs->maxstack,
	    n->body->maxstack);
}
#endif




static struct node *
IterationStatement_parse(parser)
	struct parser *parser;
{
	struct IterationStatement_while_node *w;
	struct IterationStatement_for_node *fn;
	struct IterationStatement_forin_node *fin;
	struct node *n;
	struct labelset *labelset = labelset_current(parser);

	labelset->continuable = 1;
	label_enter(parser, EMPTY_LABEL);
	switch (NEXT) {
	case tDO:
		w = NEW_NODE(struct IterationStatement_while_node,
			NODECLASS_IterationStatement_dowhile);
		SKIP;
		w->target = labelset->target;
		w->body = PARSE(Statement);
		EXPECT(tWHILE);
		EXPECT('(');
		w->cond = PARSE(Expression);
		EXPECT(')');
		EXPECT_SEMICOLON;
		label_leave(parser);
		return (struct node *)w;
	case tWHILE:
		w = NEW_NODE(struct IterationStatement_while_node,
			NODECLASS_IterationStatement_while);
		SKIP;
		w->target = labelset->target;
		EXPECT('(');
		w->cond = PARSE(Expression);
		EXPECT(')');
		w->body = PARSE(Statement);
		label_leave(parser);
		return (struct node *)w;
	case tFOR:
		break;
	default:
		SEE_ASSERT(parser->interpreter, !"unexpected token");
	}

	SKIP;		/* tFOR */
	EXPECT('(');

	if (NEXT == tVAR) {			 /* "for ( var" */
	    SKIP;	/* tVAR */
	    parser->noin = 1;
	    n = PARSE(VariableDeclarationList);	/* NB adds to parser->vars */
	    parser->noin = 0;
	    if (NEXT == tIN && 
		  n->nodeclass == NODECLASS_VariableDeclaration)
	    {					/* "for ( var VarDecl in" */
		fin = NEW_NODE(struct IterationStatement_forin_node,
		    NODECLASS_IterationStatement_forvarin);
		fin->target = labelset->target;
		fin->lhs = n;
		SKIP;	/* tIN */
		fin->list = PARSE(Expression);
		EXPECT(')');
		fin->body = PARSE(Statement);
		label_leave(parser);
		return (struct node *)fin;
	    }

	    /* Accurately describe possible tokens at this stage */
	    EXPECTX(';', 
	       (n->nodeclass == NODECLASS_VariableDeclaration
		  ? "';' or 'in'"
		  : "';'"));
					    /* "for ( var VarDeclList ;" */
	    fn = NEW_NODE(struct IterationStatement_for_node,
		NODECLASS_IterationStatement_forvar);
	    fn->target = labelset->target;
	    fn->init = n;
	    if (NEXT != ';')
		fn->cond = PARSE(Expression);
	    else
		fn->cond = NULL;
	    EXPECT(';');
	    if (NEXT != ')')
		fn->incr = PARSE(Expression);
	    else
		fn->incr = NULL;
	    EXPECT(')');
	    fn->body = PARSE(Statement);
	    label_leave(parser);
	    return (struct node *)fn;
	}

	if (NEXT != ';') {
	    parser->noin = 1;
	    n = PARSE(Expression);
	    parser->noin = 0;
	    if (NEXT == tIN && parser->is_lhs) {   /* "for ( lhs in" */
		fin = NEW_NODE(struct IterationStatement_forin_node,
		    NODECLASS_IterationStatement_forin);
		fin->target = labelset->target;
		fin->lhs = n;
		SKIP;		/* tIN */
		fin->list = PARSE(Expression);
		EXPECT(')');
		fin->body = PARSE(Statement);
		label_leave(parser);
		return (struct node *)fin;
	    }
	} else
	    n = NULL;				/* "for ( ;" */

	fn = NEW_NODE(struct IterationStatement_for_node,
	    NODECLASS_IterationStatement_for);
	fn->target = labelset->target;
	fn->init = n;
	EXPECT(';');
	if (NEXT != ';')
	    fn->cond = PARSE(Expression);
	else
	    fn->cond = NULL;
	EXPECT(';');
	if (NEXT != ')')
	    fn->incr = PARSE(Expression);
	else
	    fn->incr = NULL;
	EXPECT(')');
	fn->body = PARSE(Statement);
	label_leave(parser);
	return (struct node *)fn;
}

/*
 *	-- 12.7
 *
 *	ContinueStatement
 *	:	tCONTINUE ';'
 *	|	tCONTINUE tIDENT ';'
 *	;
 */

#if WITH_PARSER_EVAL
static void
ContinueStatement_eval(na, context, res)
	struct node *na; /* (struct ContinueStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct ContinueStatement_node *n = CAST_NODE(na, ContinueStatement);

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_CONTINUE, NULL, n->target);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ContinueStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct ContinueStatement_node *n = CAST_NODE(na, ContinueStatement);
	struct patchables *patchables;
	SEE_code_patchable_t pa;

	patchables = patch_find(cc, n->target, tCONTINUE);
	
	CG_LOC(&na->location);

	/* Generate an END instruction if we are continuing to an outer block */
	if (patchables->block_depth < cc->block_depth)
	    CG_END(patchables->block_depth);

	CG_B_ALWAYS_f(pa);
	patch_add_continue(cc, patchables, pa);

	n->node.maxstack = 0;
}
#endif



static struct node *
ContinueStatement_parse(parser)
	struct parser *parser;
{
	struct ContinueStatement_node *cn;

	cn = NEW_NODE(struct ContinueStatement_node,
		NODECLASS_ContinueStatement);
	EXPECT(tCONTINUE);
	if (NEXT_IS_SEMICOLON)
	    cn->target = target_lookup(parser, EMPTY_LABEL, tCONTINUE);
	else {
	    if (NEXT == tIDENT)
		cn->target = target_lookup(parser, NEXT_VALUE->u.string, 
			tCONTINUE);
	    EXPECT(tIDENT);
	}
	EXPECT_SEMICOLON;
	return (struct node *)cn;
}

/*
 *	-- 12.8
 *
 *	BreakStatement
 *	:	tBREAK ';'
 *	|	tBREAK tIDENT ';'
 *	;
 */

#if WITH_PARSER_EVAL
static void
BreakStatement_eval(na, context, res)
	struct node *na; /* (struct BreakStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct BreakStatement_node *n = CAST_NODE(na, BreakStatement);

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_BREAK, NULL, n->target);
}
#endif

#if WITH_PARSER_CODEGEN
static void
BreakStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct BreakStatement_node *n = CAST_NODE(na, BreakStatement);
	struct patchables *patchables;
	SEE_code_patchable_t pa;

	patchables = patch_find(cc, n->target, tBREAK);

	CG_LOC(&na->location);

	/* Generate an END instruction if we are breaking to an outer block */
	if (patchables->block_depth < cc->block_depth)
	    CG_END(patchables->block_depth);

	CG_B_ALWAYS_f(pa);
	patch_add_break(cc, patchables, pa);

	n->node.maxstack = 0;
}
#endif


static struct node *
BreakStatement_parse(parser)
	struct parser *parser;
{
	struct BreakStatement_node *cn;

	cn = NEW_NODE(struct BreakStatement_node,
		NODECLASS_BreakStatement);
	EXPECT(tBREAK);
	if (NEXT_IS_SEMICOLON)
	    cn->target = target_lookup(parser, EMPTY_LABEL, tBREAK);
	else {
	    if (NEXT == tIDENT)
		cn->target = target_lookup(parser, NEXT_VALUE->u.string, 
		    tBREAK);
	    EXPECT(tIDENT);
	}
	EXPECT_SEMICOLON;
	return (struct node *)cn;
}

/*
 *	-- 12.9
 *
 *	ReturnStatement
 *	:	tRETURN ';'
 *	|	tRETURN Expression ';'
 *	;
 */

#if WITH_PARSER_EVAL
static void
ReturnStatement_eval(na, context, res)
	struct node *na; /* (struct ReturnStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct ReturnStatement_node *n = CAST_NODE(na, ReturnStatement);
	struct SEE_value r2, *v;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->expr, context, &r2);
	v = SEE_NEW(context->interpreter, struct SEE_value);
	GetValue(context, &r2, v);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_RETURN, v, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ReturnStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct ReturnStatement_node *n = CAST_NODE(na, ReturnStatement);

	CG_LOC(&na->location);
	CODEGEN(n->expr);			/* ref */
	if (!CG_IS_VALUE(n->expr))
	    CG_GETVALUE();			/* val */
	CG_SETC();				/* - */
	CG_END(0);				/* (halt) */

	n->node.maxstack = n->expr->maxstack;
}
#endif




#if WITH_PARSER_EVAL
static void
ReturnStatement_undef_eval(na, context, res)
	struct node *na; /* (struct ReturnStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	static struct SEE_value undef = { SEE_UNDEFINED };

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_RETURN, &undef, NO_TARGET);
}
#endif

#if WITH_PARSER_CODEGEN
static void
ReturnStatement_undef_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	CG_LOC(&na->location);
	CG_UNDEFINED();			    /* undef */
	CG_SETC();			    /* - */
	CG_END(0);			    /* (halt) */

	na->maxstack = 1;
}
#endif



static struct node *
ReturnStatement_parse(parser)
	struct parser *parser;
{
	struct ReturnStatement_node *rn;

	EXPECT(tRETURN);
	if (!parser->funcdepth)
		ERRORm("'return' not within a function");
	if (!NEXT_IS_SEMICOLON) {
            rn = NEW_NODE(struct ReturnStatement_node,
                        NODECLASS_ReturnStatement);
	    rn->expr = PARSE(Expression);
	} else
            rn = NEW_NODE(struct ReturnStatement_node,
                        NODECLASS_ReturnStatement_undef);
	EXPECT_SEMICOLON;
	return (struct node *)rn;
}

/*
 *	-- 12.10
 *
 *	WithStatement
 *	:	tWITH '(' Expression ')' Statement
 *	;
 */

#if WITH_PARSER_EVAL
static void
WithStatement_eval(na, context, res)
	struct node *na; /* (struct Binary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	SEE_try_context_t ctxt;
	struct SEE_value r1, r2, r3;
	struct SEE_scope *s;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);
	SEE_ToObject(context->interpreter, &r2, &r3);

	/* Insert r3 in front of current scope chain */
	s = SEE_NEW(context->interpreter, struct SEE_scope);
	s->obj = r3.u.object;
	s->next = context->scope;
	context->scope = s;
	SEE_TRY(context->interpreter, ctxt)
	    EVAL(n->b, context, res);
	context->scope = context->scope->next;
	SEE_DEFAULT_CATCH(context->interpreter, ctxt);
}
#endif

#if WITH_PARSER_CODEGEN
static void
WithStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	int old_var_scope;

	CG_LOC(&na->location);
	CODEGEN(n->a);			/* ref */
	if (!CG_IS_VALUE(n->a))	
	    CG_GETVALUE();		/* val */
	if (!CG_IS_OBJECT(n->a))
	    CG_TOOBJECT();		/* obj */

	CG_S_WITH();			/* - */
	cg_block_enter(cc);
	old_var_scope = cg_var_set_all_scope(cc, 0);

	CODEGEN(n->b);

	CG_END(cg_block_current(cc));
	cg_block_leave(cc);
	cg_var_set_all_scope(cc, old_var_scope);

	na->maxstack = MAX(n->a->maxstack, n->b->maxstack);
}
#endif



static struct node *
WithStatement_parse(parser)
	struct parser *parser;
{
	struct Binary_node *n;

	n = NEW_NODE(struct Binary_node, NODECLASS_WithStatement);
	EXPECT(tWITH);
	EXPECT('(');
	n->a = PARSE(Expression);
	EXPECT(')');
	n->b = PARSE(Statement);
	return (struct node *)n;
}

/*
 *	-- 12.11
 *
 *	SwitchStatement
 *	:	tSWITCH '(' Expression ')' CaseBlock
 *	;
 *
 *	CaseBlock
 *	:	'{' '}'
 *	|	'{' CaseClauses '}'
 *	|	'{' DefaultClause '}'
 *	|	'{' CaseClauses DefaultClause '}'
 *	|	'{' DefaultClause '}'
 *	|	'{' CaseClauses DefaultClause CaseClauses '}'
 *	;
 *
 *	CaseClauses
 *	:	CaseClause
 *	|	CaseClauses CaseClause
 *	;
 *
 *	CaseClause
 *	:	tCASE Expression ':'
 *	|	tCASE Expression ':' StatementList
 *	;
 *
 *	DefaultClause
 *	:	tDEFAULT ':'
 *	|	tDEFAULT ':' StatementList
 *	;
 */

#if WITH_PARSER_EVAL
static void
SwitchStatement_caseblock(n, context, input, res)
	struct SwitchStatement_node *n;
	struct SEE_context *context;
	struct SEE_value *input, *res;
{
	struct case_list *c;
	struct SEE_value cc1, cc2, cc3;

	/*
	 * Note, this should be functionally equivalent
	 * to the standard. We search through the in-order
	 * case statements to find an expression that is
	 * strictly equal to 'input', and then run all
	 * the statements from there till we break or reach
	 * the end. If no expression matches, we start at the
	 * default case, if one exists.
	 */
	for (c = n->cases; c; c = c->next) {
	    if (!c->expr) continue;
	    EVAL(c->expr, context, &cc1);
	    GetValue(context, &cc1, &cc2);
	    EqualityExpression_seq(context, input, &cc2, &cc3);
	    if (cc3.u.boolean)
		break;
	}
	if (!c)
	    c = n->defcase;	/* can be NULL, meaning no default */
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, NULL, NO_TARGET);
	for (; c; c = c->next) {
	    if (c->body)
		EVAL(c->body, context, res);
	    if (res->u.completion.type != SEE_COMPLETION_NORMAL)
		break;
	}
}

static void
SwitchStatement_eval(na, context, res)
	struct node *na; /* (struct SwitchStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct SwitchStatement_node *n = CAST_NODE(na, SwitchStatement);
	struct SEE_value *v, r1, r2;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->cond, context, &r1);
	GetValue(context, &r1, &r2);
	SwitchStatement_caseblock(n, context, &r2, res);
	if (res->u.completion.type == SEE_COMPLETION_BREAK &&
	    n->target == res->u.completion.target)
	{
		v = res->u.completion.value;
		_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, v, NO_TARGET);
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
SwitchStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct SwitchStatement_node *n = CAST_NODE(na, SwitchStatement);
	struct case_list *c;
	int ncases, i;
	SEE_code_patchable_t *case_patches, default_patch;
	unsigned int expr_maxstack = 0, body_maxstack = 0;

	for (ncases = 0, c = n->cases; c; c = c->next)
	    if (c->expr)
		ncases++;
	case_patches = SEE_ALLOCA(cc->code->interpeter, 
	    SEE_code_patchable_t, ncases);

	CG_LOC(&na->location);
	CODEGEN(n->cond);		/* ref */
	if (!CG_IS_VALUE(n->cond))
	    CG_GETVALUE();		/* val */

	for (i = 0, c = n->cases; c; c = c->next)
	    if (c->expr) {
		CG_DUP();		/* val val */
		CODEGEN(c->expr);	/* val val ref */
		expr_maxstack = MAX(expr_maxstack, 2 + c->expr->maxstack);
		if (!CG_IS_VALUE(c->expr))
		    CG_GETVALUE();	/* val val val */
		CG_SEQ();		/* val bool */
		CG_B_TRUE_f(case_patches[i]);	/* val */
		i++;
	    }
	CG_B_ALWAYS_f(default_patch);

	push_patchables(cc, n->target, !CONTINUABLE);

	for (i = 0, c = n->cases; c; c = c->next) {
	    if (!c->expr)
		CG_LABEL(default_patch);
	    else {
		CG_LABEL(case_patches[i]);
		i++;
	    }
	    if (c->body) {
		CODEGEN(c->body);	/* val */
		body_maxstack = MAX(body_maxstack, 1 + c->body->maxstack);
	    }
	}

	/* If there was no default body, patch through to the end */
	if (!n->defcase)
	    CG_LABEL(default_patch);
	
	pop_patchables(cc, 0, CG_HERE());   /* All breaks lead here */
	CG_POP();

	na->maxstack = MAX3(n->cond->maxstack, expr_maxstack, body_maxstack);
}
#endif




static struct node *
SwitchStatement_parse(parser)
	struct parser *parser;
{
	struct SwitchStatement_node *n;
	struct case_list **cp, *c;
	int next;
	struct labelset *labelset = labelset_current(parser);

	n = NEW_NODE(struct SwitchStatement_node,
		NODECLASS_SwitchStatement);
	n->target = labelset->target;
	label_enter(parser, EMPTY_LABEL);
	EXPECT(tSWITCH);
	EXPECT('(');
	n->cond = PARSE(Expression);
	EXPECT(')');
	EXPECT('{');
	cp = &n->cases;
	n->defcase = NULL;
	while (NEXT != '}') {
	    c = SEE_NEW(parser->interpreter, struct case_list);
	    *cp = c;
	    cp = &c->next;
	    switch (NEXT) {
	    case tCASE:
		SKIP;
		c->expr = PARSE(Expression);
		break;
	    case tDEFAULT:
		SKIP;
		c->expr = NULL;
		if (n->defcase)
		    ERRORm("duplicate 'default' clause");
		n->defcase = c;
		break;
	    default:
		EXPECTED("'}', 'case' or 'default'");
	    }
	    EXPECT(':');
	    next = NEXT;
	    if (next != '}' && next != tDEFAULT && next != tCASE)
		c->body = PARSE(StatementList);
	    else
		c->body = NULL;
	}
	*cp = NULL;
	EXPECT('}');
	label_leave(parser);
	return (struct node *)n;
}

/*
 *	-- 12.12
 *
 *	LabelledStatement
 *	:	tIDENT
 *	|	Statement
 *	;
 */

#if WITH_PARSER_EVAL
static void
LabelledStatement_eval(na, context, res)
	struct node *na; /* (struct LabelledStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct LabelledStatement_node *n = CAST_NODE(na, LabelledStatement);

	EVAL(n->unary.a, context, res);
	if (res->u.completion.type == SEE_COMPLETION_BREAK &&
		res->u.completion.target == n->target)
	{
	    res->u.completion.type = SEE_COMPLETION_NORMAL;
	    res->u.completion.target = NO_TARGET;
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
LabelledStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct LabelledStatement_node *n = CAST_NODE(na, LabelledStatement);
	SEE_code_addr_t L1;

	push_patchables(cc, n->target, !CONTINUABLE);
	CODEGEN(n->unary.a);
    L1 = CG_HERE();
	pop_patchables(cc, NULL, L1);
	na->maxstack = n->unary.a->maxstack;
}
#endif


static struct node *
LabelledStatement_parse(parser)
	struct parser *parser;
{
	struct LabelledStatement_node *n;
	struct SEE_string *label;
	unsigned int label_count = 0;
	struct labelset *old_labelset = parser->current_labelset;

	n = NEW_NODE(struct LabelledStatement_node, 
			NODECLASS_LabelledStatement);
	
	parser->current_labelset = NULL;
	n->target = labelset_current(parser)->target;
	do {
		/* Lookahead is IDENT ':' */
		label = NEXT_VALUE->u.string;
		label_enter(parser, label);
		label_count++;
		EXPECT(tIDENT);
		EXPECT(':');
	} while (NEXT == tIDENT && lookahead(parser, 1) == ':');

	switch (NEXT) {
	case tDO:
	case tWHILE:
	case tFOR:
		n->unary.a = PARSE(IterationStatement);
		break;
	case tSWITCH:
		n->unary.a = PARSE(SwitchStatement);
		break;
	default:
		n->unary.a = PARSE(Statement);
	}

	while (label_count--)
		label_leave(parser);

	parser->current_labelset = old_labelset;
	return (struct node *)n;
}

/*
 *	-- 12.13
 *
 *	ThrowStatement
 *	:	tTHROW Expression ';'
 *	;
 */

#if WITH_PARSER_EVAL
static void
ThrowStatement_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	struct SEE_value r1, r2;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	EVAL(n->a, context, &r1);
	GetValue(context, &r1, &r2);

	traceback_enter(context->interpreter, 0, &n->node.location, 
	    SEE_CALLTYPE_THROW);
	TRACE(&na->location, context, SEE_TRACE_THROW);
	SEE_THROW(context->interpreter, &r2);

	/* NOTREACHED */
}
#endif

#if WITH_PARSER_CODEGEN
static void
ThrowStatement_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Unary_node *n = CAST_NODE(na, Unary);

	CG_LOC(&na->location);
	CODEGEN(n->a);		/* ref */
	if (!CG_IS_VALUE(n->a))
	    CG_GETVALUE();	/* val */
	CG_THROW();		/* - */

	na->maxstack = n->a->maxstack;
}
#endif


static struct node *
ThrowStatement_parse(parser)
	struct parser *parser;
{
	struct Unary_node *n;

	n = NEW_NODE(struct Unary_node, NODECLASS_ThrowStatement);
	EXPECT(tTHROW);
	if (NEXT_FOLLOWS_NL)
		ERRORm("newline not allowed after 'throw'");
	n->a = PARSE(Expression);
	EXPECT_SEMICOLON;
	return (struct node *)n;
}

/*
 *	-- 12.14
 *
 *	TryStatement
 *	:	tTRY Block Catch
 *	|	tTRY Block Finally
 *	|	tTRY Block Catch Finally
 *	;
 *
 *	Catch
 *	:	tCATCH '(' tIDENT ')' Block
 *	;
 *
 *	Finally
 *	:	tFINALLY Block
 *	;
 */

#if WITH_PARSER_EVAL
/*
 * Helper function to evaluate the catch clause in a new scope.
 * Return true if an exception was caught while executing the
 * catch clause.
 */
static int
TryStatement_catch(n, context, C, res, ctxt)
	struct TryStatement_node *n;
	struct SEE_context *context;
	struct SEE_value *C, *res;
	SEE_try_context_t *ctxt;
{
	struct SEE_object *r2;
	struct SEE_scope *s;
	struct SEE_interpreter *interp = context->interpreter;

	r2 = SEE_Object_new(interp);
	SEE_OBJECT_PUT(interp, r2, n->ident, C, SEE_ATTR_DONTDELETE);
	s = SEE_NEW(interp, struct SEE_scope);
	s->obj = r2;
	s->next = context->scope;
	context->scope = s;
	SEE_TRY(interp, *ctxt)
	    EVAL(n->bcatch, context, res);
	context->scope = context->scope->next;
	return (int)SEE_CAUGHT(*ctxt);
}

static void
TryStatement_catch_eval(na, context, res)
	struct node *na; /* (struct TryStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	SEE_try_context_t block_ctxt, catch_ctxt;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	SEE_TRY(context->interpreter, block_ctxt)
		EVAL(n->block, context, res);
	if (SEE_CAUGHT(block_ctxt))
		if (TryStatement_catch(n, context, SEE_CAUGHT(block_ctxt),
			res, &catch_ctxt)) 
		{
		    TRACE(&na->location, context, SEE_TRACE_THROW);
		    SEE_RETHROW(context->interpreter, catch_ctxt);
		} 
}
#endif

#if WITH_PARSER_CODEGEN
static void
TryStatement_catch_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	SEE_code_patchable_t L1, L2;
	int in_scope;

	CG_LOC(&na->location);
	CG_STRING(n->ident);	    /* str */
	CG_S_TRYC_f(L1);	    /* - */
	cg_block_enter(cc);
	CODEGEN(n->block);	    /* - */
	CG_B_ALWAYS_f(L2);
    CG_LABEL(L1);
	in_scope = cg_var_is_in_scope(cc, n->ident);
	if (in_scope)
	    cg_var_set_scope(cc, n->ident, 0);
	CODEGEN(n->bcatch);	    /* - */
	if (in_scope)
	    cg_var_set_scope(cc, n->ident, 1);
    CG_LABEL(L2);
	CG_END(cg_block_current(cc));
	cg_block_leave(cc);

	na->maxstack = MAX3(1, n->block->maxstack, n->bcatch->maxstack);

}
#endif





#if WITH_PARSER_EVAL
static void
TryStatement_finally_eval(na, context, res)
	struct node *na; /* (struct TryStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	struct SEE_value r2;
	SEE_try_context_t ctxt;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	SEE_TRY(context->interpreter, ctxt)
	    EVAL(n->block, context, res);
	EVAL(n->bfinally, context, &r2);
	if (SEE_VALUE_GET_TYPE(&r2) == SEE_COMPLETION &&
		r2.u.completion.type != SEE_COMPLETION_NORMAL)
	    SEE_VALUE_COPY(res, &r2); 		/* break, return etc */
	else if (SEE_CAUGHT(ctxt)) {
	    TRACE(&na->location, context, SEE_TRACE_THROW);
	    SEE_RETHROW(context->interpreter, ctxt);
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
TryStatement_finally_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	SEE_code_patchable_t L1, L2;

	CG_LOC(&na->location);
	CG_S_TRYF_f(L1);	    /* - */
	cg_block_enter(cc);
	CODEGEN(n->block);	    /* - */
	CG_B_ALWAYS_f(L2);
    CG_LABEL(L1);
	CG_GETC();		    /* val */
	CODEGEN(n->bfinally);	    /* val */
	CG_SETC();		    /* - */
    CG_LABEL(L2);
	CG_END(cg_block_current(cc));
	cg_block_leave(cc);

	na->maxstack = MAX3(1, n->block->maxstack, 1 + n->bfinally->maxstack);

}
#endif





#if WITH_PARSER_EVAL
static void
TryStatement_catchfinally_eval(na, context, res)
	struct node *na; /* (struct TryStatement_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	struct SEE_value r6;
	SEE_try_context_t block_ctxt, finally_ctxt, catch_ctxt, *C = NULL;
	struct SEE_interpreter *interp = context->interpreter;

	TRACE(&na->location, context, SEE_TRACE_STATEMENT);
	SEE_TRY(interp, block_ctxt)
/*1*/		EVAL(n->block, context, res);
/*3*/	if (SEE_CAUGHT(block_ctxt))  {
		C = &block_ctxt;
/*4*/		if (TryStatement_catch(n, context, SEE_CAUGHT(block_ctxt),
			res, &catch_ctxt))
/*5*/		    C = &catch_ctxt;
		else
		    C = NULL;
	}

	SEE_TRY(interp, finally_ctxt)
/*6*/		EVAL(n->bfinally, context, &r6);
	if (SEE_CAUGHT(finally_ctxt))
		C = &finally_ctxt;
	else if (SEE_VALUE_GET_TYPE(&r6) == SEE_COMPLETION &&
		    r6.u.completion.type != SEE_COMPLETION_NORMAL)
		SEE_VALUE_COPY(res, &r6);	/* break, return etc */

	if (C) {
		TRACE(&na->location, context, SEE_TRACE_THROW);
		SEE_RETHROW(interp, *C);
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
TryStatement_catchfinally_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	SEE_code_patchable_t L1, L2, L3a, L3b;
	int in_scope;

	CG_LOC(&na->location);
	CG_S_TRYF_f(L1);	    /* - */
	cg_block_enter(cc);
	CG_STRING(n->ident);	    /* str */
	CG_S_TRYC_f(L2);	    /* - */
	cg_block_enter(cc);
	CODEGEN(n->block);	    /* - */
	CG_B_ALWAYS_f(L3a);
    CG_LABEL(L2);
	in_scope = cg_var_is_in_scope(cc, n->ident);
	if (in_scope)
	    cg_var_set_scope(cc, n->ident, 0);
	CODEGEN(n->bcatch);	    /* - */
	if (in_scope)
	    cg_var_set_scope(cc, n->ident, 1);
	CG_B_ALWAYS_f(L3b);
    CG_LABEL(L1);
	CG_GETC();		    /* val */
	CODEGEN(n->bfinally);	    /* val */
	CG_SETC();		    /* - */
    CG_LABEL(L3a);
    CG_LABEL(L3b);
	cg_block_leave(cc);
	CG_END(cg_block_current(cc));
	cg_block_leave(cc);

	na->maxstack = MAX4(1, n->block->maxstack, 
		n->bcatch->maxstack, 1 + n->bfinally->maxstack);


	/*
	 * Peephole optimizer note:
	 * Sometimes two SETCs will be generated in a row. This
	 * would be slightly faster if the first SETC were converted
	 * into a POP
	 */

}
#endif





static struct node *
TryStatement_parse(parser)
	struct parser *parser;
{
	struct TryStatement_node *n;
	enum nodeclass_enum nc;
        struct node *block, *bcatch, *bfinally;
        struct SEE_string *ident = NULL;

	EXPECT(tTRY);
	block = PARSE(Block);
	if (NEXT == tCATCH) {
	    SKIP;
	    EXPECT('(');
	    if (NEXT == tIDENT)
		    ident = NEXT_VALUE->u.string;
	    EXPECT(tIDENT);
	    EXPECT(')');
	    bcatch = PARSE(Block);
	} else
	    bcatch = NULL;

	if (NEXT == tFINALLY) {
	    SKIP;
	    bfinally = PARSE(Block);
	} else
	    bfinally = NULL;

	if (bcatch && bfinally)
		nc = NODECLASS_TryStatement_catchfinally;
	else if (bcatch)
		nc = NODECLASS_TryStatement_catch;
	else if (bfinally)
		nc = NODECLASS_TryStatement_finally;
	else
		ERRORm("expected 'catch' or 'finally'");

	n = NEW_NODE(struct TryStatement_node, nc);
        n->block = block;
        n->bcatch = bcatch;
        n->bfinally = bfinally;
        n->ident = ident;

	return (struct node *)n;
}

/*
 *	-- 13
 *
 *	FunctionDeclaration
 *	:	tFUNCTION tIDENT '( ')' '{' FunctionBody '}'
 *	|	tFUNCTION tIDENT '( FormalParameterList ')' 
 *			'{' FunctionBody '}'
 *	;
 *
 *	FunctionExpression
 *	:	tFUNCTION '( ')' '{' FunctionBody '}'
 *	|	tFUNCTION '( FormalParameterList ')' '{' FunctionBody '}'
 *	|	tFUNCTION tIDENT '( ')' '{' FunctionBody '}'
 *	|	tFUNCTION tIDENT '( FormalParameterList ')' 
 *			'{' FunctionBody '}'
 *	;
 *
 *	FormalParameterList
 *	:	tIDENT
 *	|	FormalParameterList ',' tIDENT
 *	;
 *
 *	FunctionBody
 *	:	SourceElements
 *	;
 */

#if 0
/* This is never called, but defined in the spec. (Spec bug?) */
#if WITH_PARSER_EVAL
static void
FunctionDeclaration_eval(na, context, res)
	struct node *na; /* (struct Function_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Function_node *n = CAST_NODE(na, Function);
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, NULL, NO_TARGET); /* 14 */
}
#endif

#if WITH_PARSER_CODEGEN
static void
FunctionDeclaration_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Function_node *n = CAST_NODE(na, Function);
	/* TBD */
}
#endif
#endif



#if WITH_PARSER_EVAL
static void
FunctionDeclaration_fproc(na, context)
	struct node *na; /* struct Function_node */
	struct SEE_context *context;
{
	struct Function_node *n = CAST_NODE(na, Function);
	struct SEE_object *funcobj;
	struct SEE_value   funcval;

	/* 10.1.3 */
	funcobj = SEE_function_inst_create(context->interpreter,
	    n->function, context->scope);
	SEE_SET_OBJECT(&funcval, funcobj);
	SEE_OBJECT_PUT(context->interpreter, context->variable,
	    n->function->name, &funcval, context->varattr);
}
#endif

static struct node *
FunctionDeclaration_parse(parser)
	struct parser *parser;
{
	struct Function_node *n;
	struct node *body;
	struct var *formal;
	struct SEE_string *name = NULL;

	n = NEW_NODE(struct Function_node, NODECLASS_FunctionDeclaration);
	EXPECT(tFUNCTION);

	if (NEXT == tIDENT)
		name = NEXT_VALUE->u.string;
	EXPECT(tIDENT);

	EXPECT('(');
	formal = PARSE(FormalParameterList);
	EXPECT(')');

	EXPECT('{');
	parser->funcdepth++;
	body = PARSE(FunctionBody);
	parser->funcdepth--;
	EXPECT('}');

	n->function = SEE_function_make(parser->interpreter, 
		name, formal, make_body(parser->interpreter, body, 0));

	return (struct node *)n;
}


#if WITH_PARSER_EVAL
static void
FunctionExpression_eval(na, context, res)
	struct node *na; /* (struct Function_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct Function_node *n = CAST_NODE(na, Function);
	struct SEE_object *funcobj = NULL, *obj;
	struct SEE_value   v;
	struct SEE_scope  *scope;
	SEE_try_context_t  ctxt;
	struct SEE_interpreter *interp = context->interpreter;

	if (n->function->name == NULL) {
	    funcobj = SEE_function_inst_create(interp,
	        n->function, context->scope);
            SEE_SET_OBJECT(res, funcobj);
	} else {
	    /*
	     * Construct a single scope step that lets the
	     * function call itself recursively
	     */
	    obj = SEE_Object_new(interp);

	    scope = SEE_NEW(interp, struct SEE_scope);
	    scope->obj = obj;
	    scope->next = context->scope;
	    context->scope = scope;

	    /* Be careful to restore the scope on any exception! */
	    SEE_TRY(interp, ctxt) {
	        funcobj = SEE_function_inst_create(interp,
	            n->function, context->scope);
	        SEE_SET_OBJECT(&v, funcobj);
	        SEE_OBJECT_PUT(interp, obj, n->function->name, &v,
		    SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY);
                SEE_SET_OBJECT(res, funcobj);
	    }
	    context->scope = context->scope->next;
	    SEE_DEFAULT_CATCH(interp, ctxt);	/* re-throw any exception */
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
FunctionExpression_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct Function_node *n = CAST_NODE(na, Function);
	int in_scope;

	if (n->function->name == NULL) {
	    CG_FUNC(n->function);	    /* obj */

	    na->maxstack = 1;
	} else {
	    /*
	     * The following creates a new mini-scope with S.WITH.
	     * The scope is inherited by the FUNC instruction. This is so
	     * the function can call itself recursively by name
	     */
	    CG_OBJECT();		    /* obj */
	    CG_DUP();			    /* obj obj */
	    CG_S_WITH();		    /* obj */
	    cg_block_enter(cc);
	    in_scope = cg_var_is_in_scope(cc, n->function->name);
	    if (in_scope)
		    cg_var_set_scope(cc, n->function->name, 0);
	    CG_STRING(n->function->name);   /* obj str */
	    CG_REF();			    /* ref */
	    CG_FUNC(n->function);	    /* ref obj */
	    CG_END(cg_block_current(cc));  
	    cg_block_leave(cc);
	    if (in_scope)
		    cg_var_set_scope(cc, n->function->name, 1);
	    CG_DUP();			    /* ref obj obj */
	    CG_ROLL3();			    /* obj ref obj */
	    CG_PUTVALUEA(SEE_ATTR_DONTDELETE | SEE_ATTR_READONLY); /* obj */

	    na->maxstack = 3;
	}
}
#endif

static struct node *
FunctionExpression_parse(parser)
	struct parser *parser;
{
	struct Function_node *n;
	struct var *formal;
	int noin_save, is_lhs_save;
	struct SEE_string *name;
	struct node *body;

	/* Save parser state */
	noin_save = parser->noin;
	is_lhs_save = parser->is_lhs;
	parser->noin = 0;
	parser->is_lhs = 0;

	n = NEW_NODE(struct Function_node, NODECLASS_FunctionExpression);
	EXPECT(tFUNCTION);
	if (NEXT == tIDENT) {
		name = NEXT_VALUE->u.string;
		SKIP;
	} else
		name = NULL;
	EXPECT('(');
	formal = PARSE(FormalParameterList);
	EXPECT(')');

	EXPECT('{');
	parser->funcdepth++;
	body = PARSE(FunctionBody);
	parser->funcdepth--;
	EXPECT('}');

	n->function = SEE_function_make(parser->interpreter,
		name, formal, make_body(parser->interpreter, body, 0));

	/* Restore parser state */
	parser->noin = noin_save;
	parser->is_lhs = is_lhs_save;

	return (struct node *)n;
}


static struct var *
FormalParameterList_parse(parser)
	struct parser *parser;
{
	struct var **p;
	struct var *result;

	p = &result;

	if (NEXT == tIDENT) {
	    *p = SEE_NEW(parser->interpreter, struct var);
	    (*p)->name = NEXT_VALUE->u.string;
	    p = &(*p)->next;
	    SKIP;
	    while (NEXT == ',') {
		SKIP;
		if (NEXT == tIDENT) {
		    *p = SEE_NEW(parser->interpreter, struct var);
		    (*p)->name = NEXT_VALUE->u.string;
		    p = &(*p)->next;
		}
		EXPECT(tIDENT);
	    }
	}
	*p = NULL;
	return result;
}


#if WITH_PARSER_EVAL
static void
FunctionBody_eval(na, context, res)
	struct node *na; /* (struct Unary_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct FunctionBody_node *n = CAST_NODE(na, FunctionBody);
	struct SEE_value v;

	FPROC(n->u.a, context);
	EVAL(n->u.a, context, &v);

	SEE_ASSERT(context->interpreter,
	    SEE_VALUE_GET_TYPE(&v) == SEE_COMPLETION);
	SEE_ASSERT(context->interpreter,
	    v.u.completion.type == SEE_COMPLETION_NORMAL ||
	    v.u.completion.type == SEE_COMPLETION_RETURN);

	/* Functions convert 'normal' completion to 'return undefined',
	 * while Programs return the value from their last 'normal' 
	 * completion. */
	if ((!n->is_program && v.u.completion.type == SEE_COMPLETION_NORMAL) ||
		v.u.completion.value == NULL)
	    SEE_SET_UNDEFINED(res);
	else
	    SEE_VALUE_COPY(res, v.u.completion.value);
}
#endif

#if WITH_PARSER_CODEGEN
static void
FunctionBody_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct FunctionBody_node *n = CAST_NODE(na, FunctionBody);

	/* Note that SourceElements_codegen includes the fproc action */
	CODEGEN(n->u.a);	/* - */

	/* Non-programs convert 'normal' completion to return undefined */
	if (!n->is_program) {
	    CG_UNDEFINED();	/* undef */
	    CG_SETC();		/* - */
	}
	CG_END(0);		/* explicit return */

	na->maxstack = MAX(n->is_program ? 0 : 1, n->u.a->maxstack);
}
#endif

static struct node *
FunctionBody_make(interp, source_elements, is_program)
	struct SEE_interpreter *interp;
	struct node *source_elements;
	int is_program;
{
	struct FunctionBody_node *n;

	n = NEW_NODE_INTERNAL(interp, struct FunctionBody_node, 
	    NODECLASS_FunctionBody);
	n->u.a = source_elements;
	n->is_program = is_program;
	return (struct node *)n;
}

static struct node *
FunctionBody_parse(parser)
	struct parser *parser;
{
        struct FunctionBody_node *n;

	n = NEW_NODE(struct FunctionBody_node, NODECLASS_FunctionBody);
	n->u.a = PARSE(SourceElements);
	n->is_program = 0;
	return (struct node *)n;
}

/*
 * JavaScript 1.5 function statements. (Not part of ECMA-262, which
 * treats functions as declarations.) The statement 
 * 'function foo (args) { body };' is treated syntactically
 * equivalent to 'foo = function foo (args) { body };' The Netscape
 * documentation calls these 'conditional functions', as their intent
 * is to be used like this:
 *    if (0) function foo() { abc };
 *    else   function foo() { xyz };
 */
static struct node *
FunctionStatement_parse(parser)
	struct parser *parser;
{
	struct Function_node *f;
	struct PrimaryExpression_ident_node *i;
	struct AssignmentExpression_node *an;
	struct Unary_node *e;

	f = (struct Function_node *)FunctionExpression_parse(parser);

	i = NEW_NODE(struct PrimaryExpression_ident_node,
		NODECLASS_PrimaryExpression_ident);
	i->string = f->function->name;

	an = NEW_NODE(struct AssignmentExpression_node, 
			NODECLASS_AssignmentExpression_simple);
	an->lhs = (struct node *)i;
	an->expr = (struct node *)f;

	e = NEW_NODE(struct Unary_node, NODECLASS_ExpressionStatement);
	e->a = (struct node *)an;

	return (struct node *)e;
}

/*
 *	-- 14
 *
 *	Program
 *	:	SourceElements
 *	;
 *
 *
 *	SourceElements
 *	:	SourceElement
 *	|	SourceElements SourceElement
 *	;
 *
 *
 *	SourceElement
 *	:	Statement
 *	|	FunctionDeclaration
 *	;
 */

static struct function *
Program_parse(parser)
	struct parser *parser;
{
	struct node *body;
	struct FunctionBody_node *f;

	/*
	 * NB: The semantics of Program are indistinguishable from that of
	 * a FunctionBody. Syntactically, the only difference is that
	 * Program must be followed by the tEND (end-of-input) token.
	 * Practically, a program does not have parameters nor a name,
	 * and its 'this' is always set to the Global object.
	 */
	body = PARSE(FunctionBody);
	if (NEXT == '}')
		ERRORm("unmatched '}'");
	if (NEXT == ')')
		ERRORm("unmatched ')'");
	if (NEXT == ']')
		ERRORm("unmatched ']'");
	if (NEXT != tEND)
		ERRORm("unexpected token");

	f = CAST_NODE(body, FunctionBody);
	f->is_program = 1;

	return SEE_function_make(parser->interpreter,
		NULL, NULL, make_body(parser->interpreter, body, 0));
}

#if WITH_PARSER_EVAL
static void
SourceElements_eval(na, context, res)
	struct node *na; /* (struct SourceElements_node) */
	struct SEE_context *context;
	struct SEE_value *res;
{
	struct SourceElements_node *n = CAST_NODE(na, SourceElements);
	struct SourceElement *e;

	/*
	 * NB: strictly, this should 'evaluate' the
	 * FunctionDeclarations, but they only yield <NORMAL, NULL, NULL>
	 * so, we don't. We just run the non-functiondecl statements
	 * instead. It has the same result.
	 */
	_SEE_SET_COMPLETION(res, SEE_COMPLETION_NORMAL, NULL, NO_TARGET);
	for (e = n->statements; e; e = e->next) {
		EVAL(e->node, context, res);
		if (res->u.completion.type != SEE_COMPLETION_NORMAL)
			break;
	}
}
#endif

#if WITH_PARSER_CODEGEN
static void
SourceElements_codegen(na, cc)
	struct node *na;
	struct code_context *cc;
{
	struct SourceElements_node *n = CAST_NODE(na, SourceElements);
	unsigned int maxstack = 0;
	struct SourceElement *e;
	struct var *v;
	struct Function_node *fn;

	/* SourceElements fproc:
	 * - create function closures of the current scope
	 * - initialise 'var's.
	 */

	for (e = n->functions; e; e = e->next) {
	    fn = CAST_NODE(e->node, Function);
	    cg_var_set_scope(cc, fn->function->name, 1);
	    CG_VREF(cg_var_id(cc, fn->function->name)); /* ref */
	    CG_FUNC(fn->function);		        /* ref obj */
	    CG_PUTVALUE();			        /* - */
	    maxstack = MAX(maxstack, 2);
	}

	for (v = n->vars; v; v = v->next) {
	    cg_var_set_scope(cc, v->name, 1);
	    maxstack = MAX(maxstack, 1);
	}

	/* SourceElements eval:
	 * - execute each statement
	 */
	for (e = n->statements; e; e = e->next) {
	    CODEGEN(e->node);
	    maxstack = MAX(maxstack, e->node->maxstack);
	}

	na->maxstack = maxstack;
}
#endif

#if WITH_PARSER_EVAL
static void
SourceElements_fproc(na, context)
	struct node *na; /* struct SourceElements_node */
	struct SEE_context *context;
{
	struct SourceElements_node *n = CAST_NODE(na, SourceElements);
	struct SourceElement *e;
	struct var *v;
	struct SEE_value undefv;

	for (e = n->functions; e; e = e->next)
		FPROC(e->node, context);

	/*
	 * spec bug(?): although not mentioned in the spec, this
	 * is the place to set the declared variables
	 * to undefined. (10.1.3). 
	 * (I say 'spec bug' because there is partial overlap
	 * between 10.1.3 and the semantics of 13.)
	 */
	SEE_SET_UNDEFINED(&undefv);
	for (v = n->vars; v; v = v->next)
	    if (!SEE_OBJECT_HASPROPERTY(context->interpreter,
		context->variable, v->name))
	            SEE_OBJECT_PUT(context->interpreter, context->variable, 
			v->name, &undefv, context->varattr);
}
#endif



/* Builds a simple SourceElements around a single statement */
static struct node *
SourceElements_make1(interp, statement)
	struct SEE_interpreter *interp;
	struct node *statement;
{
	struct SourceElements_node *ss;
	struct SourceElement *s;

	s = SEE_NEW(interp, struct SourceElement);
	s->node = statement;
	s->next = NULL;
	ss = NEW_NODE_INTERNAL(interp, struct SourceElements_node, 
		NODECLASS_SourceElements); 
	ss->statements = s;
	ss->functions = NULL;
	ss->vars = NULL;
	return (struct node *)ss;
}

static struct node *
SourceElements_parse(parser)
	struct parser *parser;
{
	struct SourceElements_node *se;
	struct SourceElement **s, **f;
	struct var **vars_save;

	se = NEW_NODE(struct SourceElements_node, NODECLASS_SourceElements); 
	s = &se->statements;
	f = &se->functions;

	/* Whenever a VarDecl parses, it will get added to se->vars! */
	vars_save = parser->vars;
	parser->vars = &se->vars;

	for (;;) 
	    switch (NEXT) {
	    case tFUNCTION:
		if (lookahead(parser, 1) != '(') {
		    *f = SEE_NEW(parser->interpreter, struct SourceElement);
		    (*f)->node = PARSE(FunctionDeclaration);
		    f = &(*f)->next;
#ifndef NDEBUG
		    if (SEE_parse_debug) 
		        dprintf("SourceElements_parse: got function\n");
#endif
		    break;
		}
		/* else it's a function expression */
	    /* The 'first's of Statement */
	    case tTHIS: case tIDENT: case tSTRING: case tNUMBER:
	    case tNULL: case tTRUE: case tFALSE:
	    case '(': case '[': case '{':
	    case tNEW: case tDELETE: case tVOID: case tTYPEOF:
	    case tPLUSPLUS: case tMINUSMINUS:
	    case '+': case '-': case '~': case '!': case ';':
	    case tVAR: case tIF: case tDO: case tWHILE: case tFOR:
	    case tCONTINUE: case tBREAK: case tRETURN:
	    case tWITH: case tSWITCH: case tTHROW: case tTRY:
	    case tDIV: case tDIVEQ: /* in lieu of tREGEX */
		*s = SEE_NEW(parser->interpreter, struct SourceElement);
		(*s)->node = PARSE(Statement);
		s = &(*s)->next;
#ifndef NDEBUG
		if (SEE_parse_debug)
		    dprintf("SourceElements_parse: got statement\n");
#endif
		break;
	    case tEND:
	    default:
#ifndef NDEBUG
		if (SEE_parse_debug)
		    dprintf("SourceElements_parse: got EOF/other (%d)\n", 
		    	NEXT);
#endif
		*s = NULL;
		*f = NULL;
		*parser->vars = NULL;
		parser->vars = vars_save;

		return (struct node *)se;
	    }
}

/*------------------------------------------------------------
 * Public API
 */

/*
 * Parses a function declaration in two parts and
 * return a function structure, in a similar way to
 * FunctionDeclaration_parse() when called with the
 * right input.
 */
struct function *
SEE_parse_function(interp, name, paraminp, bodyinp)
	struct SEE_interpreter *interp;
	struct SEE_string *name;
	struct SEE_input *paraminp, *bodyinp;
{
	struct lex lex;
	struct parser parservar, *parser = &parservar;
	struct var *formal;
	struct node *body;

	if (paraminp) {
		SEE_lex_init(&lex, SEE_input_lookahead(paraminp, 6));
		parser_init(parser, interp, &lex);
		formal = PARSE(FormalParameterList);	/* handles "" too */
		EXPECT_NOSKIP(tEND);			/* uses parser var */
	} else
		formal = NULL;

	if (bodyinp) 
		SEE_lex_init(&lex, SEE_input_lookahead(bodyinp, 6));
	else {
		/* Set the lexer to EOF quickly */
		lex.input = NULL;
		lex.next = tEND;
	}
	parser_init(parser, interp, &lex);
	parser->funcdepth++;
	body = PARSE(FunctionBody);
	parser->funcdepth--;
	EXPECT_NOSKIP(tEND);

	return SEE_function_make(interp, name, formal, 
		make_body(interp, body, 0));
}

/*
 * Parses a Program. 
 * Does not close the input, but may consume up to 6 characters.
 * lookahead. This is not usually a problem, because the input is
 * always read to EOF on normal completion.
 */
struct function *
SEE_parse_program(interp, inp)
	struct SEE_interpreter *interp;
	struct SEE_input *inp;
{
	struct lex lex;
	struct parser localparse, *parser = &localparse;
	struct function *f;

	SEE_lex_init(&lex, SEE_input_lookahead(inp, 6));
	parser_init(parser, interp, &lex);
	f = PARSE(Program);

#if !defined(NDEBUG) && WITH_PARSER_PRINT && !WITH_PARSER_CODEGEN
	if (SEE_parse_debug) {
	    dprintf("parse Program result:\n");
            _SEE_parser_print(
                _SEE_parser_print_stdio_new(interp, stderr),
                (struct node *)f->body);
	    dprintf("<end>\n");
	}
#endif

	return f;
}

/*
 * Evaluates the function body with the given execution context. 
 * Function body must not be NULL
 */
static void
eval_functionbody(body, context, res)
	void *body;
	struct SEE_context *context;
	struct SEE_value *res;
{
#if WITH_PARSER_CODEGEN
	CG_EXEC((struct SEE_code *)body, context, res);
#else
	EVAL((struct node *)body, context, res);
#endif
}

/* Evaluates the function body with the given execution context. */
void
SEE_eval_functionbody(f, context, res)
	struct function *f;
	struct SEE_context *context;
	struct SEE_value *res;
{
	if (f && f->body)
	    eval_functionbody(f->body, context, res);
	else
	    SEE_SET_UNDEFINED(res);
	SEE_ASSERT(context->interpreter,
	    SEE_VALUE_GET_TYPE(res) != SEE_COMPLETION);
	SEE_ASSERT(context->interpreter,
	    SEE_VALUE_GET_TYPE(res) != SEE_REFERENCE);
}

int
SEE_functionbody_isempty(interp, f)
	struct SEE_interpreter *interp;
	struct function *f;
{
#if WITH_PARSER_CODEGEN
	return f->body == NULL;
#else
	return FunctionBody_isempty(interp, (struct node *)f->body);
#endif
}

/* Returns true if the FunctionBody is empty. */
static int
FunctionBody_isempty(interp, body)
	struct SEE_interpreter *interp;
	struct node *body;
{
	struct SourceElements_node *se;
	struct FunctionBody_node *f;
	
	f = CAST_NODE(body, FunctionBody);
	se = CAST_NODE(f->u.a, SourceElements);
	return se->statements == NULL && 
	       se->vars == NULL &&
	       (!f->is_program || se->functions == NULL);
}


/* Returns the function body as a string */
struct SEE_string *
SEE_functionbody_string(interp, f)
	struct SEE_interpreter *interp;
	struct function *f;
{
	struct SEE_string *s = SEE_string_new(interp, 0);

#if WITH_PARSER_PRINT && !WITH_PARSER_CODEGEN
        _SEE_parser_print(_SEE_parser_print_string_new(interp, s),
	        (struct node *)f->body);
#else
	SEE_string_addch(s, '/');
	SEE_string_addch(s, '*');
	SEE_string_append_int(s, (int)f);
	SEE_string_addch(s, '*');
	SEE_string_addch(s, '/');
#endif
	return s;
}

/*------------------------------------------------------------
 * eval
 *  -- 15.1.2.1
 */

/*
 * Global.eval()
 * 'Eval' is a special function (not a cfunction), because it accesses 
 * the execution context of the caller (which is not available to 
 * functions and methods invoked via SEE_OBJECT_CALL()).
 *
 * This normally only ever get called from CallExpression_eval().
 * A stub cfunction exists for Global.eval, but it is bypassed.
 */
static void
eval(context, thisobj, argc, argv, res)
	struct SEE_context *context;
	struct SEE_object *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct SEE_input *inp;
	struct function *f;
	struct SEE_context evalcontext;
	struct SEE_interpreter *interp = context->interpreter;

	if (argc == 0) {
		SEE_SET_UNDEFINED(res);
		return;
	}
	if (SEE_VALUE_GET_TYPE(argv[0]) != SEE_STRING) {
		SEE_VALUE_COPY(res, argv[0]);
		return;
	}
	
	inp = SEE_input_string(interp, argv[0]->u.string);
	inp->filename = STR(eval_input_name);
	f = SEE_parse_program(interp, inp);
	SEE_INPUT_CLOSE(inp);

	/* 10.2.2 */
	evalcontext.interpreter = interp;
	evalcontext.activation = context->activation;	/* XXX */
	evalcontext.variable = context->variable;
	evalcontext.varattr = 0;
	evalcontext.thisobj = context->thisobj;
	evalcontext.scope = context->scope;

	if (SEE_COMPAT_JS(interp, >=, JS11)	/* EXT:23 */
	    && thisobj && thisobj != interp->Global) 
	{
		/*
		 * support eval() being called from something
		 * other than the global object, where the 'thisobj'
		 * becomes the scope chain and variable object
		 */
		evalcontext.thisobj = thisobj;
		evalcontext.variable = thisobj;
		evalcontext.scope = SEE_NEW(interp, struct SEE_scope);
		evalcontext.scope->next = context->scope;
		evalcontext.scope->obj = thisobj;
	}

	/* Set formal params to undefined, if any exist -- redundant? */
	SEE_function_put_args(context, f, 0, NULL);

	/* Evaluate the statement */
	SEE_eval_functionbody(f, &evalcontext, res);
}

/* 
 * Evaluates an expression in the given context.
 * This is a helper function intended for external debuggers wanting 
 * to evaluate user expressions in a given context.
 */
void
SEE_context_eval(context, expr, res)
	struct SEE_context *context;
	struct SEE_string *expr;
	struct SEE_value *res;
{
	struct SEE_value s, *argv[1];

	argv[0] = &s;
	SEE_SET_STRING(argv[0], expr);
	eval(context, context->thisobj, 1, argv, res);
}

/*
 * Compares two value using ECMAScript == and > operator semantics.
 * Returns  0 if x == y,
 *          1 if x > y or indeterminate,
 *         -1 otherwise.
 * This could be used as a better comparsion function for Array.sort().
 * Currently only used by RegExp.prototype.test()
 */
int
SEE_compare(interp, x, y)
	struct SEE_interpreter *interp;
	struct SEE_value *x, *y;
{
	struct SEE_value v;

	EqualityExpression_eq(interp, x, y, &v);
	if (v.u.boolean)
		return 0;
	RelationalExpression_sub(interp, x, y, &v);
	if (SEE_VALUE_GET_TYPE(&v) == SEE_UNDEFINED || !v.u.boolean)
		return 1;
	else
		return -1;
}

#if WITH_PARSER_EVAL
/*
 * Table of evaluators used when executable ASTs are enabled
 */
void (*_SEE_nodeclass_eval[NODECLASS_MAX])(struct node *, 
        struct SEE_context *, struct SEE_value *) = { 0
    ,0                                      /*Unary*/
    ,0                                      /*Binary*/
    ,Literal_eval                           /*Literal*/
    ,StringLiteral_eval                     /*StringLiteral*/
    ,RegularExpressionLiteral_eval          /*RegularExpressionLiteral*/
    ,PrimaryExpression_this_eval            /*PrimaryExpression_this*/
    ,PrimaryExpression_ident_eval           /*PrimaryExpression_ident*/
    ,ArrayLiteral_eval                      /*ArrayLiteral*/
    ,ObjectLiteral_eval                     /*ObjectLiteral*/
    ,Arguments_eval                         /*Arguments*/
    ,MemberExpression_new_eval              /*MemberExpression_new*/
    ,MemberExpression_dot_eval              /*MemberExpression_dot*/
    ,MemberExpression_bracket_eval          /*MemberExpression_bracket*/
    ,CallExpression_eval                    /*CallExpression*/
    ,PostfixExpression_inc_eval             /*PostfixExpression_inc*/
    ,PostfixExpression_dec_eval             /*PostfixExpression_dec*/
    ,UnaryExpression_delete_eval            /*UnaryExpression_delete*/
    ,UnaryExpression_void_eval              /*UnaryExpression_void*/
    ,UnaryExpression_typeof_eval            /*UnaryExpression_typeof*/
    ,UnaryExpression_preinc_eval            /*UnaryExpression_preinc*/
    ,UnaryExpression_predec_eval            /*UnaryExpression_predec*/
    ,UnaryExpression_plus_eval              /*UnaryExpression_plus*/
    ,UnaryExpression_minus_eval             /*UnaryExpression_minus*/
    ,UnaryExpression_inv_eval               /*UnaryExpression_inv*/
    ,UnaryExpression_not_eval               /*UnaryExpression_not*/
    ,MultiplicativeExpression_mul_eval      /*MultiplicativeExpression_mul*/
    ,MultiplicativeExpression_div_eval      /*MultiplicativeExpression_div*/
    ,MultiplicativeExpression_mod_eval      /*MultiplicativeExpression_mod*/
    ,AdditiveExpression_add_eval            /*AdditiveExpression_add*/
    ,AdditiveExpression_sub_eval            /*AdditiveExpression_sub*/
    ,ShiftExpression_lshift_eval            /*ShiftExpression_lshift*/
    ,ShiftExpression_rshift_eval            /*ShiftExpression_rshift*/
    ,ShiftExpression_urshift_eval           /*ShiftExpression_urshift*/
    ,RelationalExpression_lt_eval           /*RelationalExpression_lt*/
    ,RelationalExpression_gt_eval           /*RelationalExpression_gt*/
    ,RelationalExpression_le_eval           /*RelationalExpression_le*/
    ,RelationalExpression_ge_eval           /*RelationalExpression_ge*/
    ,RelationalExpression_instanceof_eval   /*RelationalExpression_instanceof*/
    ,RelationalExpression_in_eval           /*RelationalExpression_in*/
    ,EqualityExpression_eq_eval             /*EqualityExpression_eq*/
    ,EqualityExpression_ne_eval             /*EqualityExpression_ne*/
    ,EqualityExpression_seq_eval            /*EqualityExpression_seq*/
    ,EqualityExpression_sne_eval            /*EqualityExpression_sne*/
    ,BitwiseANDExpression_eval              /*BitwiseANDExpression*/
    ,BitwiseXORExpression_eval              /*BitwiseXORExpression*/
    ,BitwiseORExpression_eval               /*BitwiseORExpression*/
    ,LogicalANDExpression_eval              /*LogicalANDExpression*/
    ,LogicalORExpression_eval               /*LogicalORExpression*/
    ,ConditionalExpression_eval             /*ConditionalExpression*/
    ,0                                      /*AssignmentExpression*/
    ,AssignmentExpression_simple_eval       /*AssignmentExpression_simple*/
    ,AssignmentExpression_muleq_eval        /*AssignmentExpression_muleq*/
    ,AssignmentExpression_diveq_eval        /*AssignmentExpression_diveq*/
    ,AssignmentExpression_modeq_eval        /*AssignmentExpression_modeq*/
    ,AssignmentExpression_addeq_eval        /*AssignmentExpression_addeq*/
    ,AssignmentExpression_subeq_eval        /*AssignmentExpression_subeq*/
    ,AssignmentExpression_lshifteq_eval     /*AssignmentExpression_lshifteq*/
    ,AssignmentExpression_rshifteq_eval     /*AssignmentExpression_rshifteq*/
    ,AssignmentExpression_urshifteq_eval    /*AssignmentExpression_urshifteq*/
    ,AssignmentExpression_andeq_eval        /*AssignmentExpression_andeq*/
    ,AssignmentExpression_xoreq_eval        /*AssignmentExpression_xoreq*/
    ,AssignmentExpression_oreq_eval         /*AssignmentExpression_oreq*/
    ,Expression_comma_eval                  /*Expression_comma*/
    ,Block_empty_eval                       /*Block_empty*/
    ,StatementList_eval                     /*StatementList*/
    ,VariableStatement_eval                 /*VariableStatement*/
    ,VariableDeclarationList_eval           /*VariableDeclarationList*/
    ,VariableDeclaration_eval               /*VariableDeclaration*/
    ,EmptyStatement_eval                    /*EmptyStatement*/
    ,ExpressionStatement_eval               /*ExpressionStatement*/
    ,IfStatement_eval                       /*IfStatement*/
    ,IterationStatement_dowhile_eval        /*IterationStatement_dowhile*/
    ,IterationStatement_while_eval          /*IterationStatement_while*/
    ,IterationStatement_for_eval            /*IterationStatement_for*/
    ,IterationStatement_forvar_eval         /*IterationStatement_forvar*/
    ,IterationStatement_forin_eval          /*IterationStatement_forin*/
    ,IterationStatement_forvarin_eval       /*IterationStatement_forvarin*/
    ,ContinueStatement_eval                 /*ContinueStatement*/
    ,BreakStatement_eval                    /*BreakStatement*/
    ,ReturnStatement_eval                   /*ReturnStatement*/
    ,ReturnStatement_undef_eval             /*ReturnStatement_undef*/
    ,WithStatement_eval                     /*WithStatement*/
    ,SwitchStatement_eval                   /*SwitchStatement*/
    ,LabelledStatement_eval                 /*LabelledStatement*/
    ,ThrowStatement_eval                    /*ThrowStatement*/
    ,0                                      /*TryStatement*/
    ,TryStatement_catch_eval                /*TryStatement_catch*/
    ,TryStatement_finally_eval              /*TryStatement_finally*/
    ,TryStatement_catchfinally_eval         /*TryStatement_catchfinally*/
    ,0                                      /*Function*/
    ,0 /* FunctionDeclaration_eval */       /*FunctionDeclaration*/
    ,FunctionExpression_eval                /*FunctionExpression*/
    ,FunctionBody_eval                      /*FunctionBody*/
    ,SourceElements_eval                    /*SourceElements*/
};
#endif

#ifdef WITH_PARSER_CODEGEN
void (*_SEE_nodeclass_codegen[NODECLASS_MAX])(struct node *, 
        struct code_context *) = { 0
    ,0                                      /*Unary*/
    ,0                                      /*Binary*/
    ,Literal_codegen                        /*Literal*/
    ,StringLiteral_codegen                  /*StringLiteral*/
    ,RegularExpressionLiteral_codegen       /*RegularExpressionLiteral*/
    ,PrimaryExpression_this_codegen         /*PrimaryExpression_this*/
    ,PrimaryExpression_ident_codegen        /*PrimaryExpression_ident*/
    ,ArrayLiteral_codegen                   /*ArrayLiteral*/
    ,ObjectLiteral_codegen                  /*ObjectLiteral*/
    ,Arguments_codegen                      /*Arguments*/
    ,MemberExpression_new_codegen           /*MemberExpression_new*/
    ,MemberExpression_dot_codegen           /*MemberExpression_dot*/
    ,MemberExpression_bracket_codegen       /*MemberExpression_bracket*/
    ,CallExpression_codegen                 /*CallExpression*/
    ,PostfixExpression_inc_codegen          /*PostfixExpression_inc*/
    ,PostfixExpression_dec_codegen          /*PostfixExpression_dec*/
    ,UnaryExpression_delete_codegen         /*UnaryExpression_delete*/
    ,UnaryExpression_void_codegen           /*UnaryExpression_void*/
    ,UnaryExpression_typeof_codegen         /*UnaryExpression_typeof*/
    ,UnaryExpression_preinc_codegen         /*UnaryExpression_preinc*/
    ,UnaryExpression_predec_codegen         /*UnaryExpression_predec*/
    ,UnaryExpression_plus_codegen           /*UnaryExpression_plus*/
    ,UnaryExpression_minus_codegen          /*UnaryExpression_minus*/
    ,UnaryExpression_inv_codegen            /*UnaryExpression_inv*/
    ,UnaryExpression_not_codegen            /*UnaryExpression_not*/
    ,MultiplicativeExpression_mul_codegen   /*MultiplicativeExpression_mul*/
    ,MultiplicativeExpression_div_codegen   /*MultiplicativeExpression_div*/
    ,MultiplicativeExpression_mod_codegen   /*MultiplicativeExpression_mod*/
    ,AdditiveExpression_add_codegen         /*AdditiveExpression_add*/
    ,AdditiveExpression_sub_codegen         /*AdditiveExpression_sub*/
    ,ShiftExpression_lshift_codegen         /*ShiftExpression_lshift*/
    ,ShiftExpression_rshift_codegen         /*ShiftExpression_rshift*/
    ,ShiftExpression_urshift_codegen        /*ShiftExpression_urshift*/
    ,RelationalExpression_lt_codegen        /*RelationalExpression_lt*/
    ,RelationalExpression_gt_codegen        /*RelationalExpression_gt*/
    ,RelationalExpression_le_codegen        /*RelationalExpression_le*/
    ,RelationalExpression_ge_codegen        /*RelationalExpression_ge*/
    ,RelationalExpression_instanceof_codegen/*RelationalExpression_instanceof*/
    ,RelationalExpression_in_codegen        /*RelationalExpression_in*/
    ,EqualityExpression_eq_codegen          /*EqualityExpression_eq*/
    ,EqualityExpression_ne_codegen          /*EqualityExpression_ne*/
    ,EqualityExpression_seq_codegen         /*EqualityExpression_seq*/
    ,EqualityExpression_sne_codegen         /*EqualityExpression_sne*/
    ,BitwiseANDExpression_codegen           /*BitwiseANDExpression*/
    ,BitwiseXORExpression_codegen           /*BitwiseXORExpression*/
    ,BitwiseORExpression_codegen            /*BitwiseORExpression*/
    ,LogicalANDExpression_codegen           /*LogicalANDExpression*/
    ,LogicalORExpression_codegen            /*LogicalORExpression*/
    ,ConditionalExpression_codegen          /*ConditionalExpression*/
    ,0                                      /*AssignmentExpression*/
    ,AssignmentExpression_simple_codegen    /*AssignmentExpression_simple*/
    ,AssignmentExpression_muleq_codegen     /*AssignmentExpression_muleq*/
    ,AssignmentExpression_diveq_codegen     /*AssignmentExpression_diveq*/
    ,AssignmentExpression_modeq_codegen     /*AssignmentExpression_modeq*/
    ,AssignmentExpression_addeq_codegen     /*AssignmentExpression_addeq*/
    ,AssignmentExpression_subeq_codegen     /*AssignmentExpression_subeq*/
    ,AssignmentExpression_lshifteq_codegen  /*AssignmentExpression_lshifteq*/
    ,AssignmentExpression_rshifteq_codegen  /*AssignmentExpression_rshifteq*/
    ,AssignmentExpression_urshifteq_codegen /*AssignmentExpression_urshifteq*/
    ,AssignmentExpression_andeq_codegen     /*AssignmentExpression_andeq*/
    ,AssignmentExpression_xoreq_codegen     /*AssignmentExpression_xoreq*/
    ,AssignmentExpression_oreq_codegen      /*AssignmentExpression_oreq*/
    ,Expression_comma_codegen               /*Expression_comma*/
    ,Block_empty_codegen                    /*Block_empty*/
    ,StatementList_codegen                  /*StatementList*/
    ,VariableStatement_codegen              /*VariableStatement*/
    ,VariableDeclarationList_codegen        /*VariableDeclarationList*/
    ,VariableDeclaration_codegen            /*VariableDeclaration*/
    ,EmptyStatement_codegen                 /*EmptyStatement*/
    ,ExpressionStatement_codegen            /*ExpressionStatement*/
    ,IfStatement_codegen                    /*IfStatement*/
    ,IterationStatement_dowhile_codegen     /*IterationStatement_dowhile*/
    ,IterationStatement_while_codegen       /*IterationStatement_while*/
    ,IterationStatement_for_codegen         /*IterationStatement_for*/
    ,IterationStatement_forvar_codegen      /*IterationStatement_forvar*/
    ,IterationStatement_forin_codegen       /*IterationStatement_forin*/
    ,IterationStatement_forvarin_codegen    /*IterationStatement_forvarin*/
    ,ContinueStatement_codegen              /*ContinueStatement*/
    ,BreakStatement_codegen                 /*BreakStatement*/
    ,ReturnStatement_codegen                /*ReturnStatement*/
    ,ReturnStatement_undef_codegen          /*ReturnStatement_undef*/
    ,WithStatement_codegen                  /*WithStatement*/
    ,SwitchStatement_codegen                /*SwitchStatement*/
    ,LabelledStatement_codegen              /*LabelledStatement*/
    ,ThrowStatement_codegen                 /*ThrowStatement*/
    ,0                                      /*TryStatement*/
    ,TryStatement_catch_codegen             /*TryStatement_catch*/
    ,TryStatement_finally_codegen           /*TryStatement_finally*/
    ,TryStatement_catchfinally_codegen      /*TryStatement_catchfinally*/
    ,0                                      /*Function*/
    ,0                                      /*FunctionDeclaration*/
    ,FunctionExpression_codegen             /*FunctionExpression*/
    ,FunctionBody_codegen                   /*FunctionBody*/
    ,SourceElements_codegen                 /*SourceElements*/
};
#endif



/*
 * isconst functions return true if the expression node will always evaluate
 * to the same value; that is, it is a constant expression
 */
int (*_SEE_nodeclass_isconst[NODECLASS_MAX])(struct node *, 
        struct SEE_interpreter *) = { 0
    ,Unary_isconst                          /*Unary*/
    ,Binary_isconst                         /*Binary*/
    ,Always_isconst                         /*Literal*/
    ,Always_isconst                         /*StringLiteral*/
    ,0                                      /*RegularExpressionLiteral*/
    ,0                                      /*PrimaryExpression_this*/
    ,0                                      /*PrimaryExpression_ident*/
    ,0                                      /*ArrayLiteral*/
    ,0                                      /*ObjectLiteral*/
    ,Arguments_isconst                      /*Arguments*/
    ,0                                      /*MemberExpression_new*/
    ,0                                      /*MemberExpression_dot*/
    ,0                                      /*MemberExpression_bracket*/
    ,0                                      /*CallExpression*/
    ,0                                      /*PostfixExpression_inc*/
    ,0                                      /*PostfixExpression_dec*/
    ,Unary_isconst                          /*UnaryExpression_delete*/
    ,Unary_isconst                          /*UnaryExpression_void*/
    ,Unary_isconst                          /*UnaryExpression_typeof*/
    ,0                                      /*UnaryExpression_preinc*/
    ,0                                      /*UnaryExpression_predec*/
    ,Unary_isconst                          /*UnaryExpression_plus*/
    ,Unary_isconst                          /*UnaryExpression_minus*/
    ,Unary_isconst                          /*UnaryExpression_inv*/
    ,Unary_isconst                          /*UnaryExpression_not*/
    ,Binary_isconst                         /*MultiplicativeExpression_mul*/
    ,Binary_isconst                         /*MultiplicativeExpression_div*/
    ,Binary_isconst                         /*MultiplicativeExpression_mod*/
    ,Binary_isconst                         /*AdditiveExpression_add*/
    ,Binary_isconst                         /*AdditiveExpression_sub*/
    ,Binary_isconst                         /*ShiftExpression_lshift*/
    ,Binary_isconst                         /*ShiftExpression_rshift*/
    ,Binary_isconst                         /*ShiftExpression_urshift*/
    ,Binary_isconst                         /*RelationalExpression_lt*/
    ,Binary_isconst                         /*RelationalExpression_gt*/
    ,Binary_isconst                         /*RelationalExpression_le*/
    ,Binary_isconst                         /*RelationalExpression_ge*/
    ,Binary_isconst                         /*RelationalExpression_instanceof*/
    ,Binary_isconst                         /*RelationalExpression_in*/
    ,Binary_isconst                         /*EqualityExpression_eq*/
    ,Binary_isconst                         /*EqualityExpression_ne*/
    ,Binary_isconst                         /*EqualityExpression_seq*/
    ,Binary_isconst                         /*EqualityExpression_sne*/
    ,Binary_isconst                         /*BitwiseANDExpression*/
    ,Binary_isconst                         /*BitwiseXORExpression*/
    ,Binary_isconst                         /*BitwiseORExpression*/
    ,LogicalANDExpression_isconst           /*LogicalANDExpression*/
    ,LogicalORExpression_isconst            /*LogicalORExpression*/
    ,ConditionalExpression_isconst          /*ConditionalExpression*/
    ,0                                      /*AssignmentExpression*/
    ,0                                      /*AssignmentExpression_simple*/
    ,0                                      /*AssignmentExpression_muleq*/
    ,0                                      /*AssignmentExpression_diveq*/
    ,0                                      /*AssignmentExpression_modeq*/
    ,0                                      /*AssignmentExpression_addeq*/
    ,0                                      /*AssignmentExpression_subeq*/
    ,0                                      /*AssignmentExpression_lshifteq*/
    ,0                                      /*AssignmentExpression_rshifteq*/
    ,0                                      /*AssignmentExpression_urshifteq*/
    ,0                                      /*AssignmentExpression_andeq*/
    ,0                                      /*AssignmentExpression_xoreq*/
    ,0                                      /*AssignmentExpression_oreq*/
    ,Binary_isconst                         /*Expression_comma*/
    ,0                                      /*Block_empty*/
    ,0                                      /*StatementList*/
    ,0                                      /*VariableStatement*/
    ,0                                      /*VariableDeclarationList*/
    ,0                                      /*VariableDeclaration*/
    ,0                                      /*EmptyStatement*/
    ,0                                      /*ExpressionStatement*/
    ,0                                      /*IfStatement*/
    ,0                                      /*IterationStatement_dowhile*/
    ,0                                      /*IterationStatement_while*/
    ,0                                      /*IterationStatement_for*/
    ,0                                      /*IterationStatement_forvar*/
    ,0                                      /*IterationStatement_forin*/
    ,0                                      /*IterationStatement_forvarin*/
    ,0                                      /*ContinueStatement*/
    ,0                                      /*BreakStatement*/
    ,0                                      /*ReturnStatement*/
    ,0                                      /*ReturnStatement_undef*/
    ,0                                      /*WithStatement*/
    ,0                                      /*SwitchStatement*/
    ,0                                      /*LabelledStatement*/
    ,0                                      /*ThrowStatement*/
    ,0                                      /*TryStatement*/
    ,0                                      /*TryStatement_catch*/
    ,0                                      /*TryStatement_finally*/
    ,0                                      /*TryStatement_catchfinally*/
    ,0                                      /*Function*/
    ,0                                      /*FunctionDeclaration*/
    ,0                                      /*FunctionExpression*/
    ,0                                      /*FunctionBody*/
    ,0                                      /*SourceElements*/
};

