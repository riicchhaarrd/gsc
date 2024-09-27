#define AST_VISITOR_IMPLEMENTATION
#include "ast.h"
#include "parse.h"
#include <assert.h>
#include "visitor.h"
#include <core/ds/hash_table.h>

typedef struct
{
	int depth;
} ASTVisitorContext;

static ASTVisitor visitor;

#define DISK

#define AST_VISITORS(UPPER, TYPE, LOWER) [UPPER] = (AstVisitorFn)TYPE ## _visit,

static const AstVisitorFn ast_visitors[] = {
    AST_X_MACRO(AST_VISITORS) NULL
};

static size_t visit_char(AstVisitor *visitor, const char *key, char *value, size_t nmemb, size_t size)
{
    size_t *depth = visitor->ctx;
	for(size_t i = 0; i < *depth; ++i)
		putchar('\t');
	printf("%s: %.*s\n", key, nmemb, value);
	return 0;
}
static size_t visit_uint32(AstVisitor *visitor, const char *key, uint32 *value, size_t nmemb, size_t size)
{
    size_t *depth = visitor->ctx;
	for(size_t i = 0; i < *depth; ++i)
		putchar('\t');
	printf("%s: %d\n", key, *value);
	return 0;
}
static size_t visit_int(AstVisitor *visitor, const char *key, int *value, size_t nmemb, size_t size)
{
    size_t *depth = visitor->ctx;
	for(size_t i = 0; i < *depth; ++i)
		putchar('\t');
	printf("%s: %d\n", key, *value);
	return 0;
}
static size_t visit_bool(AstVisitor *visitor, const char *key, bool *value, size_t nmemb, size_t size)
{
    size_t *depth = visitor->ctx;
	for(size_t i = 0; i < *depth; ++i)
		putchar('\t');
	printf("%s: %s\n", key, *value ? "true" : "false");
	return 0;
}

static size_t visit_ASTNodePtr(AstVisitor *visitor, const char *key, ASTNodePtr *value, size_t nmemb, size_t size)
{
	if(!value || !*value)
		return 0;
	for(size_t k = 0; k < nmemb; ++k)
	{
		ASTNode *n = value[k];
		size_t *depth = visitor->ctx;
		for(size_t i = 0; i < *depth; ++i)
			putchar('\t');
		*depth += 1;
		printf("%s ", ast_node_names[n->type]);
		if(n->type == AST_LITERAL)
		{
			switch(n->ast_literal_data.type)
			{
				case AST_LITERAL_TYPE_FLOAT:
				{
					printf("(%f)", n->ast_literal_data.value.number);
				}
				break;
				case AST_LITERAL_TYPE_STRING:
				{
					printf("(%s)", n->ast_literal_data.value.string);
				}
				break;
			}
		}
		else if(n->type == AST_ASSIGNMENT_EXPR)
		{
			char type[64];
			printf("(%s)", token_type_to_string(n->ast_assignment_expr_data.op, type, sizeof(type)));
		}
		else if(n->type == AST_BINARY_EXPR)
		{
			char type[64];
			printf("(%s)", token_type_to_string(n->ast_binary_expr_data.op, type, sizeof(type)));
		}
		else if(n->type == AST_IDENTIFIER)
		{
			// printf("(%s::%s)", n->ast_identifier_data.name, n->ast_identifier_data.file_reference);
			printf("(%s)", n->ast_identifier_data.name);
		}
		printf("\n");
		ast_visitors[n->type](visitor, key, n, 1, sizeof(ASTNode));
		*depth -= 1;
	}
}

static char *read_text_file(const char *path)
{
	FILE *fp = fopen(path, "r");
	if(!fp)
		return NULL;
	long n = 0;
	fseek(fp, 0, SEEK_END);
	n = ftell(fp);
	char *data = malloc(n + 1);
	rewind(fp);
	fread(data, 1, n, fp);
	fclose(fp);
	return data;
}

static const char *string(Parser *parser, TokenType type)
{
	lexer_token_read_string(parser->lexer, &parser->token, parser->string, parser->max_string_length);
	advance(parser, type);
	return parser->string;
}

