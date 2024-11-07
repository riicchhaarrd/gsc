#define AST_VISITOR_IMPLEMENTATION
#include "ast.h"
#include "parse.h"
#include <assert.h>
#include "visitor.h"
// #include "interpreter.h"
#include "traverse.h"
#include "compiler.h"
#include "vm.h"
#include <unistd.h>
#include "string_table.h"

FILE *open_file(const char *path);

static char *read_text_file(const char *path)
{
	// FILE *fp = fopen(path, "r");
	FILE *fp = open_file(path);
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
static bool node_fn(ASTNode *n, void *ctx)
{
	if(n->type != AST_FILE_REFERENCE)
		return false;
	Parser *parser = ctx;
	snprintf(parser->string, parser->max_string_length, "%s", n->ast_file_reference_data.file);
	// for(char *p = parser->string; *p; p++)
	// 	if(*p == '\\')
	// 		*p = '/';
	Allocator allocator = arena_allocator(parser->perm);
	hash_trie_upsert(parser->file_references, parser->string, &allocator, false);
	return false;
}

int compile_file(const char *path, const char *data, CompiledFile *cf, Arena *perm, Arena scratch, StringTable *strtab, int flags)
{
	if(!data)
		return 1;
	Allocator perm_allocator = arena_allocator(perm);
	jmp_buf jmp;
	if(setjmp(jmp))
	{
		// printf("[ERROR] Out of memory!\n");
		return 1;
	}
	
	Compiler compiler = { 0 };
	compiler.arena = &scratch;
	compiler.strings = strtab;
	compiler.jmp = &jmp;
	compiler.flags = flags;
	// char path[512];
	// snprintf(path, sizeof(path), "%s/%s.gsc", base_path, input_file);
	// for(char *p = path; *p; p++)
	// 	*p = *p == '\\' ? '/' : *p;

	// unsigned char *data = read_text_file(path);
	// if(!data)
	// {
	// 	printf("Can't read '%s'\n", path);
	// 	return 1;
	// }
	compiler.source = data;
	compiler.path = path;

	char string[16384];
	Stream s = { 0 };
	StreamBuffer sb = { 0 };
	init_stream_from_buffer(&s, &sb, (unsigned char*)data, strlen(data) + 1);
	
	Lexer l = { 0 };
	lexer_init(&l, &s);
	l.flags |= LEXER_FLAG_PRINT_SOURCE_ON_ERROR;
	// l.flags |= LEXER_FLAG_TOKENIZE_WHITESPACE;
	l.jmp = &jmp;
	Parser parser = { 0 };
	parser.verbose = false;
	// Allocator allocator = arena_allocator(&scratch);
	// parser.allocator = &allocator;
	parser.string = string;
	parser.max_string_length = sizeof(string);
	parser.lexer = &l;
	parser.perm = perm;
	parser.temp = &scratch;
	parser.file_references = &cf->file_references;
	parser.includes = &cf->includes;

	HashTrie ast_functions;
	hash_trie_init(&ast_functions);

	lexer_step(parser.lexer, &parser.token);
	parse(&parser, path, &ast_functions);

	for(HashTrieNode *it = ast_functions.head; it; it = it->next)
	{
		ASTFunction *func = it->value;
		traverse((ASTNode*)func, node_fn, &parser);
		int local_count = 0;
		CompiledFunction *compfunc = new(perm, CompiledFunction, 1);
		compfunc->file = cf;
		compfunc->parameter_count = func->parameter_count;
		Instruction *ins = compile_function(&compiler, perm, scratch, func, &local_count, compfunc);
		if(!ins)
		{
			return 1;
		}
		compfunc->local_count = local_count;
		compfunc->instructions = ins;
		HashTrieNode *entry = hash_trie_upsert(&cf->functions, func->name, &perm_allocator, false);
		entry->value = compfunc;
		compfunc->name = entry->key;
	}
	// for(HashTrieNode *it = cf->includes.head; it; it = it->next)
	// {
	// 	printf("INCLUDE:%s\n", it->key);
	// 	getchar();
	// }
	// printf("scratch %f MiB\n", arena_available_mib(&scratch));
	// getchar();
	return 0;
}

// void compile()
// {
// 	// Compile all functions
// 	// getchar();
	
// 	VM *vm = perm_allocator.malloc(perm_allocator.ctx, sizeof(VM));
// 	vm_init(vm, &perm_allocator, &strtab);
// 	vm->flags = VM_FLAG_NONE;
// 	if(verbose)
// 		vm->flags |= VM_FLAG_VERBOSE;
// 	vm->jmp = &jmp;
// 	vm->ctx = program;
// 	vm->func_lookup = vm_func_lookup;

// 	compiler_free(&compiler);
// }

// void run()
// {


// 	void register_c_functions(VM *vm);
// 	register_c_functions(vm);

// 	VMFunction *cf = get_function(get_file(input_file), "main");
// 	if(cf)
// 	{
// 		// dump_instructions(&compiler, cf->instructions);

// 		vm_call_function_thread(vm, input_file, "main", 0, NULL);
// 		while(1)
// 		{
// 			float dt = 1.f / 20.f;
// 			if(!vm_run_threads(vm, dt))
// 				break;
// 			debug_serialize(vm);
// 			debug_update();
// 			usleep(20000);
// 		}
// 	}
// 	else
// 	{
// 		printf("can't find %s::%s\n", input_file, "main");
// 	}

// 	// dump_program(program);
// 	// interp.program = program;
// 	vm_cleanup(vm);
// 	for(HashTrieNode *it = compiled_files.head; it; it = it->next)
// 	{
// 		CompiledFile *cf = it->value;
// 		for(HashTrieNode *it_fun = cf->functions.head; it_fun; it_fun = it_fun->next)
// 		{
// 			VMFunction *vmf = it_fun->value;
// 			buf_free(vmf->instructions);
// 			free(vmf);
// 		}
// 		free(cf);
// 	}
// 	// call_function(&interp, "maps/moscow", "main", 0);
// 	// printf("%f MB", get_memory_usage_kb() / 1000.f);
// }