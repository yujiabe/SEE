
typedef void (*visitor_fn_t)(struct node *, void *);

extern void (*_SEE_nodeclass_visit[])(struct node *, 
	visitor_fn_t, void *);

static void ArrayLiteral_visit(struct node *na, visitor_fn_t v, void *va);
static void ObjectLiteral_visit(struct node *na, visitor_fn_t v, void *va);
static void Arguments_visit(struct node *na, visitor_fn_t v, void *va);
static void MemberExpression_new_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void MemberExpression_dot_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void MemberExpression_bracket_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void CallExpression_visit(struct node *na, visitor_fn_t v, void *va);
static void Unary_visit(struct node *na, visitor_fn_t v, void *va);
static void Binary_visit(struct node *na, visitor_fn_t v, void *va);
static void ConditionalExpression_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void AssignmentExpression_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void VariableDeclaration_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void IfStatement_visit(struct node *na, visitor_fn_t v, void *va);
static void IterationStatement_while_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void IterationStatement_for_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void IterationStatement_forin_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void ReturnStatement_visit(struct node *na, visitor_fn_t v, void *va);
static void SwitchStatement_visit(struct node *na, visitor_fn_t v, void *va);
static void TryStatement_catch_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void TryStatement_finally_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void TryStatement_catchfinally_visit(struct node *na, visitor_fn_t v, 
        void *va);
static void Function_visit(struct node *na, visitor_fn_t v, void *va);
static void SourceElements_visit(struct node *na, visitor_fn_t v, void *va);


/*
 * Visitor macro
 */
# define VISITFN(n)  (_SEE_nodeclass_visit[(n)->nodeclass])
# define VISIT(n, v, va)	do {			\
	if (VISITFN(n))                			\
	    (*VISITFN(n))(n, v, va);		\
	(*(v))(n, va);					\
    } while (0)

static void
ArrayLiteral_visit(na, v, va)
	struct node *na; /* (struct ArrayLiteral_node) */
	visitor_fn_t v;
	void *va;
{
	struct ArrayLiteral_node *n = CAST_NODE(na, ArrayLiteral);
	struct ArrayLiteral_element *element;

	for (element = n->first; element; element = element->next)
		VISIT(element->expr, v, va);
}

static void
ObjectLiteral_visit(na, v, va)
	struct node *na; /* (struct ObjectLiteral_node) */
	visitor_fn_t v;
	void *va;
{
	struct ObjectLiteral_node *n = CAST_NODE(na, ObjectLiteral);
	struct ObjectLiteral_pair *pair;

	for (pair = n->first; pair; pair = pair->next)
		VISIT(pair->value, v, va);
}

static void
Arguments_visit(na, v, va)
	struct node *na; /* (struct Arguments_node) */
	visitor_fn_t v;
	void *va;
{
	struct Arguments_node *n = CAST_NODE(na, Arguments);
	struct Arguments_arg *arg;

	for (arg = n->first; arg; arg = arg->next)
		VISIT(arg->expr, v, va);
}

static void
MemberExpression_new_visit(na, v, va)
	struct node *na; /* (struct MemberExpression_new_node) */
	visitor_fn_t v;
	void *va;
{
	struct MemberExpression_new_node *n = 
		CAST_NODE(na, MemberExpression_new);
	VISIT(n->mexp, v, va);
	if (n->args)
		VISIT((struct node *)n->args, v, va);
}

static void
MemberExpression_dot_visit(na, v, va)
	struct node *na; /* (struct MemberExpression_dot_node) */
	visitor_fn_t v;
	void *va;
{
	struct MemberExpression_dot_node *n = 
		CAST_NODE(na, MemberExpression_dot);
	VISIT(n->mexp, v, va);
}

static void
MemberExpression_bracket_visit(na, v, va)
	struct node *na; /* (struct MemberExpression_bracket_node) */
	visitor_fn_t v;
	void *va;
{
	struct MemberExpression_bracket_node *n = 
		CAST_NODE(na, MemberExpression_bracket);
	VISIT(n->mexp, v, va);
	VISIT(n->name, v, va);
}

