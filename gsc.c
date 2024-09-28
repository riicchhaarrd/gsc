#include "visitor.h"
#include "lexer.h"

typedef struct
{
	int depth;
} ASTVisitorContext;

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

static void print_operator(int op)
{
	// printf("op:%d\n",op);
	if(op < 255)
	{
		printf("%c", op & 0xff);
		// if(op == '.' || op == '[')
		// 	printf("%c", op & 0xff);
		// else
		// 	printf(" %c ", op & 0xff);
	}
	else
	{
		static const char *operators[TK_MAX] = {
			[TK_LSHIFT] = "<<",		 [TK_RSHIFT] = ">>",		 [TK_LOGICAL_AND] = "&&",
			[TK_LOGICAL_OR] = "||",	 [TK_RSHIFT_ASSIGN] = ">>=", [TK_LSHIFT_ASSIGN] = "<<=",
			[TK_AND_ASSIGN] = "&=",	 [TK_OR_ASSIGN] = "|=",		 [TK_INCREMENT] = "++",
			[TK_DECREMENT] = "--",	 [TK_DIV_ASSIGN] = "/=",	 [TK_MUL_ASSIGN] = "*=",
			[TK_MOD_ASSIGN] = "%=",	 [TK_XOR_ASSIGN] = "^=",	 [TK_MINUS_ASSIGN] = "-=",
			[TK_PLUS_ASSIGN] = "+=", [TK_EQUAL] = "==",			 [TK_NEQUAL] = "!=",
			[TK_GEQUAL] ">=",		 [TK_LEQUAL] = "<=",		 NULL
		};
		if(op == TK_INCREMENT || op == TK_DECREMENT)
			printf("%s", operators[op]);
		else
			printf(" %s ", operators[op] ? operators[op] : "?");
	}
}
IMPL_VISIT(ASTMemberExpr)
{
	visit_node(v, n->object);
	print_operator(n->op);
	visit_node(v, n->prop);
	if(n->op == '[')
	{
		printf("]");
	}
}
// IMPL_VISIT(ASTLocalizedString)
// {
// 	printf("%s", n->reference);
// }
IMPL_VISIT(ASTUnaryExpr)
{
	print_operator(n->op);
	visit_node(v, n->argument);
}

IMPL_VISIT(ASTExprStmt)
{
	ASTVisitorContext *ctx = (ASTVisitorContext *)v->ctx;
	ctx->depth++;
	indent_printf(v, "");
	visit_node(v, n->expression);
	printf(";\n");
	ctx->depth--;
}
IMPL_VISIT(ASTReturnStmt)
{
	if(!n->argument)
		indent_printf(v, "return;");
	else
	{
		indent_printf(v, "return ");
		visit_node(v, n->argument);
		printf(";");
	}
	printf("\n");
}
IMPL_VISIT(ASTIfStmt)
{
	printf("\n");
	indent_printf(v, "if(");
	visit_node(v, n->test);
	printf(")\n");

	visit_node(v, n->consequent);

	if(n->alternative)
	{
		indent_printf(v, "else\n");
		visit_node(v, n->alternative);
	}
}
IMPL_VISIT(ASTFileReference)
{
	printf("%s", n->file);
}
IMPL_VISIT(ASTWhileStmt)
{
	printf("\n");
	indent_printf(v, "while(");
	visit_node(v, n->test);
	printf(")\n");

	visit_node(v, n->body);
}
IMPL_VISIT(ASTWaitStmt)
{
	indent_printf(v, "wait ");
	visit_node(v, n->duration);
	printf(";\n");
}
IMPL_VISIT(ASTSwitchCase)
{
	printf("\n");
	if(!n->test)
	{
		indent_printf(v, "default:");
	}
	else
	{
		indent_printf(v, "case ");
		visit_node(v, n->test);
		printf(":\n");
	}
	ASTNode **it = &n->consequent;
	while(*it)
	{
		visit_node(v, *it);
		it = &((*it)->next);
	}
	// visit_node(v, n->consequent);
}