static void syntax_error(Parser *parser, const char *fmt, ...)
{
	printf("error %s\n", fmt);
	exit(-1);
}

ASTNode *expression(Parser *parser);

static ASTNode* block(Parser *parser);
static ASTNode *body(Parser *parser);

static ASTNode *statement(Parser *parser)
{
	printf("statement() : ");
	dump_token(parser->lexer, &parser->token);
	// lexer_step(parser->lexer, &parser->token);
	ASTNode *n = NULL;
	switch(parser->token.type)
	{
		case TK_COMMENT:
		{
			advance(parser, TK_COMMENT);
			NODE(EmptyStmt, stmt);
			n = stmt;
		}
		break;

		case TK_WHILE:
		{
			NODE(WhileStmt, stmt);
			n = stmt;
			advance(parser, TK_WHILE);
			advance(parser, '(');
			stmt->test = expression(parser);
			advance(parser, ')');
			stmt->body = body(parser);
		}
		break;

		case TK_FOR:
		{
			NODE(ForStmt, stmt);
			n = stmt;
			advance(parser, TK_FOR);
			advance(parser, '(');
			if(parser->token.type != ';')
				stmt->init = expression(parser);
			advance(parser, ';');
			if(parser->token.type != ';')
				stmt->test = expression(parser);
			advance(parser, ';');
			if(parser->token.type != ')')
				stmt->update = expression(parser);
			advance(parser, ')');

			stmt->body = body(parser);
		}
		break;

		// case TK_THREAD:
		// {
		// 	NODE(ExprStmt, stmt);
		// 	advance(parser, TK_THREAD);
		// 	stmt->expression = expression(parser);
		// 	if(stmt->expression->type != AST_CALL_EXPR)
		// 	{
		// 		lexer_error(parser->lexer, "Expected call expression for thread");
		// 	}
		// 	n = stmt;
		// 	advance(parser, ';');
		// }
		// break;

		case TK_BREAK:
		{
			NODE(BreakStmt, stmt);
			n = stmt;
			advance(parser, TK_BREAK);
			advance(parser, ';');
		}
		break;
		
		case TK_WAIT:
		{
			NODE(WaitStmt, stmt);
			n = stmt;
			advance(parser, TK_WAIT);
			stmt->duration = expression(parser);
			advance(parser, ';');
		}
		break;

		case TK_CONTINUE:
		{
			NODE(ContinueStmt, stmt);
			n = stmt;
			advance(parser, TK_CONTINUE);
			advance(parser, ';');
		}
		break;

		case TK_SWITCH:
		{
			NODE(SwitchStmt, stmt);
			n = stmt;
			advance(parser, TK_SWITCH);
			advance(parser, '(');
			stmt->discriminant = expression(parser);
			advance(parser, ')');
			advance(parser, '{');
			while(1)
			{
				if(parser->token.type == '}')
					break;
				if(parser->token.type == TK_DEFAULT)
				{
					advance(parser, TK_DEFAULT);
				}
				else
				{
					advance(parser, TK_CASE);
					// advance(parser, TK_IDENTIFIER);
					expression(parser); // For a dynamic scripting language accept expressions instead of labels.
				}
				advance(parser, ':');
				while(1)
				{
					if(parser->token.type == TK_DEFAULT || parser->token.type == TK_CASE || parser->token.type == '}')
						break;
					body(parser);
				}
			}
			advance(parser, '}');
		}
		break;

		case TK_RETURN:
		{
			advance(parser, TK_RETURN);
			NODE(ReturnStmt, stmt);
			n = (ASTNode*)stmt;
			if(parser->token.type != ';')
			{
				stmt->argument = expression(parser); // TODO: can't call a threaded call expr in return, seperate parse_expression and expression functions
			}
			advance(parser, ';');
		}
		break;

		case TK_IF:
		{
			NODE(IfStmt, if_stmt);
			n = (ASTNode*)if_stmt;
			advance(parser, TK_IF);
			advance(parser, '(');
			if_stmt->test = expression(parser);
			advance(parser, ')');
			if_stmt->consequent = body(parser);
			if(parser->token.type == TK_ELSE)
			{
				advance(parser, TK_ELSE);
				if_stmt->alternative = body(parser);
			}
		}
		break;

		default:
		{
			NODE(ExprStmt, stmt);
			n = (ASTNode*)stmt;
			stmt->expression = expression(parser);
			advance(parser, ';');
		} break;
	}
	return n;
}

