#pragma once

#include "stream.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>

#ifndef CT_HASH
	#define CT_HASH(string, hash) (hash)
#endif

#define IDENTS(X)                                \
	X(FOR, CT_HASH(for, 0xacf38390))             \
	X(WHILE, CT_HASH(while, 0xdc628ce))          \
	X(IF, CT_HASH(if, 0x39386e06))               \
	X(ELSE, CT_HASH(else, 0xbdbf5bf0))           \
	X(DO, CT_HASH(do, 0x621cd814))               \
	X(CONTINUE, CT_HASH(continue, 0xb1727e44))   \
	X(BREAK, CT_HASH(break, 0xc9648178))         \
	X(RETURN, CT_HASH(return, 0x85ee37bf))       \
	X(SWITCH, CT_HASH(switch, 0x93e05f71))       \
	X(DEFAULT, CT_HASH(default, 0x933b5bde))     \
	X(CASE, CT_HASH(case, 0x9b2538b1))           \
	X(TRUE, CT_HASH(true, 0x4db211e5))           \
	X(FALSE, CT_HASH(false, 0xb069958))          \
	X(UNDEFINED, CT_HASH(undefined, 0x9b61ad43)) \
	X(NULL, CT_HASH(null, 0x77074ba4))           \
	X(IN, CT_HASH(in, 0x41387a9e))               \
	X(FOREACH, CT_HASH(foreach, 0xb75f8759))     \
	X(THREAD, CT_HASH(thread, 0xa2026671))       \
	X(WAIT, CT_HASH(wait, 0x892e4ca0))           \
	X(SELF, CT_HASH(self, 0x645ba277))           \
	X(WAITTILLFRAMEEND, CT_HASH(waittillframeend, 0x17257589))

#define LEXER_STATIC static

#define TOKENS_(X) \
	X(IDENTIFIER) \
	X(FILE_REFERENCE) \
	X(STRING) X(NUMBER) X(INTEGER) X(COMMENT) X(WHITESPACE) \
	X(LSHIFT, "<<") \
	X(RSHIFT, ">>") \
	X(LOGICAL_AND, "&&") \
	X(LOGICAL_OR, "||") \
	X(RSHIFT_ASSIGN, ">>=") \
	X(LSHIFT_ASSIGN, "<<=") \
	X(GEQUAL, '>') \
	X(LEQUAL, '<') \
	X(AND_ASSIGN, '&') \
	X(OR_ASSIGN, '|') \
	X(INCREMENT, "++") \
	X(DECREMENT, "--") \
	X(SCOPE_RESOLUTION, "::") \
	X(DIV_ASSIGN, '/') \
	X(ELLIPSIS, "...") \
	X(PLUS_ASSIGN, "+=") \
	X(MINUS_ASSIGN, "-=")

#define EQ_OPERATORS(X) \
	X(XOR_ASSIGN, '^') \
	X(MOD_ASSIGN, '%') \
	X(MUL_ASSIGN, '*') \
	X(EQUAL, '=') \
	X(NEQUAL, '!')

#define TOKENS(X) TOKENS_(X) EQ_OPERATORS(X) IDENTS(X)

#define TOKEN_ENUM(NAME, ...)	   TK_##NAME,
typedef enum
{
	TK_EOF = 0,

	TK_PLUS = '+',        // '+'
    TK_MINUS = '-',       // '-'
    TK_MULTIPLY = '*',    // '*'
    TK_DIVIDE = '/',      // '/'
    TK_MOD = '%',         // '%'
    TK_LESS = '<',          // '<'
    TK_GREATER = '>',          // '>'
    TK_ASSIGN = '=',      // '='
    TK_BITWISE_AND = '&', // '&'
    TK_BITWISE_OR = '|',  // '|'
    TK_BITWISE_XOR = '^', // '^'
    TK_TERNARY = '?',     // '?'
    TK_COLON = ':',       // ':'
    TK_COMMA = ',',       // ','
    TK_NOT = '!',         // '!'
    TK_TILDE = '~',       // '~'

	// ASCII table ...
	TK_NBSP = 255, // Non-Breaking Space, NBSP
	TOKENS(TOKEN_ENUM)
	TK_MAX
} TokenType;