static void
CallExpression_visit(na, v, va)
	struct node *na; /* (struct CallExpression_node) */
	visitor_fn_t v;
	void *va;
{
	struct CallExpression_node *n = CAST_NODE(na, CallExpression);
	VISIT(n->exp, v, va);
	VISIT((struct node *)n->args, v, va);
}

static void
Unary_visit(na, v, va)
	struct node *na; /* (struct Unary_node) */
	visitor_fn_t v;
	void *va;
{
	struct Unary_node *n = CAST_NODE(na, Unary);
	VISIT(n->a, v, va);
}

static void
Binary_visit(na, v, va)
	struct node *na; /* (struct Binary_node) */
	visitor_fn_t v;
	void *va;
{
	struct Binary_node *n = CAST_NODE(na, Binary);
	VISIT(n->a, v, va);
	VISIT(n->b, v, va);
}

static void
ConditionalExpression_visit(na, v, va)
	struct node *na; /* (struct ConditionalExpression_node) */
	visitor_fn_t v;
	void *va;
{
	struct ConditionalExpression_node *n = 
		CAST_NODE(na, ConditionalExpression);
	VISIT(n->a, v, va);
	VISIT(n->b, v, va);
	VISIT(n->c, v, va);
}

static void
AssignmentExpression_visit(na, v, va)
	struct node *na; /* (struct AssignmentExpression_node) */
	visitor_fn_t v;
	void *va;
{
	struct AssignmentExpression_node *n = 
		CAST_NODE(na, AssignmentExpression);
	VISIT(n->lhs, v, va);
	VISIT(n->expr, v, va);
}

static void
VariableDeclaration_visit(na, v, va)
	struct node *na; /* (struct VariableDeclaration_node) */
	visitor_fn_t v;
	void *va;
{
	struct VariableDeclaration_node *n = 
		CAST_NODE(na, VariableDeclaration);
	if (n->init)
		VISIT(n->init, v, va);
}

static void
IfStatement_visit(na, v, va)
	struct node *na; /* (struct IfStatement_node) */
	visitor_fn_t v;
	void *va;
{
	struct IfStatement_node *n = CAST_NODE(na, IfStatement);
	VISIT(n->cond, v, va);
	VISIT(n->btrue, v, va);
	if (n->bfalse)
	    VISIT(n->bfalse, v, va);
}

static void
IterationStatement_while_visit(na, v, va)
        struct node *na; /* (struct IterationStatement_while_node) */
	visitor_fn_t v;
	void *va;
{
        struct IterationStatement_while_node *n = 
		CAST_NODE(na, IterationStatement_while);

	VISIT(n->cond, v, va);
	VISIT(n->body, v, va);
}

static void
IterationStatement_for_visit(na, v, va)
	struct node *na; /* (struct IterationStatement_for_node) */
	visitor_fn_t v;
	void *va;
{
	struct IterationStatement_for_node *n = 
		CAST_NODE(na, IterationStatement_for);

	if (n->init) VISIT(n->init, v, va);
	if (n->cond) VISIT(n->cond, v, va);
	if (n->incr) VISIT(n->incr, v, va);
	VISIT(n->body, v, va);
}

static void
IterationStatement_forin_visit(na, v, va)
	struct node *na; /* (struct IterationStatement_forin_node) */
	visitor_fn_t v;
	void *va;
{
	struct IterationStatement_forin_node *n = 
		CAST_NODE(na, IterationStatement_forin);
	VISIT(n->lhs, v, va);
	VISIT(n->list, v, va);
	VISIT(n->body, v, va);
}

static void
ReturnStatement_visit(na, v, va)
	struct node *na; /* (struct ReturnStatement_node) */
	visitor_fn_t v;
	void *va;
{
	struct ReturnStatement_node *n = CAST_NODE(na, ReturnStatement);
	VISIT(n->expr, v, va);
}

