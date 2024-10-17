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

ASTNode *expression(Parser *parser);

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

ASTNode *parse_function_name(Parser *parser, Token *t)
{
	if(!t)
	{
		t = &parser->token;
	}
	if(t->type == '[')
	{
		if(t == &parser->token)
		{
			advance(parser, '[');
			advance(parser, '[');
		}
		NODE(FunctionPointerExpr, n);
		n->expression = expression(parser);
		if(t == &parser->token)
		{
			advance(parser, ']');
			advance(parser, ']');
		}
		return n;
	} else if(t->type == TK_FILE_REFERENCE)
	{
		NODE(FileReference, file_ref);
		lexer_token_read_string(parser->lexer, t, file_ref->file, sizeof(file_ref->file));
		if(t == &parser->token)
			advance(parser, TK_FILE_REFERENCE);

		advance(parser, TK_SCOPE_RESOLUTION);

		NODE(Literal, n);
		n->type = AST_LITERAL_TYPE_FUNCTION;
		n->value.function.file = file_ref;
		NODE(Identifier, function_ident);
		lexer_token_read_string(parser->lexer, &parser->token, function_ident->name, sizeof(function_ident->name));
		advance(parser, TK_IDENTIFIER);
		n->value.function.function = function_ident;
		return n;
	}
	NODE(Identifier, n);
	lexer_token_read_string(parser->lexer, t, n->name, sizeof(n->name));
	if(t == &parser->token)
		advance(parser, TK_IDENTIFIER);
	return n;
}

