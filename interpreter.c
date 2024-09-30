#include "interpreter.h"
#include "visitor.h"
#include "lexer.h"

static void error(ASTVisitor *v, const char *fmt, ...)
{
	Interpreter *interp = (Interpreter*)v->ctx;
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[INTERP] ERROR: %s", message);
	if(interp->jmp)
		longjmp(*interp->jmp, 1);
}

#define IMPL_VISIT(TYPE) static void visit_##TYPE##_(ASTVisitor *v, ASTNode *ast_node, TYPE *n)

static void visit_fallback(ASTVisitor *v, ASTNode *n, const char *type)
{
	printf("Node '%s' not implemented\n", type);
	exit(-1);
}

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
			error(v, "Unsupported callee %s", ast_node_names[n->type]);
		}
		break;
	}
}

static size_t string(ASTVisitor *v, const char *str)
{
	Interpreter *ctx = (Interpreter*)v->ctx;
	HashTableEntry *entry = hash_table_insert(&ctx->stringtable, str);
	if(entry->value)
	{
		return *(size_t*)entry->value;
	}
	entry->value = malloc(sizeof(size_t));
	*(size_t*)entry->value = ctx->string_index++;
	return ctx->string_index - 1;
}

IMPL_VISIT(ASTLiteral)
{
	Interpreter *ctx = (Interpreter*)v->ctx;
	static const char *_[] = {

		"STRING", "INTEGER", "BOOLEAN", "FLOAT", "VECTOR", "ANIMATION", "FUNCTION", "LOCALIZED_STRING", "UNDEFINED"
	};
	printf("push %s ", _[n->type]);
	switch(n->type)
	{
		case AST_LITERAL_TYPE_ANIMATION:
		case AST_LITERAL_TYPE_LOCALIZED_STRING:
		case AST_LITERAL_TYPE_STRING:
		{
			printf("%d // %s", string(v, n->value.string), n->value.string);
		}
		break;
		case AST_LITERAL_TYPE_BOOLEAN:
		{
			printf("%d", n->value.boolean);
		}
		break;
		case AST_LITERAL_TYPE_INTEGER:
		{
			printf("%d", n->value.integer);
		}
		break;
		case AST_LITERAL_TYPE_FLOAT:
		{
			printf("%f", n->value.number);
		}
		break;

		default:
		{
			error(v, "Unhandled literal %d", n->type);
		}
		break;
	}
	printf("\n");
}
IMPL_VISIT(ASTIfStmt)
{
	Interpreter *ctx = (Interpreter*)v->ctx;
	// visit_node(v, n->test);
	// printf("test\n");
	// size_t skip = ctx->label++;
	// printf("jz %d\n", skip);
	// visit_node(v, n->consequent);
	// int jmp_label = -1;
	// if(n->alternative)
	// {
	// 	jmp_label = ctx->label++;
	// 	printf("const0\n");
	// 	printf("jmp %d\n", jmp_label);
	// }
	// printf("label %d\n", skip);
	// if(n->alternative)
	// {
	// 	printf("const1\n");
	// 	if(jmp_label != -1)
	// 	{
	// 		printf("label %d\n", jmp_label);
	// 	}
	// 	printf("test\n");
	// 	size_t skip2 = ctx->label++;
	// 	printf("jz %d\n", skip2);
	// 	visit_node(v, n->alternative);
	// 	printf("label %d\n", skip2);
	// }
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
		printf("%s", operators[op] ? operators[op] : "?");
	}
}

static void property(ASTVisitor *v, ASTNode *n, char *key, size_t key_length)
{
	Interpreter *ctx = (Interpreter*)v->ctx;
	switch(n->type)
	{
		case AST_IDENTIFIER:
		{
			int written = snprintf(key, key_length, "%s", n->ast_identifier_data.name);
            if(written < 0 || (size_t)written >= key_length)
				error(v, "property exceeded length");
		}
		break;
		default:
		{
			error(v, "Invalid node %s for property", ast_node_names[n->type]);
		}
		break;
	}
}

