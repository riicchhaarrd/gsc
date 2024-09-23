// http://crockford.com/javascript/tdop/tdop.html
// https://langdev.stackexchange.com/questions/3254/what-exactly-is-pratt-parsing-used-for-and-how-does-it-work

/*

In Top-Down Operator Precedence Parsing, Vaughan Pratt devised a method of simplifying this pattern, in a way that
integrates well with recursive descent parsing.

The technique is to add a precedence table, which associates each operator token with a few properties:
	“Left binding power” (LBP), an integer precedence level
	“Null denotation” (NUD), a function to parse it in prefix position (nothing to its left)
	“Left denotation” (LED), a function to parse it in infix position (something to its left)

The chain of calls is replaced by a single loop that uses the token to dispatch to the appropriate parser immediately,
skipping over the irrelevant precedence levels.
*/

// #define AST_VISITOR_IMPLEMENTATION
#include "ast.h"
#include <core/hash.h>
#include "parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

static void DEBUG(const char *fmt, ...)
{
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[DEBUG] %s\n", message);
	// abort();
}

static ASTNode *boolean(Parser *parser, bool b)
{
	NODE(Literal, lit);
	lit->type = AST_LITERAL_TYPE_BOOLEAN;
	lit->value.boolean = b;
	return (ASTNode *)lit;
}

static ASTNode *undefined(Parser *parser)
{
	NODE(Literal, lit);
	lit->type = AST_LITERAL_TYPE_UNDEFINED;
	return (ASTNode *)lit;
}

static void syntax_error(Parser *parser, const char *fmt, ...)
{
	printf("error %s\n", fmt);
	exit(-1);
}

ASTNode *identifier(Parser *parser, const char *s)
{
	NODE(Identifier, n);
	snprintf(n->name, sizeof(n->name), "%s", s);
	if(lexer_accept(parser->lexer, TK_SCOPE_RESOLUTION, NULL))
	{
		lexer_expect_read_string(parser->lexer, TK_IDENTIFIER, n->file_reference, sizeof(n->file_reference));
	}
	return (ASTNode *)n;
}

typedef enum
{
	LEFT_ASSOC,
	RIGHT_ASSOC
} Associativity;

typedef struct
{
	int precedence;
	Associativity associativity;
	
	// Function for null denotation (literals, variables, and prefix)
	ASTNode* (*nud)(Parser *parser, Token *token);

	// Function for left denotation (infix and suffix)
	ASTNode* (*led)(Parser *parser, ASTNode *left, Token *token, int bp);
} Operator;

ASTNode *parse_expression(Parser *parser, int precedence);

ASTNode *nud_literal(Parser *parser, Token *t)
{
	switch(t->type)
	{
		case TK_IDENTIFIER:
		{
			char id[256];
			lexer_token_read_string(parser->lexer, t, id, sizeof(id));
			return identifier(parser, id);
		}
		break;

		case TK_TRUE: return boolean(parser, true); break;
		case TK_FALSE: return boolean(parser, false); break;
		case TK_UNDEFINED: return undefined(parser); break;

		case TK_NUMBER:
		{
			NODE(Literal, n);
			n->type = AST_LITERAL_TYPE_FLOAT;
			n->value.number = lexer_token_read_float(parser->lexer, t);
			return (ASTNode*)n;
		}
		break;

		case TK_STRING:
		{
			NODE(Literal, n);
			n->type = AST_LITERAL_TYPE_STRING;
			lexer_token_read_string(parser->lexer, t, parser->string, parser->max_string_length);
			n->value.string = strdup(parser->string);
			return (ASTNode*)n;
		}
		break;

		default:
			lexer_error(parser->lexer, "Unexpected token '%d'", t->type);
		break;
	}
	return NULL;
}

ASTNode *led_binary(Parser *parser, ASTNode *left, Token *token, int bp)
{
    NODE(BinaryExpr, n);
    n->lhs = left;
    n->rhs = parse_expression(parser, bp);
    n->op = token->type;
	return (ASTNode *)n;
}