static void
SwitchStatement_visit(na, v, va)
	struct node *na; /* (struct SwitchStatement_node) */
	visitor_fn_t v;
	void *va;
{
	struct SwitchStatement_node *n = CAST_NODE(na, SwitchStatement);
	struct case_list *c;

	VISIT(n->cond, v, va);
	for (c = n->cases; c; c = c->next) {
		if (c->expr) 
		    VISIT(c->expr, v, va);
		VISIT(c->body, v, va);
	}
}

static void
TryStatement_catch_visit(na, v, va)
	struct node *na; /* (struct TryStatement_node) */
	visitor_fn_t v;
	void *va;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	VISIT(n->block, v, va);
	VISIT(n->bcatch, v, va);
}

static void
TryStatement_finally_visit(na, v, va)
	struct node *na; /* (struct TryStatement_node) */
	visitor_fn_t v;
	void *va;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	VISIT(n->block, v, va);
	VISIT(n->bfinally, v, va);
}

static void
TryStatement_catchfinally_visit(na, v, va)
	struct node *na; /* (struct TryStatement_node) */
	visitor_fn_t v;
	void *va;
{
	struct TryStatement_node *n = CAST_NODE(na, TryStatement);
	VISIT(n->block, v, va);
	VISIT(n->bcatch, v, va);
	VISIT(n->bfinally, v, va);
}

static void
Function_visit(na, v, va)
	struct node *na; /* (struct Function_node) */
	visitor_fn_t v;
	void *va;
{
	struct Function_node *n = CAST_NODE(na, Function);
	VISIT((struct node *)n->function->body, v, va);
}

static void
SourceElements_visit(na, v, va)
	struct node *na; /* (struct SourceElements_node) */
	visitor_fn_t v;
	void *va;
{
	struct SourceElements_node *n = CAST_NODE(na, SourceElements);
	struct SourceElement *e;

	for (e = n->functions; e; e = e->next)
		VISIT(e->node, v, va);
	for (e = n->statements; e; e = e->next)
		VISIT(e->node, v, va);
}