IMPL_VISIT(ASTSwitchStmt)
{
	printf("\n");
	indent_printf(v, "switch(");
	visit_node(v, n->discriminant);
	printf(")\n");

	ASTNode **it = &n->cases;
	while(*it)
	{
		visit_node(v, *it);
		it = &((*it)->next);
	}
}
IMPL_VISIT(ASTForStmt)
{
	printf("\n");
	indent_printf(v, "for(");
	visit_node(v, n->init);
	printf("; ");
	visit_node(v, n->test);
	printf("; ");
	visit_node(v, n->update);
	indent_printf(v, ")\n");

	visit_node(v, n->body);
}
IMPL_VISIT(ASTBreakStmt)
{
	indent_printf(v, "break;");
}
IMPL_VISIT(ASTContinueStmt)
{
	indent_printf(v, "continue;");
}
IMPL_VISIT(ASTGroupExpr)
{
	printf("(");
	visit_node(v, n->expression);
	printf(")");
}
// IMPL_VISIT(ASTFunctionPtr)
// {
// 	printf("::%s", n->function_name);
// }
IMPL_VISIT(ASTLiteral)
{
	switch(n->type)
	{
		case AST_LITERAL_TYPE_FLOAT:
		{
			printf("%f", n->value.number);
		}
		break;
		case AST_LITERAL_TYPE_BOOLEAN:
		{
			printf("%s", n->value.boolean ? "true" : "false");
		}
		break;
		case AST_LITERAL_TYPE_INTEGER:
		{
			printf("%d", n->value.integer);
		}
		break;
		case AST_LITERAL_TYPE_STRING:
		{
			printf("\"%s\"", n->value.string);
		}
		break;
		case AST_LITERAL_TYPE_ANIMATION:
		{
			printf("%%s", n->value.string);
		}
		break;
		case AST_LITERAL_TYPE_LOCALIZED_STRING:
		{
			printf("&\"%s\"", n->value.string);
		}
		break;
		case AST_LITERAL_TYPE_UNDEFINED:
		{
			printf("undefined");
		}
		break;
		case AST_LITERAL_TYPE_FUNCTION:
		{
			if(n->value.function.file)
				visit_node(v, n->value.function.file);
			printf("::");
			visit_node(v, n->value.function.function);
		}
		break;
		default:
		{
			printf("unhandled literal %d", n->type);
			exit(-1);
		}
		break;
	}
}
IMPL_VISIT(ASTBinaryExpr)
{
	visit_node(v, n->lhs);
	print_operator(n->op);
	visit_node(v, n->rhs);
}
IMPL_VISIT(ASTBlockStmt)
{
	ASTVisitorContext *ctx = (ASTVisitorContext *)v->ctx;
	ctx->depth++;
	indent_printf(v, "{\n");
	ASTNode **it = &n->body;
	while(*it)
	{
		visit_node(v, *it);
		it = &((*it)->next);
	}
	// for(size_t i = 0; i < n->numbody; ++i)
	// {
	// 	visit_node(v, n->body[i]);
	// }
	indent_printf(v, "}\n");
	ctx->depth--;
}
IMPL_VISIT(ASTAssignmentExpr)
{
	visit_node(v, n->lhs);
	print_operator(n->op);
	visit_node(v, n->rhs);
}
IMPL_VISIT(ASTVectorExpr)
{
	printf("(");
	for(size_t i = 0; i < n->numelements; ++i)
	{
		visit_node(v, n->elements[i]);
		if(i + 1 < n->numelements)
			printf(", ");
	}
	printf(")");
}
IMPL_VISIT(ASTIdentifier)
{
	// if(n->file_reference[0] != 0)
	// 	printf("%s::", n->file_reference);
	printf("%s", n->name);
}
IMPL_VISIT(ASTArrayExpr)
{
	printf("[");
	for(size_t i = 0; i < n->numelements; ++i)
	{
		visit_node(v, n->elements[i]);
	}
	printf("]");
}
IMPL_VISIT(ASTFunction)
{
	printf("%s()\n", n->name);
	visit_node(v, n->body);
}
IMPL_VISIT(ASTCallExpr)
{
	visit_node(v, n->callee);
	printf("(");
	for(size_t i = 0; i < n->numarguments; ++i)
	{
		visit_node(v, n->arguments[i]);
		if(i + 1 < n->numarguments)
			printf(", ");
	}
	printf(")");
}
static void visit_fallback(ASTVisitor *v, ASTNode *n, const char *type)
{
	printf("Node '%s' not implemented\n", type);
	exit(-1);
}

static ASTVisitorContext ctx = { 0 };

void ast_visitor_gsc_init(ASTVisitor *v)
{
	v->ctx = &ctx;

	v->visit_fallback = visit_fallback;
	v->visit_ast_block_stmt = visit_ASTBlockStmt_;
	v->visit_ast_call_expr = visit_ASTCallExpr_;
	v->visit_ast_identifier = visit_ASTIdentifier_;
	v->visit_ast_literal = visit_ASTLiteral_;
	// v->visit_ast_localized_string = visit_ASTLocalizedString_;
	v->visit_ast_member_expr = visit_ASTMemberExpr_;
	v->visit_ast_unary_expr = visit_ASTUnaryExpr_;
	v->visit_ast_expr_stmt = visit_ASTExprStmt_;
	v->visit_ast_binary_expr = visit_ASTBinaryExpr_;
	v->visit_ast_group_expr = visit_ASTGroupExpr_;
	v->visit_ast_assignment_expr = visit_ASTAssignmentExpr_;
	v->visit_ast_function = visit_ASTFunction_;
	v->visit_ast_if_stmt = visit_ASTIfStmt_;
	v->visit_ast_vector_expr = visit_ASTVectorExpr_;
	v->visit_ast_return_stmt = visit_ASTReturnStmt_;
	v->visit_ast_file_reference = visit_ASTFileReference_;
	v->visit_ast_for_stmt = visit_ASTForStmt_;
	v->visit_ast_while_stmt = visit_ASTWhileStmt_;
	v->visit_ast_switch_stmt = visit_ASTSwitchStmt_;
	v->visit_ast_switch_case = visit_ASTSwitchCase_;
	v->visit_ast_break_stmt = visit_ASTBreakStmt_;
	v->visit_ast_continue_stmt = visit_ASTContinueStmt_;
	v->visit_ast_wait_stmt = visit_ASTWaitStmt_;
	v->visit_ast_array_expr = visit_ASTArrayExpr_;
	// v->visit_ast_function_ptr = visit_ASTFunctionPtr_;
}