ASTNode *led_member(Parser *parser, ASTNode *left, Token *token, int bp)
{
    NODE(MemberExpr, n);
	n->object = left;
	n->op = token->type;
	if(parser->token.type != TK_IDENTIFIER)
	{
		char type[64];
		lexer_error(parser->lexer,
					"Expected identifier for . got %s",
					token_type_to_string(parser->token.type, type, sizeof(type)));
	}
	n->prop = parse_expression(parser, bp);
	return (ASTNode *)n;
}

ASTNode *led_bracket(Parser *parser, ASTNode *left, Token *token, int bp)
{
    NODE(MemberExpr, n);
	n->object = left;
	n->op = token->type;
	n->prop = parse_expression(parser, 0);
	if(parser->token.type != ']')
	{
		lexer_error(parser->lexer, "Expected ]");
	}
	// lexer_expect(parser->lexer, ']', NULL);
	return (ASTNode *)n;
}

ASTNode *led_function(Parser *parser, ASTNode *left, Token *token, int bp)
{
	NODE(CallExpr, n);
	// Stream *s = parser->lexer->stream;
	// int64_t current = s->tell(s);
	size_t cap = 16;
	n->callee = left;
	n->arguments = malloc(sizeof(ASTNode*) * cap);
	size_t nargs = 0;
	if(parser->token.type != ')')
	{
		while(1)
		{
			ASTNode *arg = parse_expression(parser, 0);
			if(nargs >= cap)
			{
				lexer_error(parser->lexer, "Max args for function call");
			}
			printf("arg: %s\n", ast_node_names[arg->type]);
			n->arguments[nargs++] = arg;
			if(parser->token.type != ',')
				break;
			advance(parser, ',');
		}
		advance(parser, ')');
	}
	n->object = NULL;
	n->pointer = false;
	n->threaded = false;
	n->numarguments = nargs;
	return (ASTNode *)n;
}

ASTNode *nud_array(Parser *parser, Token *token)
{
	lexer_error(parser->lexer, "TODO %s", __FUNCTION__);
	return NULL;
}

ASTNode *nud_group(Parser *parser, Token *token)
{
    NODE(GroupExpr, n);
	n->expression = parse_expression(parser, 0);
	if(parser->token.type != ')')
	{
		lexer_error(parser->lexer, "Expected ) after left paren");
	}
	advance(parser, ')');
	return (ASTNode*)n;
}

ASTNode *led_assignment(Parser *parser, ASTNode *left, Token *token, int bp)
{
    NODE(AssignmentExpr, n);
	n->lhs = left;
	n->rhs = parse_expression(parser, bp);
	n->op = token->type;
	return (ASTNode*)n;
}

ASTNode *led_ternary(Parser *parser, ASTNode *left, Token *token, int bp)
{
    NODE(IfStmt, n);
	n->test = left;
	n->consequent = parse_expression(parser, 0);
	if(parser->token.type != ':')
	{
		lexer_error(parser->lexer, "Expected :");
	}
	lexer_step(parser->lexer, &parser->token);
	n->alternative = parse_expression(parser, 0);
	return (ASTNode *)n;
}

ASTNode *nud_unary(Parser *parser, Token *token);

