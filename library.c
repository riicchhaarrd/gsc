#define GSC_EXPORTS
#ifdef GSC_EXPORTS

#include "include/gsc.h"
#include <core/ds/hash_trie.h>
#include "vm.h"
#include "ast.h"
#include "compiler.h"
#include <setjmp.h>

#define SMALL_STACK_SIZE (16)

struct gsc_State
{
	HashTrie files;
	
	gsc_CreateOptions options;
	Allocator allocator;
	char *heap;
	Arena perm;
	Arena temp;
	// HashTrie c_functions;
	// HashTrie c_methods;

	Variable small_stack[SMALL_STACK_SIZE];
	int sp;

	bool error;
	char error_message[2048];

	Arena strtab_arena;
	StringTable strtab;

	VM *vm;

	jmp_buf jmp_oom;
};

CompiledFile *find_or_create_compiled_file(gsc_State *state, const char *path)
{
	HashTrieNode *entry = hash_trie_upsert(&state->files, path, &state->allocator, false);
	if(!entry->value)
	{
		CompiledFile *cf = new(&state->perm, CompiledFile, 1);
		cf->state = COMPILE_STATE_NOT_STARTED;
		cf->name = entry->key;
		hash_trie_init(&cf->file_references);
		hash_trie_init(&cf->functions);
		hash_trie_init(&cf->includes);
		entry->value = cf;
	}
	return entry->value;
}

CompiledFile *compile(gsc_State *state, const char *path, const char *data)
{
	CompiledFile *cf = find_or_create_compiled_file(state, path);
	if(cf->state != COMPILE_STATE_NOT_STARTED)
		return cf;
	int status = compile_file(path, data, cf, &state->perm, state->temp, &state->strtab);
	cf->state = status == 0 ? COMPILE_STATE_DONE : COMPILE_STATE_FAILED;
	if(cf->state != COMPILE_STATE_DONE)
		return cf;
	// printf("%s %s\n", path, cf->name);
	for(HashTrieNode *it = cf->functions.head; it; it = it->next)
	{
		CompiledFunction *f = it->value;
		// printf("%s (%d instructions)\n", it->key, buf_size(f->instructions));
	}
	for(HashTrieNode *it = cf->file_references.head; it; it = it->next)
	{
		// printf("reference: '%s'\n", it->key);
		find_or_create_compiled_file(state, it->key);
	}
	for(HashTrieNode *it = cf->includes.head; it; it = it->next)
	{
		// printf("include: '%s'\n", it->key);
		find_or_create_compiled_file(state, it->key);
	}
	return cf;
}

static int error(gsc_State *state, const char *fmt, ...)
{
	state->error = true;
	va_list va;
	va_start(va, fmt);
	vsnprintf(state->error_message, sizeof(state->error_message), fmt, va);
	va_end(va);
	return 1;
}

#define CHECK_ERROR(state) \
	if(state->error)       \
	{                      \
		return 1;          \
	}

static void *gsc_malloc(void *ctx, size_t size)
{
	gsc_State *state = (gsc_State*)ctx;
	return new(&state->perm, char, size);
	// return state->options.allocate_memory(state->options.userdata, size);
}

static void gsc_free(void *ctx, void *ptr)
{
	gsc_State *state = (gsc_State*)ctx;
	// state->options.free_memory(state->options.userdata, ptr);
}

static CompiledFile *get_file(gsc_State *state, const char *file)
{
	HashTrieNode *n = hash_trie_upsert(&state->files, file, NULL, false);
	if(!n)
		return NULL;
	CompiledFile *cf = n->value;
	if(cf->state == COMPILE_STATE_FAILED)
		return NULL;
	return cf;
}

static CompiledFunction *get_function(gsc_State *state, const char *file, const char *function)
{
	CompiledFile *f = get_file(state, file);
	if(!f)
		return NULL;
	HashTrieNode *n = hash_trie_upsert(&f->functions, function, NULL, false);
	if(n && n->value)
		return n->value;
	return NULL;
}

