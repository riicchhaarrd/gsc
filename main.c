#define AST_VISITOR_IMPLEMENTATION
#include "ast.h"
#include "parse.h"
#include <assert.h>
#include "visitor.h"
#include <core/ds/hash_table.h>

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

static ASTNode** statements(Parser *parser, ASTNode **root);
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
		case '{':
		{
			advance(parser, '{');
			n = block(parser);
		}
		break;

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

			ASTNode **root = &stmt->cases;

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
				NODE(SwitchCase, c);

				*root = c;
				root = &((ASTNode*)c)->next;

				if(parser->token.type == TK_DEFAULT)
				{
					advance(parser, TK_DEFAULT);
				}
				else
				{
					advance(parser, TK_CASE);
					// advance(parser, TK_IDENTIFIER);
					c->test = expression(parser); // For a dynamic scripting language accept expressions instead of labels.
				}
				advance(parser, ':');
				ASTNode **consequent = &c->consequent;
				while(1)
				{
					if(parser->token.type == TK_DEFAULT || parser->token.type == TK_CASE || parser->token.type == '}')
						break;
					consequent = statements(parser, consequent);
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

static ASTNode** statements(Parser *parser, ASTNode **root)
{
	ASTNode *stmt = statement(parser);
	*root = stmt;
	return &stmt->next;
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
	// if(parser->token.type == '{')
	// {
	// 	advance(parser, '{');
	// 	return block(parser);
	// }
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

int main(int argc, char **argv)
{

	void ast_visitor_compiler_init(ASTVisitor *v);
	void ast_visitor_gsc_init(ASTVisitor *v);
	ast_visitor_compiler_init(&visitor);

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