typedef struct
{
	TokenType type;
	size_t offset, length, next_offset;
} Token;

#define EQ_OPERATORS_MAP(NAME, CH) [CH] = TK_##NAME,
static const TokenType eq_op_char_map[] = { EQ_OPERATORS(EQ_OPERATORS_MAP) [TK_NBSP] = TK_NBSP };

LEXER_STATIC const char *token_type_to_string(TokenType token_type, char *string_out, int string_out_size)
{
	if(token_type >= TK_MAX)
		return "?";
	if(string_out_size < 2)
		return "?";
	if(token_type <= 0xff) // Printable ASCII range
	// if(token_type >= 0x20 && token_type <= /*0x7e*/0xff) // Printable ASCII range
	{
		string_out[0] = token_type & 0xff;
		string_out[1] = 0;
		return string_out;
	}
	if(token_type < 256)
		return "?";
#define TOKEN_STRINGS(NAME, ...)	#NAME,
	static const char *type_strings[] = { TOKENS(TOKEN_STRINGS) NULL };
	return type_strings[token_type - 256];
}

enum
{
	LEXER_FLAG_NONE = 0,
	LEXER_FLAG_PRINT_SOURCE_ON_ERROR = 1,
	LEXER_FLAG_TOKENIZE_WHITESPACE = 2,
	LEXER_FLAG_TOKENIZE_NEWLINES = 4,
	LEXER_FLAG_TOKENIZE_COMMENTS = 8
};

typedef struct
{
	Stream *stream;
	jmp_buf *jmp;
	uint32_t flags;
	FILE *out;
	void *userptr;
	int line;
} Lexer;

LEXER_STATIC void lexer_init(Lexer *l, Stream *stream)
{
	l->stream = stream;
	l->jmp = NULL;
	l->flags = LEXER_FLAG_NONE;
	l->out = stdout;
	l->userptr = NULL;
	l->line = 0;
}

LEXER_STATIC size_t lexer_token_read_string(Lexer *lexer, Token *t, char *temp, size_t max_temp_size)
{
	if(t->length == 0)
	{
		temp[0] = 0;
		return 0;
	}
	Stream *ls = lexer->stream;
	int64_t offset = ls->tell(ls);
	ls->seek(ls, t->offset, SEEK_SET);
	size_t n = max_temp_size - 1;
	if(t->length < n)
		n = t->length;
	ls->read(ls, temp, 1, n);
	temp[n] = 0;
	ls->seek(ls, offset, SEEK_SET);
	return n;
}

LEXER_STATIC int lexer_advance(Lexer *l)
{
	unsigned char buf = 0;
	if(l->stream->read(l->stream, &buf, 1, 1) != 1)
		return 0;
	return buf;
}

LEXER_STATIC bool lexer_match_char(Lexer *l, int needle)
{
	Stream *s = l->stream;
	int64_t offset = s->tell(s);

	unsigned char next = 0;

	if(s->read(s, &next, 1, 1) != 1 || needle != next)
	{
		s->seek(s, offset, SEEK_SET);
		return false;
	}
	return true;
}

LEXER_STATIC void lexer_print_source(Lexer *lexer, Token *t, int range_min, int range_max)
{
	Stream *ls = lexer->stream;
	int64_t pos = ls->tell(ls);
	ls->seek(ls, t->offset + range_min, SEEK_SET);
	size_t n = range_max - range_min;
	for(int i = 0; i < n; ++i)
	{
		char ch;
		if(0 == ls->read(ls, &ch, 1, 1) || !ch)
			break;
		if(ls->tell(ls) == t->offset)
			putc('*', stdout);
		putc(ch, stdout);
	}
	ls->seek(ls, pos, SEEK_SET);
}

LEXER_STATIC void lexer_error(Lexer *l, const char *fmt, ...)
{
	char text[2048] = { 0 };
	if(fmt)
	{
		va_list va;
		va_start(va, fmt);
		vsnprintf(text, sizeof(text), fmt, va);
		va_end(va);
	}
	Token ft = { 0 };
	ft.offset = l->stream->tell(l->stream);
	if(l->flags & LEXER_FLAG_PRINT_SOURCE_ON_ERROR)
	{
		fprintf(l->out, "===============================================================\n");
		lexer_print_source(l, &ft, -100, 100);
		fprintf(l->out, "\n===============================================================\n");
	}
	fprintf(l->out, "Lexer error: %s\n", text);
	if(l->jmp)
	{
		longjmp(*l->jmp, 1);
	}
}

