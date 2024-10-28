#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <core/arena.h>
#include "string_table.h"
#include "compiler.h"
#include "vm.h"
#include <setjmp.h>

#ifdef EMSCRIPTEN
	#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE void gsc_execute_file(const char *input_file)
{
	execute_file(input_file, true);
}

EMSCRIPTEN_KEEPALIVE void gsc_execute(const char *source)
{
	execute(source, false);
}

static double previous;
void em_frame()
{
	double current = emscripten_get_now();
	double dt = (current - previous) / 1000.0;
	previous = current;
	if(!frame(dt))
	{
		emscripten_cancel_main_loop();
	}
}
#endif

typedef struct
{
	char *input_file;
	bool verbose;
} Opts;

static Opts opts = { .verbose = false, .input_file = NULL };

static void parse_opts(int argc, char **argv)
{
	for(int i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-v"))
		{
			opts.verbose = true;
		}
		else // if(i == argc - 1)
		{
			opts.input_file = argv[i];
		}
	}
}
static const char *base_path = "scripts";

void find_gsc_files(const char *dir_path, int depth);

#ifdef GSC_STANDALONE

static HashTrie files;

Arena perm;
char *heap;
StringTable strtab;
Arena temp;
Allocator allocator;

CompiledFile *find_or_create_compiled_file(const char *path)
{
	HashTrieNode *entry = hash_trie_upsert(&files, path, &allocator, false);
	if(!entry->value)
	{
		CompiledFile *cf = new(&perm, CompiledFile, 1);
		cf->state = COMPILE_STATE_NOT_STARTED;
		hash_trie_init(&cf->file_references);
		hash_trie_init(&cf->functions);
		hash_trie_init(&cf->includes);
		entry->value = cf;
	}
	return entry->value;
}
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

void compile(const char *fullpath, const char *path)
{
	CompiledFile *cf = find_or_create_compiled_file(path);
	if(cf->state != COMPILE_STATE_NOT_STARTED)
		return;
	const char *data = read_text_file(fullpath);
	int status = compile_file(fullpath, data, cf, &perm, temp, &strtab);
	cf->state = status == 0 ? COMPILE_STATE_DONE : COMPILE_STATE_FAILED;
	if(cf->state != COMPILE_STATE_DONE)
		return;
	for(HashTrieNode *it = cf->functions.head; it; it = it->next)
	{
		CompiledFunction *f = it->value;
		// printf("%s (%d ins)\n", it->key, buf_size(f->instructions));
	}
	for(HashTrieNode *it = cf->file_references.head; it; it = it->next)
	{
		// printf("reference: '%s'\n", it->key);
		find_or_create_compiled_file(it->key);
	}
	for(HashTrieNode *it = cf->includes.head; it; it = it->next)
	{
		// printf("include: '%s'\n", it->key);
		find_or_create_compiled_file(it->key);
	}
}

int main(int argc, char **argv)
{
	find_gsc_files(base_path, 0);
	parse_opts(argc, argv);
	#ifndef EMSCRIPTEN
	size_t N = 1 << 28;
	heap = malloc(N); // 256MiB
	arena_init(&perm, heap, N);
	allocator = arena_allocator(&perm);
	jmp_buf jmp;
	if(setjmp(jmp))
	{
		printf("Out of memory!");
		return -1;
	}
	perm.jmp_oom = &jmp;
	temp = arena_split(&perm, N >> 2);
	string_table_init(&strtab, arena_split(&perm, N >> 4));
	char path[256];
	snprintf(path, sizeof(path), "%s/%s.gsc", base_path, opts.input_file);
	compile(path, opts.input_file);
	while(1)
	{
		bool done = true;
		for(HashTrieNode *it = files.head; it; it = it->next)
		{
			CompiledFile *cf = it->value;
			if(cf->state != COMPILE_STATE_NOT_STARTED)
				continue;
			done = false;
			snprintf(path, sizeof(path), "%s/%s.gsc", base_path, it->key);
			compile(path, it->key);
		}
		if(done)
			break;
	}
	printf("done\n");
	return 0;
	#else
	previous = emscripten_get_now();
	emscripten_set_main_loop(em_frame, 0, 1);
	return 0; // unreachable
	#endif
}
#endif