static const Operator operator_table[TK_MAX] = {
	[TK_PLUS] = { 50, LEFT_ASSOC, NULL, led_binary },
	[TK_MINUS] = { 50, LEFT_ASSOC, NULL, led_binary },
	[TK_MULTIPLY] = { 60, LEFT_ASSOC, NULL, led_binary },
	[TK_DIVIDE] = { 60, LEFT_ASSOC, NULL, led_binary },
	[TK_MOD] = { 60, LEFT_ASSOC, NULL, led_binary },
	[TK_LSHIFT] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_RSHIFT] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_LESS] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_LEQUAL] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_GREATER] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_GEQUAL] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_EQUAL] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_NEQUAL] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_LOGICAL_AND] = { 30, LEFT_ASSOC, NULL, led_binary },
	[TK_LOGICAL_OR] = { 30, LEFT_ASSOC, NULL, led_binary },
	[TK_BITWISE_AND] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_BITWISE_OR] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_BITWISE_XOR] = { 40, LEFT_ASSOC, NULL, led_binary },
	[TK_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_DIV_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_MUL_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_MOD_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_LSHIFT_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_RSHIFT_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_XOR_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_OR_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_AND_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_PLUS_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_MINUS_ASSIGN] = { 10, RIGHT_ASSOC, NULL, led_assignment },
	[TK_TERNARY] = { 20, RIGHT_ASSOC, NULL, led_ternary },
	// [TK_COLON] = { 20, RIGHT_ASSOC, NULL, NULL }, // Ternary operator
	// [TK_COMMA] = { 1, LEFT_ASSOC, NULL, led_binary },
	['('] = { 80, LEFT_ASSOC, nud_group, led_function },
	// [TK_INCREMENT] = { 1, LEFT_ASSOC, NULL, NULL },		// Postfix
	// [TK_DECREMENT] = { 1, LEFT_ASSOC, NULL, NULL },		// Postfix
	['['] = { 80, LEFT_ASSOC, nud_array, led_bracket },
	['.'] = { 80, LEFT_ASSOC, NULL, led_member },
	[TK_NOT] = { 70, RIGHT_ASSOC, nud_unary, NULL },		// Prefix
	[TK_TILDE] = { 70, RIGHT_ASSOC, nud_unary, NULL },	// Prefix
	[TK_NUMBER] = { 0, LEFT_ASSOC, nud_literal, NULL }, // Literal
	[TK_STRING] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_TRUE] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_FALSE] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_IDENTIFIER] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_UNDEFINED] = { 0, LEFT_ASSOC, nud_literal, NULL },
};

ASTNode *nud_unary(Parser *parser, Token *token)
{
    NODE(UnaryExpr, n);
    n->argument = parse_expression(parser, operator_table[token->type].precedence);
    n->op = token->type;
	n->prefix = true;
	return (ASTNode *)n;
}

ASTNode *parse_nud(Parser *parser, Token *token)
{
	const Operator *op = &operator_table[token->type];
	char type[64];
	DEBUG("parse_nud(parser, token = '%s')", token_type_to_string(token->type, type, sizeof(type)));
	if(op->nud)
	{
		return op->nud(parser, token);
	}
	lexer_error(parser->lexer,
				"Error: Unexpected token in nud for '%s'\n",
				token_type_to_string(token->type, type, sizeof(type)));
	return NULL;
}

ASTNode *parse_led(Parser *parser, ASTNode *left, Token *token, int bp)
{
	const Operator *op = &operator_table[token->type];
	char type[64];
	DEBUG("parse_led(parser, left = %s, token = '%s', bp = %d)",
		  ast_node_names[left->type],
		  token_type_to_string(token->type, type, sizeof(type)),
		  bp);
	if(op->led)
	{
		return op->led(parser, left, token, bp);
	}
	lexer_error(parser->lexer,
				"Error: Unexpected token in led for '%s'\n",
				token_type_to_string(token->type, type, sizeof(type)));
	return NULL;
}

ASTNode *parse_expression(Parser *parser, int precedence)
{
	Token t = parser->token;
	lexer_step(parser->lexer, &parser->token);
	ASTNode *left = parse_nud(parser, &t);

	while(precedence < operator_table[parser->token.type].precedence)
	{
		t = parser->token;
		lexer_step(parser->lexer, &parser->token);
		const Operator *op = &operator_table[t.type];
		int next_precedence = (op->associativity == LEFT_ASSOC) ? op->precedence : op->precedence - 1;

		left = parse_led(parser, left, &t, next_precedence);
	}

	return left;
}