static Variable *variable()
{
    Variable *var = malloc(sizeof(Variable));
	var->type = VAR_UNDEFINED;
	var->u.ival = 0;
    return var;
}

static Variable *lvalue(ASTVisitor *v, ASTNode *n)
{
	Interpreter *ctx = (Interpreter*)v->ctx;
    Thread *thread = ctx->thread;
    StackFrame *sf = thread->frame;
	switch(n->type)
	{
		case AST_IDENTIFIER:
		{
			HashTableEntry *entry = hash_table_find(&sf->locals, n->ast_identifier_data.name);
			if(!entry)
			{
				entry = hash_table_insert(&sf->locals, n->ast_identifier_data.name);
				entry->value = variable();
			}
            return entry->value;
		}
		break;
		case AST_MEMBER_EXPR:
		{
			Variable *var = lvalue(v, n->ast_member_expr_data.object);
            if(var->type != VAR_OBJECT)
            {
                error(v, "not an object");
            }
            char prop[256];
			property(v, n->ast_member_expr_data.prop, prop, sizeof(prop));
			HashTableEntry *entry = hash_table_find(&var->u.oval->fields, prop);
            if(!entry)
            {
				entry = hash_table_insert(&var->u.oval->fields, prop);
                entry->value = variable();
            }
            return entry;
		}
		break;
		
		default:
		{
			error(v, "Invalid lvalue for %s", ast_node_names[n->type]);
		}
		break;
	}
    return NULL;
}

IMPL_VISIT(ASTUnaryExpr)
{
	error(v, "TODO");
}
IMPL_VISIT(ASTIdentifier)
{
	// Interpreter *ctx = (Interpreter*)v->ctx;
    // Thread *thread = ctx->thread;
    // StackFrame *sf = thread->frame;
	// HashTableEntry *entry = hash_table_find(&sf->locals, n->name);
	// if(!entry)
	// {
	// 	error(v, "No variable '%s'", n->name);
	// }
	// printf("load %d\n", entry->integer);
}
IMPL_VISIT(ASTAssignmentExpr)
{
	if(n->op == '=')
	{
		visit_node(v, n->rhs);
		lvalue(v, n->lhs);
		printf("store\n");
	}
	else
	{
		visit_node(v, n->rhs);
		visit_node(v, n->lhs);
		printf("binop ");
		print_operator(n->op);
		printf("\n");
		lvalue(v, n->lhs);
		printf("store\n");
		visit_node(v, n->lhs);
	}
}
IMPL_VISIT(ASTBinaryExpr)
{
	visit_node(v, n->rhs);
	visit_node(v, n->lhs);
	printf("binop ");
	print_operator(n->op);
	printf("\n");
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
	Interpreter *ctx = (Interpreter*)v->ctx;
    Thread *thread = ctx->thread;
	
	// visit_node(v, n->body);
}

void call_function(Interpreter *interp, const char *name)
{
	// interp->
}

void ast_visitor_interpreter_init(ASTVisitor *v, Interpreter *interp)
{	
	v->ctx = interp;
// WHEN VISITNG PUSH TO A LIST OF NODES TO PROCESS SO WE CAN PAUSE
	v->visit_ast_function = visit_ASTFunction_;
	v->visit_ast_block_stmt = visit_ASTBlockStmt_;
	v->visit_ast_expr_stmt = visit_ASTExprStmt_;
	v->visit_ast_call_expr = visit_ASTCallExpr_;
	v->visit_ast_literal = visit_ASTLiteral_;
	v->visit_ast_if_stmt = visit_ASTIfStmt_;
	v->visit_ast_binary_expr = visit_ASTBinaryExpr_;
	v->visit_ast_assignment_expr = visit_ASTAssignmentExpr_;
	v->visit_ast_identifier = visit_ASTIdentifier_;

	v->visit_fallback = visit_fallback;
}