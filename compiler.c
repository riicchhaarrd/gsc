#include "visitor.h"
#include "lexer.h"
#include <setjmp.h>

typedef struct
{
	int unused;
} Compiler;

typedef struct
{
	Compiler *c;
	int depth;
	jmp_buf *jmp;
} ASTVisitorContext;

static void error(ASTVisitor *v, const char *fmt, ...)
{
	ASTVisitorContext *ctx = (ASTVisitorContext *)v->ctx;
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[COMPILER] ERROR: %s", message);
	longjmp(*ctx->jmp, 1);
}

#define IMPL_VISIT(TYPE) static void visit_##TYPE##_(ASTVisitor *v, ASTNode *ast_node, TYPE *n)

static void indent_printf(ASTVisitor *v, const char *fmt, ...)
{
	ASTVisitorContext *ctx = (ASTVisitorContext *)v->ctx;
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("%.*s%s", ctx->depth, "\t", message);
}

static void visit_fallback(ASTVisitor *v, ASTNode *n, const char *type)
{
	printf("Node '%s' not implemented\n", type);
	exit(-1);
}

static ASTVisitorContext ctx = { 0 };

static void callee(ASTVisitor *v, ASTNode *n)
{
	switch(n->type)
	{
		case AST_IDENTIFIER:
		{
			printf("call %s\n", n->ast_identifier_data.name);
		}
		break;
		case AST_FUNCTION:
		{
			ASTLiteral *lit = &n->ast_literal_data;
			if(lit->type != AST_LITERAL_TYPE_FUNCTION)
				error(v, "Not a function");
			if(lit->value.function.file->type != AST_FILE_REFERENCE)
				error(v, "Not a file reference");
			if(lit->value.function.function->type != AST_IDENTIFIER)
				error(v, "Not a identifier");
			printf("call %s::%s\n", lit->value.function.file->ast_file_reference_data.file, lit->value.function.function->ast_identifier_data.name);
		}
		break;

		default:
		{
			error("Unsupported callee %s", ast_node_names[n->type]);
		}
		break;
	}
}

IMPL_VISIT(ASTLiteral)
{
	switch(n->type)
	{
		case AST_LITERAL_TYPE_INTEGER:
		{
			printf("push %d\n", n->value.integer);
		}
		break;
		case AST_LITERAL_TYPE_FLOAT:
		{
			printf("pushf %f\n", n->value.number);
		}
		break;

		default:
		{
			error("Unhandled literal %d", n->type);
		}
		break;
	}
}
IMPL_VISIT(ASTCallExpr)
{
	for(size_t i = 0; i < n->numarguments; ++i)
	{
		visit_node(v, n->arguments[i]);
	}
	callee(v, n->callee);
}
IMPL_VISIT(ASTExprStmt)
{
	visit_node(v, n->expression);
}
IMPL_VISIT(ASTBlockStmt)
{
	ASTNode **it = &n->body;
	while(*it)
	{
		visit_node(v, *it);
		it = &((*it)->next);
	}
}
IMPL_VISIT(ASTFunction)
{
	printf("push ebp // %s\n", n->name);
	printf("mov ebp, esp\n");
	visit_node(v, n->body);
	printf("pop ebp\n");
	printf("mov esp, ebp\n");
	printf("retn\n");
}
void ast_visitor_compiler_init(ASTVisitor *v)
{
	v->ctx = &ctx;
	jmp_buf jmp;
	if(setjmp(jmp))
	{
		return;
	}
	ctx.jmp = &jmp;
	v->visit_ast_function = visit_ASTFunction_;
	v->visit_ast_block_stmt = visit_ASTBlockStmt_;
	v->visit_ast_expr_stmt = visit_ASTExprStmt_;
	v->visit_ast_call_expr = visit_ASTCallExpr_;
	v->visit_ast_literal = visit_ASTLiteral_;

	v->visit_fallback = visit_fallback;
}