void (*_SEE_nodeclass_visit[NODECLASS_MAX])(struct node *, 
	visitor_fn_t, void *) = { 0
    ,Unary_visit                            /*Unary*/
    ,Binary_visit                           /*Binary*/
    ,0                                      /*Literal*/
    ,0                                      /*StringLiteral*/
    ,0                                      /*RegularExpressionLiteral*/
    ,0                                      /*PrimaryExpression_this*/
    ,0                                      /*PrimaryExpression_ident*/
    ,ArrayLiteral_visit                     /*ArrayLiteral*/
    ,ObjectLiteral_visit                    /*ObjectLiteral*/
    ,Arguments_visit                        /*Arguments*/
    ,MemberExpression_new_visit             /*MemberExpression_new*/
    ,MemberExpression_dot_visit             /*MemberExpression_dot*/
    ,MemberExpression_bracket_visit         /*MemberExpression_bracket*/
    ,CallExpression_visit                   /*CallExpression*/
    ,Unary_visit                            /*PostfixExpression_inc*/
    ,Unary_visit                            /*PostfixExpression_dec*/
    ,Unary_visit                            /*UnaryExpression_delete*/
    ,Unary_visit                            /*UnaryExpression_void*/
    ,Unary_visit                            /*UnaryExpression_typeof*/
    ,Unary_visit                            /*UnaryExpression_preinc*/
    ,Unary_visit                            /*UnaryExpression_predec*/
    ,Unary_visit                            /*UnaryExpression_plus*/
    ,Unary_visit                            /*UnaryExpression_minus*/
    ,Unary_visit                            /*UnaryExpression_inv*/
    ,Unary_visit                            /*UnaryExpression_not*/
    ,Binary_visit                           /*MultiplicativeExpression_mul*/
    ,Binary_visit                           /*MultiplicativeExpression_div*/
    ,Binary_visit                           /*MultiplicativeExpression_mod*/
    ,Binary_visit                           /*AdditiveExpression_add*/
    ,Binary_visit                           /*AdditiveExpression_sub*/
    ,Binary_visit                           /*ShiftExpression_lshift*/
    ,Binary_visit                           /*ShiftExpression_rshift*/
    ,Binary_visit                           /*ShiftExpression_urshift*/
    ,Binary_visit                           /*RelationalExpression_lt*/
    ,Binary_visit                           /*RelationalExpression_gt*/
    ,Binary_visit                           /*RelationalExpression_le*/
    ,Binary_visit                           /*RelationalExpression_ge*/
    ,Binary_visit                           /*RelationalExpression_instanceof*/
    ,Binary_visit                           /*RelationalExpression_in*/
    ,Binary_visit                           /*EqualityExpression_eq*/
    ,Binary_visit                           /*EqualityExpression_ne*/
    ,Binary_visit                           /*EqualityExpression_seq*/
    ,Binary_visit                           /*EqualityExpression_sne*/
    ,Binary_visit                           /*BitwiseANDExpression*/
    ,Binary_visit                           /*BitwiseXORExpression*/
    ,Binary_visit                           /*BitwiseORExpression*/
    ,Binary_visit                           /*LogicalANDExpression*/
    ,Binary_visit                           /*LogicalORExpression*/
    ,ConditionalExpression_visit            /*ConditionalExpression*/
    ,AssignmentExpression_visit             /*AssignmentExpression*/
    ,AssignmentExpression_visit             /*AssignmentExpression_simple*/
    ,AssignmentExpression_visit             /*AssignmentExpression_muleq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_diveq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_modeq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_addeq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_subeq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_lshifteq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_rshifteq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_urshifteq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_andeq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_xoreq*/
    ,AssignmentExpression_visit             /*AssignmentExpression_oreq*/
    ,Binary_visit                           /*Expression_comma*/
    ,0                                      /*Block_empty*/
    ,Binary_visit                           /*StatementList*/
    ,Unary_visit                            /*VariableStatement*/
    ,Binary_visit                           /*VariableDeclarationList*/
    ,VariableDeclaration_visit              /*VariableDeclaration*/
    ,0                                      /*EmptyStatement*/
    ,Unary_visit                            /*ExpressionStatement*/
    ,IfStatement_visit                      /*IfStatement*/
    ,IterationStatement_while_visit         /*IterationStatement_dowhile*/
    ,IterationStatement_while_visit         /*IterationStatement_while*/
    ,IterationStatement_for_visit           /*IterationStatement_for*/
    ,IterationStatement_for_visit           /*IterationStatement_forvar*/
    ,IterationStatement_forin_visit         /*IterationStatement_forin*/
    ,IterationStatement_forin_visit         /*IterationStatement_forvarin*/
    ,0                                      /*ContinueStatement*/
    ,0                                      /*BreakStatement*/
    ,ReturnStatement_visit                  /*ReturnStatement*/
    ,0                                      /*ReturnStatement_undef*/
    ,Binary_visit                           /*WithStatement*/
    ,SwitchStatement_visit                  /*SwitchStatement*/
    ,Unary_visit                            /*LabelledStatement*/
    ,Unary_visit                            /*ThrowStatement*/
    ,0                                      /*TryStatement*/
    ,TryStatement_catch_visit               /*TryStatement_catch*/
    ,TryStatement_finally_visit             /*TryStatement_finally*/
    ,TryStatement_catchfinally_visit        /*TryStatement_catchfinally*/
    ,Function_visit                         /*Function*/
    ,Function_visit                         /*FunctionDeclaration*/
    ,Function_visit                         /*FunctionExpression*/
    ,Unary_visit                            /*FunctionBody*/
    ,SourceElements_visit                   /*SourceElements*/
};

