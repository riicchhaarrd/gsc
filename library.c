#define GSC_EXPORTS
#ifdef GSC_EXPORTS

#include "include/gsc.h"
#include <core/ds/hash_trie.h>
#include "vm.h"
#include "ast.h"
#include "compiler.h"

#define SMALL_STACK_SIZE (16)

struct gsc_State
{
	HashTrie files;
	
	gsc_CreateOptions options;
	Allocator allocator;
	char *heap;
	Arena arena;
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
};

CompiledFile *find_or_create_compiled_file(gsc_State *state, const char *path)
{
	HashTrieNode *entry = hash_trie_upsert(&state->files, path, &state->allocator, false);
	if(!entry->value)
	{
		CompiledFile *cf = new(&state->arena, CompiledFile, 1);
		cf->state = COMPILE_STATE_NOT_STARTED;
		hash_trie_init(&cf->file_references);
		hash_trie_init(&cf->functions);
		hash_trie_init(&cf->includes);
		entry->value = cf;
	}
	return entry->value;
}

void compile(gsc_State *state, const char *path, const char *data)
{
	CompiledFile *cf = find_or_create_compiled_file(state, path);
	if(cf->state != COMPILE_STATE_NOT_STARTED)
		return;
	int status = compile_file(path, data, cf, &state->arena, state->temp, &state->strtab);
	cf->state = status == 0 ? COMPILE_STATE_DONE : COMPILE_STATE_FAILED;
	if(cf->state != COMPILE_STATE_DONE)
		return;
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
	return state->options.allocate_memory(state->options.userdata, size);
}

static void gsc_free(void *ctx, void *ptr)
{
	gsc_State *state = (gsc_State*)ctx;
	state->options.free_memory(state->options.userdata, ptr);
}

static CompiledFile *get_file(gsc_State *state, const char *file)
{
	HashTrieNode *n = hash_trie_upsert(&state->files, file, NULL, false);
	if(n && n->value)
		return n->value;
	return NULL;
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

	ctx->allocator.ctx = ctx;
	ctx->allocator.malloc = gsc_malloc;
	ctx->allocator.free = gsc_free;

	hash_trie_init(&ctx->files);
	// hash_trie_init(&ctx->c_functions);
	// hash_trie_init(&ctx->c_methods);

	// TODO: FIXME
	size_t n = (1 << 28); // 256 MiB
	ctx->heap = options.allocate_memory(options.userdata, n);
	arena_init(&ctx->arena, ctx->heap, n);
	ctx->temp = arena_split(&ctx->arena, n >>= 2);

	ctx->strtab_arena = arena_split(&ctx->arena, n >>= 2);
	string_table_init(&ctx->strtab, ctx->strtab_arena);

	VM *vm = options.allocate_memory(options.userdata, sizeof(VM));
	vm_init(vm, &ctx->allocator, &ctx->strtab);
	vm->flags = VM_FLAG_NONE;
	// vm->flags |= VM_FLAG_VERBOSE;
	vm->jmp = NULL;
	vm->ctx = ctx;
	vm->func_lookup = vm_func_lookup;
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
		opts.free_memory(opts.userdata, state->vm);
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

int gsc_compile(gsc_State *state, const char *filename)
{
	int status = GSC_OK;
	const char *source = state->options.read_file(state->options.userdata, filename, &status);
	if(status != GSC_OK)
		return status;
	compile(state, filename, source);
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
	CHECK_ERROR(state);
	float dt = 1.f / (float)delta_time;
	if(!vm_run_threads(state->vm, dt))
		return GSC_DONE;
	return GSC_OK;
}

int gsc_call(gsc_State *state, const char *namespace, const char *function, int nargs)
{
	CHECK_ERROR(state);
	//TODO: handle args
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

#endif