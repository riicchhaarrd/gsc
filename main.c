#define AST_VISITOR_IMPLEMENTATION
#include "ast.h"
#include "parse.h"
#include <assert.h>
#include "visitor.h"
#include <core/ds/hash_table.h>
// #include "interpreter.h"
#include "traverse.h"
#include "compiler.h"
#include "vm.h"
#include <unistd.h>
#include "string_table.h"

static void strtolower(char *str)
{
	for(char *p = str; *p; ++p)
		*p = tolower(*p);
}

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
	char *data = calloc(1, n + 1);
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
	printf("PARSE ERROR: %s\n", fmt);
	exit(-1);
}

ASTNode *expression(Parser *parser);

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
			n = stmt;
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

		case TK_WAITTILLFRAMEEND:
		{
			NODE(WaitStmt, stmt);
			n = stmt;
			advance(parser, TK_WAITTILLFRAMEEND);
			stmt->duration = NULL;
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

static ASTFile *add_file(ASTProgram *program, const char *path)
{
	HashTableEntry *entry = hash_table_find(&program->files, path);
	if(!entry)
	{
		ASTFile *file = calloc(1, sizeof(ASTFile));
		memset(file, 0, sizeof(ASTFile));
		snprintf(file->path, sizeof(file->path), "%s", path);
		// for(size_t i = 0; file->path[i]; ++i)
		// {
		// 	if(file->path[i] == '\\')
		// 	{
		// 		file->path[i] = '/';
		// 	}
		// }
		hash_table_init(&file->functions, 10, program->allocator); // 1024 function limit for now

		entry = hash_table_insert(&program->files, path);
		entry->value = file;
	}
	return entry->value;
}

static void parse(Parser *parser, ASTProgram *prog, ASTFile *file)
{
	if(file->parsed)
		return;
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
					add_file(prog, path);
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
			
#ifdef DISK
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
						*parms = parm;
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

				HashTableEntry *entry = hash_table_insert(&file->functions, func->name);
				if(entry->value)
				{
					lexer_error(parser->lexer, "Function '%s' already defined for '%s'", func->name, file->path);
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
	file->parsed = true;
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
		// printf("Token: %s '%s' (%d[%d])\n", token_type_to_string(t.type, type, sizeof(type)), repr, t.offset, t.length);
	}
}

static bool node_fn(ASTNode *n, void *ctx)
{
	if(n->type != AST_FILE_REFERENCE)
		return false;
	ASTProgram *prog = ctx;
	add_file(prog, n->ast_file_reference_data.file);
	return false;
}

static ASTFile *parse_source(Allocator *allocator, ASTFile *file, const char *data, ASTProgram *program)
{
	file->source = data;

	char string[16384];
	Stream s = { 0 };
	StreamBuffer sb = { 0 };
	init_stream_from_buffer(&s, &sb, (unsigned char*)data, strlen(data) + 1);
	
	Lexer l = { 0 };
	lexer_init(&l, &s);
	l.flags |= LEXER_FLAG_PRINT_SOURCE_ON_ERROR;
	// l.flags |= LEXER_FLAG_TOKENIZE_WHITESPACE;
	jmp_buf jmp;
	if(setjmp(jmp))
	{
		return NULL;
	}
	l.jmp = &jmp;
	Parser parser = { 0 };
	parser.verbose = false;
	parser.allocator = allocator;
	parser.string = string;
	parser.max_string_length = sizeof(string);
	parser.lexer = &l;

	lexer_step(parser.lexer, &parser.token);
	parse(&parser, program, file);

	for(HashTableEntry *it = file->functions.head; it; it = it->next)
	{
		ASTFunction *func = it->value;
		traverse(func, node_fn, program);
	}
	return file;
}

static ASTFile *parse_file(Allocator *allocator, const char *filename, ASTProgram *program)
{
	char path[512];
	snprintf(path, sizeof(path), "%s%s.gsc", program->base_path, filename);
	for(char *p = path; *p; p++)
		*p = *p == '\\' ? '/' : *p;

	ASTFile *file = add_file(program, filename);
	unsigned char *data = read_text_file(path);
	if(!data)
	{
		printf("Can't read '%s'\n", filename);
		return NULL;
	}
	return parse_source(allocator, file, data, program);
}

static void dump_function(ASTFile *file, ASTFunction *func)
{
	// printf("\tfunc: %s::%s\n", file->path, func->name);
}

static void dump_file(ASTFile *file)
{
	if(file->parsed)
		printf("file: %s [parsed]\n", file->path);
	else
		printf("file: %s\n", file->path);
	for(HashTableEntry *it = file->functions.head; it; it = it->next)
	{
		dump_function(file, it->value);
	}
}

static void dump_program(ASTProgram *prog)
{
	for(HashTableEntry *it = prog->files.head; it; it = it->next)
	{
		dump_file(it->value);
	}
}
int get_memory_usage_kb()
{
#ifdef EMSCRIPTEN
	return -1;
#else
	FILE *file = fopen("/proc/self/status", "r");
	if(!file)
	{
		return -1;
	}

	int memory_usage_kb = -1;
	char line[256];

	while(fgets(line, sizeof(line), file))
	{
		if(strncmp(line, "VmRSS:", 6) == 0)
		{
			sscanf(line, "VmRSS: %d kB", &memory_usage_kb);
			break;
		}
	}

	fclose(file);
	return memory_usage_kb;
#endif
}

typedef struct
{
	HashTable functions;
} CompiledFile;

static HashTable compiled_files;

CompiledFile *get_file(const char *name)
{
	HashTableEntry *entry = hash_table_find(&compiled_files, name);
	if(!entry)
		return NULL;
	return entry->value;
}

VMFunction *get_function(CompiledFile *cf, const char *name)
{
	if(!cf)
		return NULL;
	HashTableEntry *entry = hash_table_find(&cf->functions, name);
	if(!entry)
		return NULL;
	return entry->value;
}

void compile_file(Allocator *allocator, Arena scratch, Compiler *c, ASTFile *file)
{
	c->file = file;
	CompiledFile *cf = calloc(1, sizeof(CompiledFile));
	hash_table_init(&cf->functions, 16, allocator);
	for(HashTableEntry *it = file->functions.head; it; it = it->next)
	{
		ASTFunction *f = it->value;
		VMFunction *compfunc = compile_function(c, file, f, scratch);
		if(!compfunc)
			continue;
		char lower_function[256];
		snprintf(lower_function, sizeof(lower_function), "%s", f->name);
		strtolower(lower_function);
		// printf("file:%s,instr:%d,name:%s,%d funcs,ast funcs:%d\n",file->path,buf_size(ins),f->name,cf->functions.length,file->functions.length);
		HashTableEntry *func_entry = hash_table_insert(&cf->functions, lower_function);
		if(func_entry)
		{
			func_entry->value = compfunc;
		}
	}
    char lower_file[256];
	snprintf(lower_file, sizeof(lower_file), "%s", file->path);
	strtolower(lower_file);
	hash_table_insert(&compiled_files, lower_file)->value = cf;
}

VMFunction *vm_func_lookup(void *ctx, const char *file, const char *function)
{
    char lower_file[256];
	snprintf(lower_file, sizeof(lower_file), "%s", file);
	strtolower(lower_file);

	CompiledFile *cf = get_file(lower_file);
	if(!cf)
		return NULL;
		
    char lower_function[256];
	snprintf(lower_function, sizeof(lower_function), "%s", function);
	strtolower(lower_function);

	VMFunction *compfunc = get_function(cf, lower_function);
	if(!compfunc)
		return NULL;
	return compfunc;
}

static char *heap;
Arena perm;
jmp_buf jmp_;

StringTable strtab;
Allocator perm_allocator;
Allocator scratch_allocator;
Arena scratch;

void init()
{
	// size_t cap = (1 << 29);
	size_t cap = (1 << 27);
	if(!heap)
		heap = malloc(cap);
	// printf("[INFO] Allocated %.2f MB\n", (float)cap / 1000.f / 1000.f);
	// getchar();
	arena_init(&perm, heap, cap);
	perm.jmp_oom = &jmp_;

	perm_allocator = arena_allocator(&perm);
	cap >>= 2;
	scratch = arena_split(&perm, cap);
	scratch_allocator = arena_allocator(&scratch);

	cap >>= 2;
	string_table_init(&strtab, arena_split(&perm, cap));
}

VM *gvm;

bool frame(float dt)
{
	if(gvm)
	{
		if(!vm_run_threads(gvm, dt))
			gvm = NULL;
	}
	return true;
}

int execute(const char *source, bool verbose)
{
	init();
	
	if(setjmp(jmp_))
	{
		printf("[ERROR] Out of memory!\n");
		return -1;
	}

	hash_table_init(&compiled_files, 12, &perm_allocator);
	
	Compiler compiler = { 0 };
	jmp_buf jmp;
	ASTProgram *program = NULL;
	ASTFile *file = NULL;
	if(setjmp(jmp))
	{
		fprintf(stderr, "Failed");
		return -1;
	}
	// Interpreter interp = { 0 };
	// hash_table_init(&interp.stringtable, 16);
	// interp.jmp = &jmp;
	void ast_visitor_gsc_init(ASTVisitor *v);
	compiler_init(&compiler, &jmp, &perm_allocator, &strtab);

	static const char *base_path = "scripts/";
	
	program = new(&scratch, ASTProgram, 1);
	program->allocator = &scratch_allocator;
	snprintf(program->base_path, sizeof(program->base_path), "%s", base_path);

	hash_table_init(&program->files, 10, &scratch_allocator);
	const char *input_file = "*source";
	file = add_file(program, input_file);
	parse_source(&scratch_allocator, file, source, program);

	// compiler.source = program->source;

	// for(HashTableEntry *it = program->files.head; it; it = it->next)
	// {
	// 	ASTFile *f = it->value;
	// 	if(f->parsed)
	// 		continue;
	// 	parse_file(&scratch_allocator, f->path, program);
	// }
	
	// Compile all functions
	for(HashTableEntry *it = program->files.head; it; it = it->next)
	{
		ASTFile *f = it->value;
		if(!f->parsed)
			continue;
		compile_file(&perm_allocator, scratch, &compiler, f);
		// printf("%f MB", get_memory_usage_kb() / 1000.f);
		// getchar();
	}
	// getchar();
	
	VM *vm = perm_allocator.malloc(perm_allocator.ctx, sizeof(VM));
	vm_init(vm, &perm_allocator, &strtab);
	vm->flags = VM_FLAG_NONE;
	if(verbose)
		vm->flags |= VM_FLAG_VERBOSE;
	vm->jmp = &jmp;
	vm->ctx = program;
	vm->func_lookup = vm_func_lookup;
	compiler_free(&compiler);
	hash_table_destroy(&program->files);

	void register_c_functions(VM *vm);
	register_c_functions(vm);

	VMFunction *cf = get_function(get_file(input_file), "main");
	if(cf)
	{
		// dump_instructions(&compiler, cf->instructions);

		vm_call_function_thread(vm, input_file, "main", 0, NULL);
		gvm = vm;
	}
	else
	{
		printf("can't find %s::%s\n", input_file, "main");
	}
}

int execute_file(const char *input_file, bool verbose)
{
	init();
	
	if(setjmp(jmp_))
	{
		printf("[ERROR] Out of memory!\n");
		return -1;
	}

	if(!input_file)
	{
		fprintf(stderr, "No input file provided.\n");
		return -1;
	}

	hash_table_init(&compiled_files, 12, &perm_allocator);
	
	Compiler compiler = { 0 };
	jmp_buf jmp;
	ASTProgram *program = NULL;
	ASTFile *file = NULL;
	if(setjmp(jmp))
	{
		fprintf(stderr, "Failed");
		return -1;
	}
	// Interpreter interp = { 0 };
	// hash_table_init(&interp.stringtable, 16);
	// interp.jmp = &jmp;
	void ast_visitor_gsc_init(ASTVisitor *v);
	compiler_init(&compiler, &jmp, &perm_allocator, &strtab);
	
	#ifdef DISK
	static const char *base_path = "scripts/";
	
	program = new(&scratch, ASTProgram, 1);
	program->allocator = &scratch_allocator;
	snprintf(program->base_path, sizeof(program->base_path), "%s", base_path);

	hash_table_init(&program->files, 10, &scratch_allocator);
	file = parse_file(&scratch_allocator, input_file, program);

	// compiler.source = program->source;

	for(HashTableEntry *it = program->files.head; it; it = it->next)
	{
		ASTFile *f = it->value;
		if(f->parsed)
			continue;
		parse_file(&scratch_allocator, f->path, program);
	}
	
	// Compile all functions
	for(HashTableEntry *it = program->files.head; it; it = it->next)
	{
		ASTFile *f = it->value;
		if(!f->parsed)
			continue;
		CompiledFile *cf = compile_file(&perm_allocator, scratch, &compiler, f);
		for(Node *include = f->includes; include; include = include->next)
		{
			CompiledFile *cfi = compile_file(&perm_allocator, scratch, &compiler, include->data);
			for(HashTableEntry *cfi_entry = cfi->functions.head; cfi_entry; cfi_entry = cfi_entry->next)
			{
				hash_table_insert(&cf->functions, cfi_entry->key)->value = cfi_entry->value;
			}
		}
		printf("%f MB", get_memory_usage_kb() / 1000.f);
		// getchar();
	}
	// getchar();
	
	VM *vm = perm_allocator.malloc(perm_allocator.ctx, sizeof(VM));
	vm_init(vm, &perm_allocator, &strtab);
	vm->flags = VM_FLAG_NONE;
	if(verbose)
		vm->flags |= VM_FLAG_VERBOSE;
	vm->jmp = &jmp;
	vm->ctx = program;
	vm->func_lookup = vm_func_lookup;
	compiler_free(&compiler);
	hash_table_destroy(&program->files);

	void register_c_functions(VM *vm);
	register_c_functions(vm);

	VMFunction *cf = get_function(get_file(input_file), "main");
	if(cf)
	{
		// dump_instructions(&compiler, cf->instructions);

		vm_call_function_thread(vm, input_file, "main", 0, NULL);
		while(1)
		{
			float dt = 1.f / 20.f;
			if(!vm_run_threads(vm, dt))
				break;
			usleep(20000);
		}
	}
	else
	{
		printf("can't find %s::%s\n", input_file, "main");
	}

	// dump_program(program);
	// interp.program = program;
	vm_cleanup(vm);
	for(HashTableEntry *it = compiled_files.head; it; it = it->next)
	{
		CompiledFile *cf = it->value;
		for(HashTableEntry *it_fun = cf->functions.head; it_fun; it_fun = it_fun->next)
		{
			VMFunction *vmf = it_fun->value;
			buf_free(vmf->instructions);
			free(vmf);
		}
		hash_table_clear(&cf->functions);
		free(cf);
	}
	hash_table_clear(&compiled_files);
	// call_function(&interp, "maps/moscow", "main", 0);
	printf("%f MB", get_memory_usage_kb() / 1000.f);
	#else
	char line[16384];
	while(fgets(line, sizeof(line), stdin))
	{
    	parse_line(line);
	}
	#endif
	return 0;
}
