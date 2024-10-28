
#include "ast.h"
#include "parse.h"

static const char *string(Parser *parser, TokenType type)
{
	lexer_token_read_string(parser->lexer, &parser->token, parser->string, parser->max_string_length);
	advance(parser, type);
	return parser->string;
}

static void syntax_error(Parser *parser, const char *fmt, ...)
{
	printf("PARSE ERROR: %s\n", fmt);
	exit(-1);
}

static ASTNode** statements(Parser *parser, ASTNode **root);
static ASTNode* block(Parser *parser);
static ASTNode *body(Parser *parser);

static ASTNode *statement(Parser *parser)
{
	// printf("statement() : ");
	// dump_token(parser->lexer, &parser->token);
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

		case ';':
		{
			advance(parser, ';');
			NODE(EmptyStmt, stmt);
			n = (ASTNode*)stmt;
		}
		break;

		case TK_COMMENT:
		{
			advance(parser, TK_COMMENT);
			NODE(EmptyStmt, stmt);
			n = (ASTNode*)stmt;
		}
		break;

		case TK_WHILE:
		{
			NODE(WhileStmt, stmt);
			n = (ASTNode*)stmt;
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
			n = (ASTNode*)stmt;
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
		// 	n = (ASTNode*)stmt;
		// 	advance(parser, ';');
		// }
		// break;

		case TK_BREAK:
		{
			NODE(BreakStmt, stmt);
			n = (ASTNode*)stmt;
			advance(parser, TK_BREAK);
			advance(parser, ';');
		}
		break;
		
		case TK_WAIT:
		{
			NODE(WaitStmt, stmt);
			n = (ASTNode*)stmt;
			advance(parser, TK_WAIT);
			stmt->duration = expression(parser);
			advance(parser, ';');
		}
		break;

		case TK_WAITTILLFRAMEEND:
		{
			NODE(WaitStmt, stmt);
			n = (ASTNode*)stmt;
			advance(parser, TK_WAITTILLFRAMEEND);
			stmt->duration = NULL;
			advance(parser, ';');
		}
		break;

		case TK_CONTINUE:
		{
			NODE(ContinueStmt, stmt);
			n = (ASTNode*)stmt;
			advance(parser, TK_CONTINUE);
			advance(parser, ';');
		}
		break;

		case TK_SWITCH:
		{
			NODE(SwitchStmt, stmt);

			ASTNode **root = (ASTNode**)&stmt->cases;

			n = (ASTNode*)stmt;
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

				*root = (ASTNode*)c;
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
	ASTNode **root = (ASTNode**)&n->body;
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

void parse(Parser *parser, const char *path, HashTrie *functions)
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
				// printf("ident:%s\n", ident);
				if(!strcmp(ident, "include"))
				{
					const char *path = string(parser, TK_FILE_REFERENCE);
					// printf("path:%s\n", path);
					// Node *include = malloc(sizeof(Node));

					HashTrieNode *entry = hash_trie_upsert(parser->includes, path, parser->allocator, false);
					 // TODO: FIXME
					if(entry)
					{
						for(char *p = entry->key; *p; p++)
							if(*p == '\\')
								*p = '/';
					}
					// Node *include = parser->allocator->malloc(parser->allocator->ctx, sizeof(Node));
					// include->data = ast_add_file(prog, path);
					// include->next = file->includes;
					// file->includes = include;
				} else if(!strcmp(ident, "using_animtree"))
				{
					advance(parser, '(');
					const char *tree = string(parser, TK_STRING);
					snprintf(parser->animtree, sizeof(parser->animtree), "%s", tree);
					advance(parser, ')');
					// printf("using_animtree tree:%s\n", tree);
				}
				else
				{
					syntax_error(parser, "Invalid directive");
				}
				advance(parser, ';');
			}
			break;
            
			// Global scope, function definitions
			case TK_IDENTIFIER:
			{
				NODE(Function, func);
				lexer_token_read_string(parser->lexer, &parser->token, func->name, sizeof(func->name));
				advance(parser, TK_IDENTIFIER);
				advance(parser, '(');
				ASTNode **parms = &func->parameters;
				func->parameter_count = 0;
				if(parser->token.type != ')')
				{
					while(1)
					{
						NODE(Identifier, parm);
						lexer_token_read_string(parser->lexer, &parser->token, parm->name, sizeof(parm->name));
						*parms = (ASTNode*)parm;
						parms = &((ASTNode*)parm)->next;
						++func->parameter_count;
						advance(parser, TK_IDENTIFIER);
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

				HashTrieNode *entry = hash_trie_upsert(functions, func->name, parser->allocator, false);
				if(entry->value)
				{
					lexer_error(parser->lexer, "Function '%s' already defined for '%s'", func->name, path);
				}
				entry->value = func;
			}
			break;

			default:
			{
				dump_token(parser->lexer, &parser->token);
				syntax_error(parser, "Expected identifier or #");
			}
			break;
		}
	}
}