static CompiledFunction *vm_func_lookup(void *ctx, const char *file, const char *function)
{
	gsc_State *state = (gsc_State*)ctx;
	return get_function(state, file, function);
}

gsc_State *gsc_create(gsc_CreateOptions options)
{
	gsc_State *ctx = options.allocate_memory(options.userdata, sizeof(gsc_State));
	memset(ctx, 0, sizeof(gsc_State));
	ctx->options = options;

	if(setjmp(&ctx->jmp_oom))
	{
		return NULL;
	}

	ctx->allocator.ctx = ctx;
	ctx->allocator.malloc = gsc_malloc;
	ctx->allocator.free = gsc_free;

	hash_trie_init(&ctx->files);
	// hash_trie_init(&ctx->c_functions);
	// hash_trie_init(&ctx->c_methods);

	// TODO: FIXME
	#define HEAP_SIZE (256 * 1024 * 1024)
	ctx->heap = options.allocate_memory(options.userdata, HEAP_SIZE);
	arena_init(&ctx->perm, ctx->heap, HEAP_SIZE);
	ctx->perm.jmp_oom = &ctx->jmp_oom;
	
	#define TEMP_SIZE (64 * 1024 * 1024)

	arena_init(&ctx->temp, new(&ctx->perm, char, TEMP_SIZE), TEMP_SIZE);
	ctx->temp.jmp_oom = ctx->perm.jmp_oom;

	#define STRTAB_SIZE (128 * 1024 * 1024)
	arena_init(&ctx->strtab_arena, new(&ctx->perm, char, STRTAB_SIZE), STRTAB_SIZE);
	ctx->strtab_arena.jmp_oom = ctx->perm.jmp_oom;

	string_table_init(&ctx->strtab, ctx->strtab_arena);

	VM *vm = new(&ctx->perm, VM, 1);
	vm_init(vm, &ctx->allocator, &ctx->strtab);
	vm->flags = VM_FLAG_NONE;
	if(options.verbose)
		vm->flags |= VM_FLAG_VERBOSE;
	vm->jmp = &ctx->jmp_oom;
	vm->ctx = ctx;
	vm->func_lookup = vm_func_lookup;

	void register_dummy_c_functions(VM * vm);
	register_dummy_c_functions(vm);

	ctx->vm = vm;
	return ctx;
}

void gsc_destroy(gsc_State *state)
{
	if(state)
	{
		vm_cleanup(state->vm);

		gsc_CreateOptions opts = state->options;
		opts.free_memory(opts.userdata, state->heap);
		// opts.free_memory(opts.userdata, state->vm);
		opts.free_memory(opts.userdata, state);
	}
}
void gsc_register_function(gsc_State *state, const char *namespace, const char *name, gsc_Function callback)
{
	hash_trie_upsert(&state->vm->c_functions, name, &state->allocator, false)->value = callback;
}

void gsc_register_method(gsc_State *state, const char *namespace, const char *name, gsc_Method callback)
{
	hash_trie_upsert(&state->vm->c_methods, name, &state->allocator, false)->value = callback;
}

void gsc_object_set_field(gsc_State *state, gsc_Object *, const char *name)
{
}

int gsc_link(gsc_State *state)
{
	Allocator perm_allocator = arena_allocator(&state->perm);
	for(HashTrieNode *it = state->files.head; it; it = it->next)
	{
		CompiledFile *cf = it->value;
		if(cf->state != COMPILE_STATE_DONE)
			continue;
		// Go through all includes
		for(HashTrieNode *include = cf->includes.head; include; include = include->next)
		{
			CompiledFile *included = get_file(state, include->key);
			// If included file failed to compile for any reason, skip
			if(!included)
			{
				continue;
			}
			// printf("include->key:%s, include:%x\n", include->key, included);
			for(HashTrieNode *include_func_it = included->functions.head; include_func_it;
				include_func_it = include_func_it->next)
			{
				// If the file that's including already has a function with this name, don't overwrite it
				HashTrieNode *fnd = hash_trie_upsert(&cf->functions, include_func_it->key, &perm_allocator, false);
				if(!fnd->value)
				{
					fnd->value = include_func_it->value;
				}
			}
		}
	}
	return GSC_OK;
}