LEXER_STATIC void lexer_unget(Lexer *l, Token *t)
{
	Stream *s = l->stream;
	int64_t current = s->tell(s);
	if(current == 0)
		return;
	if(!t)
	{
		s->seek(s, current - 1, SEEK_SET);
	}
	else
	{
		l->stream->seek(l->stream, t->offset, SEEK_SET);
	}
}

LEXER_STATIC void lexer_parse_string(Lexer *lexer, Token *t)
{
	t->type = TK_STRING;
	size_t n = 0;
	bool escaped = false;
	while(1)
	{
		int ch = lexer_advance(lexer);
		if(!ch)
		{
			break;
		}
		if(ch == '"' && !escaped)
		{
			break;
		}
		escaped = (!escaped && ch == '\\');
		++n;
	}
	t->length = n;
}

LEXER_STATIC void lexer_parse_multiline_comment(Lexer *lexer, TokenType type, Token *t, int initial_char)
{
	t->type = type;
	t->offset = lexer->stream->tell(lexer->stream);
	size_t n = 0;
	while(1)
	{
		int ch = lexer_advance(lexer);
		if(!ch)
			break;
		if(ch == initial_char)
		{
			int second = lexer_advance(lexer);
			if(second == '/')
			{
				break;
			}
			lexer_unget(lexer, NULL);
		}
		++n;
	}
	t->length = n;
}

LEXER_STATIC uint32_t lexer_parse_characters(Lexer *lexer, Token *t, TokenType type, bool (*cond)(Token *t, int ch, bool *undo))
{
	uint32_t prime = 0x01000193;
	uint32_t offset = 0x811c9dc5;

	uint32_t hash = offset;

	t->type = type;
	t->offset = lexer->stream->tell(lexer->stream);
	size_t n = 0;
	while(1)
	{
		int ch = lexer_advance(lexer);
		if(!ch)
		{
			break;
		}
		bool undo = false;
		if(cond(t, ch, &undo))
		{
			if(undo)
			{
				lexer_unget(lexer, NULL);
			}
			else
			{
				++n;
			}
			break;
		}
		hash ^= ch;
		hash *= prime;
		++n;
	}
	t->length = n;
	return hash;
}

LEXER_STATIC bool cond_string_(Token *t, int ch, bool *undo)
{
	*undo = false;
	return ch == '"';
}

LEXER_STATIC bool cond_numeric_(Token *t, int ch, bool *undo)
{
	*undo = true;

	// if(ch == '-') // Removed because it would parse 1- as a number instead of 2 tokens '1' and '-'
	// 	return false;

	if(ch >= '0' && ch <= '9') // Decimal
		return false;

	if(ch == 'e') // Scientific notation
		return false;

	if(ch == 'x') // Hexadecimal separator
		return false;

	if(ch >= 'a' && ch <= 'f') // Hexadecimal
		return false;

	if(ch >= 'A' && ch <= 'F') // Hexadecimal
		return false;

	if(ch == '.' || ch == 'f') // Floating point and 'f' postfix
	{
		t->type = TK_NUMBER;
		return false;
	}

	return true;
}

LEXER_STATIC bool cond_ident_(Token *t, int ch, bool *undo)
{
	*undo = true;
	if(ch >= 'a' && ch <= 'z')
		return false;
	if(ch >= 'A' && ch <= 'Z')
		return false;
	if(ch >= '0' && ch <= '9')
		return false;
	if(ch == '_')// || ch == '\\')
		return false;
	return true;
}

LEXER_STATIC bool cond_file_ref_(Token *t, int ch, bool *undo)
{
	*undo = true;
	if(ch == '\\')
		return false;
	return cond_ident_(t, ch, undo);
}

LEXER_STATIC bool cond_single_line_comment_(Token *t, int ch, bool *undo)
{
	*undo = true;
	//\0 is implicitly handled by the if(!ch) check in lexer_parse_characters
	return ch == '\r' || ch == '\n';
}

