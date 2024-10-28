#include "ast.h"
#include "traverse.h"

typedef struct
{
    void *ctx;
    TraverseNodeFn callback;
    // bool (*match)(ASTNode*);
    // ASTNode *results[16];
    // size_t numresults;
} TraverseContext;

#define FWD(UPPER, TYPE, LOWER) static void visit_ ## TYPE(TYPE*, TraverseContext *ctx);
AST_X_MACRO(FWD)

typedef void (*VisitorFn)(ASTNode*, TraverseContext*);

#undef VISITORS
#define VISITORS(UPPER, TYPE, LOWER) [UPPER] = (VisitorFn)visit_ ## TYPE,

static const VisitorFn visitors[] = {
    AST_X_MACRO(VISITORS) NULL
};

static void visit_(ASTNode *n, void *ctx)
{
	if(!n)
		return;
	visitors[n->type](n, ctx);
}
#define visit(x) visit_(x, ctx)

#define DEFINE_VISITOR(TYPE)                                \
	static void visit_##TYPE(TYPE *n, TraverseContext *ctx) \
	{                                                       \
		if(!n)                                              \
			return;                                         \
		ctx->callback((ASTNode*)n, ctx->ctx);
#define DEFINE_VISITOR_END() }

DEFINE_VISITOR(ASTFunction)
{
    for(ASTNode *it = n->parameters; it; it = it->next)
    {
        visit(it);
    }
    visit(n->body);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTFunctionPointerExpr)
{
    visit(n->expression);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTSelf)
{
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTStructExpr)
{
    for(size_t i = 0; i < n->numelements; ++i)
		visit(n->elements[i]);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTArrayExpr)
{
    for(size_t i = 0; i < n->numelements; ++i)
		visit(n->elements[i]);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTGroupExpr)
{
    visit(n->expression);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTAssignmentExpr)
{
    visit(n->lhs);
    visit(n->rhs);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTBinaryExpr)
{
    visit(n->lhs);
    visit(n->rhs);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTCallExpr)
{
    visit(n->object);
    visit(n->callee);
    for(size_t i = 0; i < n->numarguments; ++i)
		visit(n->arguments[i]);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTConditionalExpr)
{
    visit(n->condition);
    visit(n->consequent);
    visit(n->alternative);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTFileReference)
{
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTIdentifier)
{
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTLiteral)
{
    if(n->type == AST_LITERAL_TYPE_FUNCTION)
    {
        visit(n->value.function.function);
        visit(n->value.function.file);
    }
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTMemberExpr)
{
    visit(n->object);
    visit(n->prop);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTUnaryExpr)
{
    visit(n->argument);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTVectorExpr)
{
    for(size_t i = 0; i < n->numelements; ++i)
		visit(n->elements[i]);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTBlockStmt)
{
    for(ASTNode *it = (ASTNode*)n->body; it; it = it->next)
    {
        visit(it);
    }
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTBreakStmt)
{
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTContinueStmt)
{
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTDoWhileStmt)
{
    visit(n->body);
    visit(n->test);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTEmptyStmt)
{
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTExprStmt)
{
    visit(n->expression);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTForStmt)
{
	visit(n->init);
	visit(n->test);
	visit(n->update);
	visit(n->body);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTIfStmt)
{
	visit(n->test);
	visit(n->consequent);
	visit(n->alternative);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTReturnStmt)
{
    visit(n->argument);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTSwitchCase)
{
    visit(n->test);
    for(ASTNode *it = n->consequent; it; it = it->next)
    {
        visit(it);
    }
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTSwitchStmt)
{
    for(ASTNode *it = (ASTNode*)n->cases; it; it = it->next)
    {
        visit(it);
    }
    visit(n->discriminant);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTWaitStmt)
{
    visit(n->duration);
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTWaitTillFrameEndStmt)
{
}
DEFINE_VISITOR_END()

DEFINE_VISITOR(ASTWhileStmt)
{
	visit(n->test);
	visit(n->body);
}
DEFINE_VISITOR_END()

void traverse(ASTNode *root, TraverseNodeFn node_fn, void *ctx_)
{
	TraverseContext ctx = { .ctx = ctx_, .callback = node_fn };
	visit_(root, &ctx);
}