int gsc_compile(gsc_State *state, const char *filename)
{
	int status = GSC_OK;
	const char *source = state->options.read_file(state->options.userdata, filename, &status);
	if(status != GSC_OK)
		return status;
	CompiledFile *cf = compile(state, filename, source);
	switch(cf->state)
	{
		case COMPILE_STATE_DONE: return GSC_OK;
		case COMPILE_STATE_FAILED: return GSC_ERROR;
		case COMPILE_STATE_NOT_STARTED: return GSC_YIELD;
	}
	// while(1)
	// {
	// 	bool done = true;
	// 	for(HashTrieNode *it = files.head; it; it = it->next)
	// 	{
	// 		CompiledFile *cf = it->value;
	// 		if(cf->state != COMPILE_STATE_NOT_STARTED)
	// 			continue;
	// 		done = false;
	// 		snprintf(path, sizeof(path), "%s/%s.gsc", base_path, it->key);
	// 		compile(path, it->key);
	// 	}
	// 	if(done)
	// 		break;
	// }
	return status;
}

const char *gsc_next_compile_dependency(gsc_State *state)
{
	for(HashTrieNode *it = state->files.head; it; it = it->next)
	{
		CompiledFile *cf = it->value;
		if(cf->state == COMPILE_STATE_NOT_STARTED)
			return it->key;
	}
	return NULL;
}

int gsc_update(gsc_State *state, int delta_time)
{
	// const char **states[] = { [COMPILE_STATE_NOT_STARTED] = "not started",
	// 						  [COMPILE_STATE_DONE] = "done",
	// 						  [COMPILE_STATE_FAILED] = "failed" };
	// for(HashTrieNode *it = state->files.head; it; it = it->next)
	// {
	// 	CompiledFile *cf = it->value;
	// 	printf("file: %s %s, state: %s, %x\n", it->key, cf->name, states[cf->state], it->value);
	// 	for(HashTrieNode *fit = cf->functions.head; fit; fit = fit->next)
	// 	{
	// 		printf("\t%s %s %x\n", it->key, ((CompiledFunction*)fit->value)->name, fit->value);
	// 	}
	// 	// getchar();
	// }
	// // getchar();
	CHECK_ERROR(state);
	float dt = 1.f / (float)delta_time;
	if(setjmp(state->jmp_oom))
	{
		return GSC_ERROR;
	}
	if(!vm_run_threads(state->vm, dt))
		return GSC_OK;
	return GSC_YIELD;
}

int gsc_call(gsc_State *state, const char *namespace, const char *function, int nargs)
{
	CHECK_ERROR(state);
	//TODO: handle args
	if(setjmp(state->jmp_oom))
	{
		return GSC_ERROR;
	}
	vm_call_function_thread(state->vm, namespace, function, 0, nargs);
	return GSC_OK; // TODO: FIXME
}

void gsc_push(gsc_State *state, void *value)
{
	if(state->sp >= SMALL_STACK_SIZE)
	{
		error(state, "Stack pointer >= SMALL_STACK_SIZE");
		return;
	}
	state->small_stack[state->sp++] = *(Variable*)value;
}

void *gsc_pop(gsc_State *state)
{
	return NULL;
}

void gsc_push_int(gsc_State *state, int value)
{
}

void gsc_push_float(gsc_State *state, float value)
{
}

void gsc_push_string(gsc_State *state, const char *value)
{
}

int gsc_to_int(gsc_State *state, int index)
{
	return 0;
}

float gsc_to_float(gsc_State *state, int index)
{
	return 0.0f;
}

const char *gsc_to_string(gsc_State *state, int index)
{
	return NULL;
}

GSC_API void *gsc_get_internal_pointer(gsc_State *state, const char *tag)
{
	if(!strcmp(tag, "vm"))
	{
		return state->vm;
	}
	return NULL;
}

#endif