LEXER_STATIC bool cond_whitespace_(Token *t, int ch, bool *undo)
{
	*undo = true;
	// return !(ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t');
	return !(ch == '\r' || ch == ' ' || ch == '\t');
}

LEXER_STATIC bool lexer_step(Lexer *lexer, Token *t);

LEXER_STATIC bool lexer_accept(Lexer *lexer, TokenType type, Token *t)
{
	Token _;
	if(!t)
	{
		t = &_;
	}

	Stream *s = lexer->stream;

	int64_t offset = s->tell(s);
	if(!lexer_step(lexer, t))
		return false;

	if(type != t->type)
	{
		// Undo
		s->seek(s, offset, SEEK_SET);
		return false;
	}
	return true;
}
#define lexer_expect(lexer, type, t) lexer_expect_(lexer, type, t, __FILE__, __LINE__)
LEXER_STATIC void lexer_expect_(Lexer *lexer, int type, Token *t, const char *file, int line)
{
	Token _;
	if(!t)
	{
		t = &_;
	}
	char expected[64];
	if(type == -1)
	{
		if(!lexer_step(lexer, t))
		{
			lexer_error(lexer, "Expected '%s' got EOF", token_type_to_string(type, expected, sizeof(expected)));
		}
		return;
	}

	if(!lexer_accept(lexer, type, t))
	{
		char got[64];
		lexer_error(lexer,
					"Expected '%s' got '%s' (%s:%d)",
					token_type_to_string(type, expected, sizeof(expected)),
					token_type_to_string(t->type, got, sizeof(got)),
					file,
					line);
	}
}

LEXER_STATIC void lexer_match_identifier(Lexer *lexer, Token *t, uint32_t hash)
{
#define IDENT_COMPARE(NAME, HASH) \
	case HASH: t->type = TK_##NAME; break;
	switch(hash)
	{
		IDENTS(IDENT_COMPARE)
	}
}

