#pragma once
#include <core/io/stream_buffer.h>
#include "lexer.h"

typedef struct
{
    Lexer *lexer;
	Token token;
	char *string;
	size_t max_string_length;
} Parser;

typedef struct ASTNode ASTNode;

#define AST_INIT_NODE(UPPER, TYPE, LOWER)     \
	static TYPE *init_##TYPE()                \
	{                                         \
		ASTNode *n = malloc(sizeof(ASTNode)); \
		memset(n, 0, sizeof(ASTNode));        \
		n->type = UPPER;                      \
		return (TYPE *)n;                     \
	}

AST_X_MACRO(AST_INIT_NODE)

#define NODE(TYPE, VAR) AST##TYPE *VAR = init_AST##TYPE()

static void dump_token(Lexer *lexer, Token *t)
{
	char type[64];
	char str[256];
	lexer_token_read_string(lexer, t, str, sizeof(str));
	printf("Token: %s '%s'\n", token_type_to_string(t->type, type, sizeof(type)), str);
}
#define advance(parser, type) advance_(parser, type, __FILE__, __LINE__)
static void advance_(Parser *parser, TokenType type, const char *file, int line)
{
	if(parser->token.type == type)
	{
		lexer_step(parser->lexer, &parser->token);
		return;
	}
	// abort();
	char temp[128];
	lexer_token_read_string(parser->lexer, &parser->token, temp, sizeof(temp));
	char type_expected[64];
	char type_current[64];
	lexer_error(parser->lexer,
				"Expected token type '%s' got '%s' (%s:%d) '%s'",
				token_type_to_string(type, type_expected, sizeof(type_expected)),
				token_type_to_string(parser->token.type, type_current, sizeof(type_current)),
				file,
				line,
				temp);
}

ASTNode *parse_expression(Parser *parser, int precedence);