static ASTNode* block(Parser *parser)
{
	NODE(BlockStmt, n);
	n->numbody = 0;
	ASTNode **root = &n->body;
	while(parser->token.type != '}')
	{
		ASTNode *stmt = statement(parser);
		*root = stmt;
		root = &stmt->next;
		// visit_node(&visitor, stmt);
		assert(stmt);
		// printf("statement(%s) ", ast_node_names[stmt->type]);
		// dump_token(parser->lexer, &parser->token);
		// if(!lexer_step(parser->lexer, &parser->token))
		// {
		// 	lexer_error(parser->lexer, "Unexpected EOF");
		// }
	}
	advance(parser, '}');
	return (ASTNode*)n;
}

static ASTNode *body(Parser *parser)
{
	if(parser->token.type == '{')
	{
		advance(parser, '{');
		return block(parser);
	}
	return statement(parser);
}

typedef struct
{
	HashTable ht;
} ASTProgram;

static void parse(Parser *parser, ASTProgram *prog)
{
	// while(lexer_step(parser->lexer, &parser->token))
	char type[64];
	while(parser->token.type != 0)
	{
		// printf("%s\n", token_type_to_string(parser->token.type, type, sizeof(type)));
		switch(parser->token.type)
		{
			case '#':
			{
				advance(parser, '#');
				const char *ident = string(parser, TK_IDENTIFIER);
				printf("ident:%s\n",ident);
				if(!strcmp(ident, "include"))
				{
					const char *path = string(parser, TK_FILE_REFERENCE);
					printf("path:%s\n", path);
				} else if(!strcmp(ident, "using_animtree"))
				{
					advance(parser, '(');
					const char *tree = string(parser, TK_STRING);
					advance(parser, ')');
					printf("using_animtree tree:%s\n", tree);
				}
				else
				{
					syntax_error(parser, "Invalid directive");
				}
				advance(parser, ';');
			}
			break;
			
#ifdef DISK
			// Global scope, function definitions
			case TK_IDENTIFIER:
			{
				NODE(Function, func);
				lexer_token_read_string(parser->lexer, &parser->token, func->name, sizeof(func->name));
				advance(parser, TK_IDENTIFIER);
				advance(parser, '(');
				if(parser->token.type != ')')
				{
					while(1)
					{
						advance(parser, TK_IDENTIFIER); // TODO: save args
						if(parser->token.type != ',')
							break;
						advance(parser, ',');
					}
				}
				advance(parser, ')');
				// lexer_step(parser->lexer, &parser->token);
				advance(parser, '{');
				func->body = block(parser);
				// visit_node(&visitor, func->body);

				HashTableEntry *entry = hash_table_insert(&prog->ht, func->name);
				if(entry->value)
				{
					lexer_error(parser->lexer, "Function '%s' already defined", func->name);
				}
				entry->value = func;
			}
			break;
#endif

			default:
			{
#ifdef DISK
				dump_token(parser->lexer, &parser->token);
				syntax_error(parser, "Expected identifier or #");
#else
				expression(parser);
#endif
			}
			break;
		}
	}
}

static void tokenize(Lexer *lexer)
{
	Token t;
	char type[64];
	char repr[256];
	while(lexer_step(lexer, &t))
	{
		lexer_token_read_string(lexer, &t, repr, sizeof(repr));
		// printf("%s", repr);
		printf("Token: %s '%s' (%d[%d])\n", token_type_to_string(t.type, type, sizeof(type)), repr, t.offset, t.length);
	}
}

