#define AST_VISITOR_IMPLEMENTATION
#include "ast.h"
#include "parse.h"
#include <assert.h>
#include "visitor.h"

static ASTVisitor visitor;

// #define DISK

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
			printf("(%s::%s)", n->ast_identifier_data.name, n->ast_identifier_data.file_reference);
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

static ASTNode *expression(Parser *parser)
{
	// parser->token.type = 0;
	// lexer_step(parser->lexer, &parser->token);
	ASTNodePtr node = parse_expression(parser, 0);

	// size_t depth = 0;
	// AstVisitor visitor;
	// type_ast_visitor_init(&visitor, &depth);
	// visitor.visit_ASTNodePtr = (AstVisitorFn)visit_ASTNodePtr;
	// visitor.visit_char = (AstVisitorFn)visit_char;
	// visitor.visit_uint32 = (AstVisitorFn)visit_uint32;
	// visitor.visit_int = (AstVisitorFn)visit_int;
	// visitor.visit_bool = (AstVisitorFn)visit_bool;
	// visit_ASTNodePtr(&visitor, NULL, &node, 1, 0);
	visit_node(&visitor, node);
	return node;
}

static ASTNode* block(Parser *parser);
static ASTNode *statement(Parser *parser)
{
	printf("statement() : ");
	dump_token(parser->lexer, &parser->token);
	// lexer_step(parser->lexer, &parser->token);
	ASTNode *n = NULL;
	switch(parser->token.type)
	{
		case TK_IF:
		{
			NODE(IfStmt, if_stmt);
			n = (ASTNode*)if_stmt;
			advance(parser, TK_IF);
			advance(parser, '(');
			if_stmt->test = expression(parser);
			advance(parser, ')');
			if(parser->token.type == '{')
			{
				advance(parser, '{');
				if_stmt->consequent = block(parser);
			} else
			{
				if_stmt->consequent = statement(parser);
			}
			if(parser->token.type == TK_ELSE)
			{
				advance(parser, TK_ELSE);
				if(parser->token.type == '{')
				{
					advance(parser, '{');
					if_stmt->consequent = block(parser);
				}
				else
				{
					if_stmt->consequent = statement(parser);
				}
			}
		}
		break;

		default:
			n = expression(parser);
			advance(parser, ';');
			break;
	}
	return n;
}

static ASTNode* block(Parser *parser)
{
	NODE(BlockStmt, n);
	while(parser->token.type != '}')
	{
		ASTNode *stmt = statement(parser);
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

static void parse(Parser *parser)
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
					const char *path = string(parser, TK_IDENTIFIER);
					printf("path:%s\n", path);
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
				char name[128];
				lexer_token_read_string(parser->lexer, &parser->token, name, sizeof(name));
				advance(parser, TK_IDENTIFIER);
				printf("func: %s\n", name);
				advance(parser, '(');
				advance(parser, ')');
				// lexer_step(parser->lexer, &parser->token);
				advance(parser, '{');
				ASTNode *body = block(parser);
				visit_node(&visitor, body);
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
	parse(&parser);
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

static void print_operator(int op)
{
	// printf("op:%d\n",op);
	if(op < 255)
		printf("%c", op & 0xff);
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
IMPL_VISIT(ASTMemberExpr)
{
	visit_node(v, n->object);
	print_operator(n->op);
	visit_node(v, n->prop);
}
IMPL_VISIT(ASTLocalizedString)
{
	printf("%s", n->reference);
}
IMPL_VISIT(ASTUnaryExpr)
{
	printf("%c", n->op < 255 ? n->op : '?');
	visit_node(v, n->argument);
}
IMPL_VISIT(ASTExprStmt)
{
	visit_node(v, n->expression);
	printf(";");
}
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
			printf("%s", n->value.string);
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
	printf("{\n");
	for(size_t i = 0; i < n->numbody; ++i)
	{
		visit_node(v, n->body[i]);
	}
	printf("}\n");
}
IMPL_VISIT(ASTIdentifier)
{
	printf("%s", n->name);
	if(n->file_reference[0] != 0)
		printf("::%s", n->file_reference);
}
IMPL_VISIT(ASTCallExpr)
{
	visit_node(v, n->callee);
	printf("(");
	for(size_t i = 0; i < n->numarguments; ++i)
	{
		visit_node(v, n->arguments[i]);
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
	visitor.visit_fallback = visit_fallback;
	visitor.visit_ast_block_stmt = visit_ASTBlockStmt_;
	visitor.visit_ast_call_expr = visit_ASTCallExpr_;
	visitor.visit_ast_identifier = visit_ASTIdentifier_;
	visitor.visit_ast_literal = visit_ASTLiteral_;
	visitor.visit_ast_localized_string = visit_ASTLocalizedString_;
	visitor.visit_ast_member_expr = visit_ASTMemberExpr_;
	visitor.visit_ast_unary_expr = visit_ASTUnaryExpr_;
	visitor.visit_ast_expr_stmt = visit_ASTExprStmt_;
	visitor.visit_ast_binary_expr = visit_ASTBinaryExpr_;

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