ASTNode *nud_file_ref(Parser *parser, Token *t)
{
	return parse_function_name(parser, t);
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

ASTNode *call_expression(Parser *parser, ASTNode *callee)
{
	NODE(CallExpr, n);
	// Stream *s = parser->lexer->stream;
	// int64_t current = s->tell(s);
	size_t cap = 32;
	n->callee = callee;
	n->arguments = malloc(sizeof(ASTNode*) * cap);
	size_t nargs = 0;
	if(parser->token.type != ')')
	{
		while(1)
		{
			if(parser->token.type == ')')
				break;
			ASTNode *arg = expression(parser);
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
	}
	advance(parser, ')');
	n->object = NULL;
	n->pointer = false;
	n->threaded = false;
	n->numarguments = nargs;
	return (ASTNode *)n;
}

ASTNode *expression(Parser *parser)
{
	ASTNodePtr node = parse_expression(parser, 0);
	return node;
}

ASTNode *nud_literal(Parser *parser, Token *t)
{
	switch(t->type)
	{
		case '&':
		{
			NODE(Literal, n);
			n->type = AST_LITERAL_TYPE_LOCALIZED_STRING;
			lexer_token_read_string(parser->lexer, &parser->token, parser->string, parser->max_string_length);
			advance(parser, TK_STRING);
			n->value.string = strdup(parser->string);
			return (ASTNode*)n;
		}
		break;

		case TK_SCOPE_RESOLUTION:
		{
			NODE(Literal, n);
			n->type = AST_LITERAL_TYPE_FUNCTION;
			NODE(Identifier, func_name);
			lexer_token_read_string(parser->lexer, &parser->token, func_name->name, sizeof(func_name->name));
			advance(parser, TK_IDENTIFIER);
			n->value.function.function = func_name;
			return (ASTNode*)n;
		}
		break;

		case TK_MOD:
		{
			NODE(Literal, n);
			n->type = AST_LITERAL_TYPE_STRING;
			// n->type = AST_LITERAL_TYPE_ANIMATION;
			lexer_token_read_string(parser->lexer, &parser->token, parser->string, parser->max_string_length);
			advance(parser, TK_IDENTIFIER);
			n->value.string = strdup(parser->string);
			return (ASTNode*)n;
		}
		break;

		case TK_IDENTIFIER:
		{
			NODE(Identifier, n);
			lexer_token_read_string(parser->lexer, t, n->name, sizeof(n->name));
			return (ASTNode *)n;
		}
		break;

		case TK_TRUE: return boolean(parser, true); break;
		case TK_FALSE: return boolean(parser, false); break;
		case TK_UNDEFINED: return undefined(parser); break;

		case TK_INTEGER:
		{
			NODE(Literal, n);
			n->type = AST_LITERAL_TYPE_INTEGER;
			n->value.integer = lexer_token_read_int(parser->lexer, t);
			return (ASTNode*)n;
		}
		break;

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

ASTNode *nud_thread_call(Parser *parser, Token *token)
{
	ASTNode *fn = parse_function_name(parser, NULL);
	advance(parser, '(');
	ASTNodePtr n = call_expression(parser, fn);
	n->ast_call_expr_data.threaded = true;
	n->ast_call_expr_data.object = NULL;
	return (ASTNode *)n;
}

ASTNode *led_thread_member_call(Parser *parser, ASTNode *left, Token *token, int bp)
{
	ASTNode *fn = parse_function_name(parser, NULL);
	advance(parser, '(');
	ASTNodePtr n = call_expression(parser, fn);
	n->ast_call_expr_data.threaded = true;
	n->ast_call_expr_data.object = left;
	return (ASTNode *)n;
}

ASTNode *led_member_call(Parser *parser, ASTNode *left, Token *token, int bp)
{
	ASTNode *fn = parse_function_name(parser, token);
	advance(parser, '(');
	ASTNodePtr n = call_expression(parser, fn);
	n->ast_call_expr_data.threaded = false;
	n->ast_call_expr_data.object = left;
	return (ASTNode *)n;
}

ASTNode *led_unary(Parser *parser, ASTNode *left, Token *token, int bp)
{
    NODE(UnaryExpr, n);
    n->argument = left;
    n->op = token->type;
	n->prefix = false;
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
	if(parser->token.type == '[')
	{
		advance(parser, '[');
		NODE(FunctionPointerExpr, fp);
		fp->expression = expression(parser);
		advance(parser, ']');
		advance(parser, ']');
		// advance(parser, '(');
		// ASTNodePtr n = call_expression(parser, fp);
		// n->ast_call_expr_data.threaded = false;
		// n->ast_call_expr_data.object = left;
		// return (ASTNode *)n;
		return (ASTNode*)fp;
	}
    NODE(MemberExpr, n);
	n->object = left;
	n->op = token->type;
	n->prop = expression(parser);
	advance(parser, ']');
	return (ASTNode *)n;
}

ASTNode *led_function(Parser *parser, ASTNode *left, Token *token, int bp)
{
	return call_expression(parser, left);
}

ASTNode *nud_array(Parser *parser, Token *token)
{
	if(parser->token.type == '[')
	{
		advance(parser, '[');
		NODE(FunctionPointerExpr, fp);
		fp->expression = expression(parser);
		advance(parser, ']');
		advance(parser, ']');
		return (ASTNode*)fp;
	}
	NODE(ArrayExpr, n);
	n->numelements = 0;
	if(parser->token.type == ']')
	{
		advance(parser, ']');
	}
	return n;
}

ASTNode *nud_group(Parser *parser, Token *token)
{
	ASTNode *n = NULL;
	ASTNode *expr = expression(parser);
	if(parser->token.type == ',') // Vector
	{
		NODE(VectorExpr, vec);
		vec->elements = malloc(sizeof(ASTNode *) * 3);
		vec->elements[0] = expr;
		for(size_t i = 1; i < 3; ++i)
		{
			advance(parser, ',');
			vec->elements[i] = expression(parser);
		}
		vec->numelements = 3;
		n = vec;
	} else
	{
		NODE(GroupExpr, grp);
		grp->expression = expr;
		n = grp;
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
	n->consequent = expression(parser);
	advance(parser, ':');
	n->alternative = expression(parser);
	return (ASTNode *)n;
}

ASTNode *nud_unary(Parser *parser, Token *token);

static const Operator operator_table[TK_MAX] = {
	['#'] = { 0, LEFT_ASSOC, nud_unary, NULL },
	[TK_PLUS] = { 50, LEFT_ASSOC, nud_unary, led_binary },
	[TK_MINUS] = { 50, LEFT_ASSOC, nud_unary, led_binary },
	[TK_MULTIPLY] = { 60, LEFT_ASSOC, NULL, led_binary },
	[TK_DIVIDE] = { 60, LEFT_ASSOC, NULL, led_binary },
	[TK_MOD] = { 60, LEFT_ASSOC, nud_literal, led_binary },
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
	[TK_BITWISE_AND] = { 40, LEFT_ASSOC, nud_literal, led_binary },
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
	// [')'] = { 80, LEFT_ASSOC, NULL, NULL },
	// [TK_INCREMENT] = { 1, LEFT_ASSOC, NULL, NULL },		// Postfix
	// [TK_DECREMENT] = { 1, LEFT_ASSOC, NULL, NULL },		// Postfix
	['['] = { 80, LEFT_ASSOC, nud_array, led_bracket },
	// [']'] = { 80, LEFT_ASSOC, NULL, NULL },
	['.'] = { 80, LEFT_ASSOC, NULL, led_member },
	[TK_INCREMENT] = { 80, LEFT_ASSOC, NULL, led_unary },
	[TK_DECREMENT] = { 80, LEFT_ASSOC, NULL, led_unary },
	[TK_NOT] = { 70, RIGHT_ASSOC, nud_unary, NULL },		// Prefix
	[TK_TILDE] = { 70, RIGHT_ASSOC, nud_unary, NULL },	// Prefix
	[TK_NUMBER] = { 0, LEFT_ASSOC, nud_literal, NULL }, // Literal
	[TK_INTEGER] = { 0, LEFT_ASSOC, nud_literal, NULL }, // Literal
	[TK_STRING] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_TRUE] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_FALSE] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_IDENTIFIER] = { 80, LEFT_ASSOC, nud_literal, led_member_call },
	[TK_FILE_REFERENCE] = { 80, LEFT_ASSOC, nud_file_ref, led_member_call },
	[TK_THREAD] = { 80, LEFT_ASSOC, nud_thread_call, led_thread_member_call },
	[TK_SCOPE_RESOLUTION] = { 0, LEFT_ASSOC, nud_literal, NULL },
	[TK_UNDEFINED] = { 0, LEFT_ASSOC, nud_literal, NULL },
};

ASTNode *nud_unary(Parser *parser, Token *token)
{
	ASTNode *result = NULL;
	if(token->type == '#')
	{
		lexer_token_read_string(parser->lexer, &parser->token, parser->string, parser->max_string_length);
		if(strcmp(parser->string, "animtree"))
			lexer_error(parser->lexer, "Expected animtree directive");
		advance(parser, TK_IDENTIFIER);

		NODE(Literal, n);
		n->type = AST_LITERAL_TYPE_STRING;
		n->value.string = strdup(parser->animtree);
		result = n;
	}
	else
	{
		NODE(UnaryExpr, n);
		n->argument = parse_expression(parser, operator_table[token->type].precedence);
		n->op = token->type;
		n->prefix = true;
		result = n;
	}
	return (ASTNode *)result;
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