static void parse_line(const char *line)
{
	char string[16384];
	Stream s = { 0 };
	StreamBuffer sb = { 0 };
	init_stream_from_buffer(&s, &sb, (unsigned char*)line, strlen(line) + 1);
	
	Lexer l = { 0 };
	lexer_init(&l, &s);
	l.flags |= LEXER_FLAG_PRINT_SOURCE_ON_ERROR;
	// l.flags |= LEXER_FLAG_TOKENIZE_WHITESPACE;
	jmp_buf jmp;
	if(setjmp(jmp))
	{
		printf("ERROR\n");
		exit(-1);
	}
	l.jmp = &jmp;
	// tokenize(&l);
	// return 0;
	Parser parser = { 0 };
	parser.string = string;
	parser.max_string_length = sizeof(string);
	parser.lexer = &l;

	// tokenize(parser.lexer);
	// getchar();
	lexer_step(parser.lexer, &parser.token);
	ASTProgram prog = { 0 };
	hash_table_init(&prog.ht, 12);
	parse(&parser, &prog);

	HashTableEntry *it = prog.ht.head;
	while(it)
	{
		// TODO: reverse iteration order
		visit_node(&visitor, it->value);
		it = it->next;
	}
}

static void parse_file(const char *path)
{
	unsigned char *data = read_text_file(path);
	parse_line(data);
	// Stream s = { 0 };
	// StreamBuffer sb = { 0 };
	// init_stream_from_buffer(&s, &sb, data, strlen(data) + 1);
	// char line[16384];
	// while(!stream_read_line(&s, line, sizeof(line)))
	// {
	// 	parse_line(line);
	// }
	// while(fgets(line, sizeof(line), stdin))
	// {
    // 	parse_line(line);
	// }
}
#define IMPL_VISIT(TYPE) static void visit_##TYPE##_(ASTVisitor *v, ASTNode *ast_node, TYPE *n)

static void indent_printf(ASTVisitor *v, const char *fmt, ...)
{
	ASTVisitorContext *ctx = (ASTVisitorContext*)v->ctx;
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
	ASTVisitorContext *ctx = (ASTVisitorContext*)v->ctx;
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
	ASTVisitorContext *ctx = (ASTVisitorContext*)v->ctx;
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

int main(int argc, char **argv)
{
	ASTVisitorContext ctx = { 0 };
	visitor.ctx = &ctx;
	visitor.visit_fallback = visit_fallback;
	visitor.visit_ast_block_stmt = visit_ASTBlockStmt_;
	visitor.visit_ast_call_expr = visit_ASTCallExpr_;
	visitor.visit_ast_identifier = visit_ASTIdentifier_;
	visitor.visit_ast_literal = visit_ASTLiteral_;
	// visitor.visit_ast_localized_string = visit_ASTLocalizedString_;
	visitor.visit_ast_member_expr = visit_ASTMemberExpr_;
	visitor.visit_ast_unary_expr = visit_ASTUnaryExpr_;
	visitor.visit_ast_expr_stmt = visit_ASTExprStmt_;
	visitor.visit_ast_binary_expr = visit_ASTBinaryExpr_;
	visitor.visit_ast_group_expr = visit_ASTGroupExpr_;
	visitor.visit_ast_assignment_expr = visit_ASTAssignmentExpr_;
	visitor.visit_ast_function = visit_ASTFunction_;
	visitor.visit_ast_if_stmt = visit_ASTIfStmt_;
	visitor.visit_ast_vector_expr = visit_ASTVectorExpr_;
	visitor.visit_ast_return_stmt = visit_ASTReturnStmt_;
	visitor.visit_ast_file_reference = visit_ASTFileReference_;
	visitor.visit_ast_for_stmt = visit_ASTForStmt_;
	visitor.visit_ast_while_stmt = visit_ASTWhileStmt_;
	// visitor.visit_ast_function_ptr = visit_ASTFunctionPtr_;

	assert(argc > 1);
	#ifdef DISK
	static const char *base_path = "scripts/";
	char path[512];
	snprintf(path, sizeof(path), "%s%s.gsc", base_path, argv[1]);
	parse_file(path);
	#else
	char line[16384];
	while(fgets(line, sizeof(line), stdin))
	{
    	parse_line(line);
	}
	#endif
	return 0;
}