LEXER_STATIC bool lexer_step(Lexer *lexer, Token *t)
{
	Stream *s = lexer->stream;
	unsigned char ch = 0;
repeat:
	t->type = 0;
	t->length = 0;
	t->offset = s->tell(s);
	t->next_offset = -1;

	ch = lexer_advance(lexer);
	if(!ch)
	{
		return false;
	}
	t->type = ch;
	switch(ch)
	{
		case '"':
			t->offset = s->tell(s);
			lexer_parse_string(lexer, t);
			break;

		case '.':
		{
			// if(lexer_match_char(lexer, '.') && lexer_match_char(lexer, '.'))
			// {
			// 	t->type = TK_ELLIPSIS;
			// }
			// else
			{
				ch = lexer_advance(lexer);
				lexer_unget(lexer, NULL);
				if(ch >= '0' && ch <= '9')
				{
					lexer_unget(lexer, t);
					lexer_parse_characters(lexer, t, TK_NUMBER, cond_numeric_);
				}
			}
		}
		break;

		#define EQ_OPERATORS_CASE(NAME, CH) case CH:
		EQ_OPERATORS(EQ_OPERATORS_CASE)
		{
			if(lexer_match_char(lexer, '='))
			{
				t->type = eq_op_char_map[t->type];
			}
		}
		break;

		case ':':
			if(lexer_match_char(lexer, ':'))
				t->type = TK_SCOPE_RESOLUTION;
		break;
		
		case '-':
			if(lexer_match_char(lexer, '-'))
				t->type = TK_DECREMENT;
			else if(lexer_match_char(lexer, '='))
				t->type = TK_MINUS_ASSIGN;
		break;

		case '+':
			if(lexer_match_char(lexer, '+'))
				t->type = TK_INCREMENT;
			else if(lexer_match_char(lexer, '='))
				t->type = TK_PLUS_ASSIGN;
		break;

		case '&':
			if(lexer_match_char(lexer, '='))
			{
				t->type = TK_AND_ASSIGN;
			}
			else if(lexer_match_char(lexer, '&'))
			{
				t->type = TK_LOGICAL_AND;
			}
			break;

		case '|':
			if(lexer_match_char(lexer, '='))
			{
				t->type = TK_OR_ASSIGN;
			}
			else if(lexer_match_char(lexer, '|'))
			{
				t->type = TK_LOGICAL_OR;
			}
			break;

		case '<':
			if(lexer_match_char(lexer, '<'))
			{
				t->type = TK_LSHIFT;
				if(lexer_match_char(lexer, '='))
				{
					t->type = TK_LSHIFT_ASSIGN;
				}
			}
			else if(lexer_match_char(lexer, '='))
			{
				t->type = TK_LEQUAL;
			}
			break;

		case '>':
			if(lexer_match_char(lexer, '>'))
			{
				t->type = TK_RSHIFT;
				if(lexer_match_char(lexer, '='))
				{
					t->type = TK_RSHIFT_ASSIGN;
				}
			}
			else if(lexer_match_char(lexer, '='))
			{
				t->type = TK_GEQUAL;
			}
			break;

		case '\t':
		case ' ':
		case '\r':
			lexer_unget(lexer, t);
			lexer_parse_characters(lexer, t, TK_WHITESPACE, cond_whitespace_);
			if(!(lexer->flags & LEXER_FLAG_TOKENIZE_WHITESPACE))
				goto repeat;
			break;

		case '\n':
			lexer->line++;
			if(!(lexer->flags & LEXER_FLAG_TOKENIZE_NEWLINES))
				goto repeat;
		break;

		case '/':
		{
			ch = lexer_advance(lexer);
			if(ch == '/')
			{
				lexer_parse_characters(lexer, t, TK_COMMENT, cond_single_line_comment_);
				if(!(lexer->flags & LEXER_FLAG_TOKENIZE_COMMENTS))
					goto repeat;
			}
			else if(ch == '*' || ch == '#') // Treat /# as comment for now
			{
				lexer_parse_multiline_comment(lexer, TK_COMMENT, t, ch);
				if(!(lexer->flags & LEXER_FLAG_TOKENIZE_COMMENTS))
					goto repeat;
			}
			else if(ch == '=')
			{
				t->type = TK_DIV_ASSIGN;
			}
			else
			{
				lexer_unget(lexer, NULL);
			}
		}
		break;
		default:
		{
			if(ch >= '0' && ch <= '9')
			{
				lexer_unget(lexer, t);
				lexer_parse_characters(lexer, t, TK_INTEGER, cond_numeric_);
			}
			else if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_')
			{
				lexer_unget(lexer, t);
				lexer_match_identifier(lexer, t, lexer_parse_characters(lexer, t, TK_IDENTIFIER, cond_ident_));
				if(t->type == TK_IDENTIFIER && lexer_match_char(lexer, '\\'))
				{
					lexer_unget(lexer, t);
					lexer_parse_characters(lexer, t, TK_FILE_REFERENCE, cond_file_ref_);
				}
			}
		}
		break;
	}
	if(t->length == 0 && t->type != TK_STRING && t->type != TK_COMMENT) // Ignore "container" type tokens
	{
		t->length = s->tell(s) - t->offset;
	}
	t->next_offset = s->tell(s);
	return true;
}

LEXER_STATIC void lexer_consume(Lexer *lexer, Token *t)
{
	Stream *s = lexer->stream;
	s->seek(s, t->next_offset, SEEK_SET);
}

LEXER_STATIC Token lexer_peek(Lexer *lexer)
{
	Stream *s = lexer->stream;
	int64_t current = s->tell(s);
	Token t = { 0 };
	lexer_step(lexer, &t);
	s->seek(s, current, SEEK_SET);
	return t;
}

LEXER_STATIC int64_t lexer_token_read_int(Lexer *lexer, Token *t)
{
	char str[64];
	lexer_token_read_string(lexer, t, str, sizeof(str));
	char *x = strchr(str, 'x');
	if(x)
	{
		return strtoll(x + 1, NULL, 16);
	}
	return strtoll(str, NULL, 10);
}

LEXER_STATIC double lexer_token_read_float(Lexer *lexer, Token *t)
{
	char str[256];
	lexer_token_read_string(lexer, t, str, sizeof(str));
	return atof(str);
}

LEXER_STATIC void lexer_expect_read_string(Lexer *lexer, TokenType type, char *str, size_t n)
{
	Token t;
	lexer_expect(lexer, type, &t);
	lexer_token_read_string(lexer, &t, str, n);
}
