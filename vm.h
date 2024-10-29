#pragma once

#include <core/ds/hash_table.h>
#include <core/ds/buf.h>
#include <core/arena.h>
#include <stdarg.h>
#include <setjmp.h>
#include "variable.h"
#include "instruction.h"
#include <core/allocator.h>
#include <core/ds/hash_trie.h>
#include <core/ds/object_pool.h>
#include "string_table.h"

// #define VM_THREAD_ID_INVALID (-1)
// typedef int VMThreadId;

typedef struct VM VM;
typedef int (*vm_CFunction)(VM *);

int vm_checkinteger(VM*, int);
bool vm_checkbool(VM *vm, int idx);
void vm_pushinteger(VM*, int);
void vm_pushfloat(VM *vm, float val);
void vm_pushbool(VM *vm, bool b);
void vm_pushundefined(VM *vm);
float vm_checkfloat(VM *vm, int idx);
void vm_checkvector(VM *vm, int idx, float *outvec);
const char *vm_checkstring(VM *vm, int idx);
void vm_pushstring(VM *vm, const char *str);
void vm_pushstring_n(VM *vm, const char *str, size_t n);
void vm_pushvector(VM *vm, float*);
int vm_string_index(VM *vm, const char *s);

typedef struct Variable Variable;
typedef struct ObjectField ObjectField;

// https://nullprogram.com/blog/2023/09/30

struct ObjectField
{
	ObjectField *child[4];
	const char *key;
	Variable *value;
	ObjectField *next;
};
enum { sizeof_ObjectField = sizeof(ObjectField) };

typedef struct Object Object;
struct Object
{
    ObjectField **tail;
    ObjectField *fields;
    int field_count;
    // Maybe set it _on_ the object itself as a ObjectField with a underscore post/pre fix?
    // Just to save 4/8 bytes lol
    int refcount;
};
enum { sizeof_Object = sizeof(Object) };

ObjectField *vm_object_upsert(VM *vm, Object *obj, const char *key);

typedef int (*vm_CMethod)(VM *, Object *self);
#pragma pack(push, 1)
typedef union
{
    int ival;
    float fval;
    char *sval;
    Object *oval;
    float vval[3];
    struct
    {
        int file;
        int function;
    } funval;
    Variable *refval;
} VariableValue;
#pragma pack(pop)
enum { sizeof_VariableValue = sizeof(VariableValue) };

// #define VAR_FLAG_NONE (0)
// #define VAR_FLAG_NO_FREE (1)

#pragma pack(push, 8)
struct Variable
{
	int type;
    // int flags;
    // int refcount;
	VariableValue u;
    // Variable *next;
};
#pragma pack(pop)

enum { sizeof_Variable = sizeof(Variable) };

#pragma pack(push, 8)
typedef struct
{
    Variable *locals;
    int local_count;
    Instruction *instructions;
    const char *file, *function;
    int ip;
    // Variable self;
} StackFrame;
#pragma pack(pop)
enum { sizeof_StackFrame = sizeof(StackFrame) };

typedef struct
{
    int name;
    Object *object;
    Variable *arguments;
} VMEvent;
enum { sizeof_VMEvent = sizeof(VMEvent) };

typedef enum
{
	VM_THREAD_INACTIVE,
	VM_THREAD_ACTIVE,
	VM_THREAD_WAITING_TIME,
	VM_THREAD_WAITING_FRAME,
	VM_THREAD_WAITING_EVENT
} VMThreadState;

static const char *vm_thread_state_names[] = { "INACTIVE",		"ACTIVE",		 "WAITING_TIME",
											   "WAITING_FRAME", "WAITING_EVENT", NULL };

#define VM_STACK_SIZE (256)
#define VM_FRAME_SIZE (256)
#define VM_THREAD_POOL_SIZE (2048)

typedef struct
{
    VMThreadState state;
    Variable stack[VM_STACK_SIZE]; // Make pointers?
    StackFrame frames[VM_FRAME_SIZE];
    // StackFrame *frame;
    int sp, bp;
    int result;
    float wait;
    VMEvent waittill;
    int *endon;
    struct{
		const char *file, *function;
	} caller;
} Thread;

enum { sizeof_Thread = sizeof(Thread) };

// typedef struct VMFunction VMFunction;
// struct VMFunction
// {
// 	Instruction *instructions;
// 	size_t parameter_count;
// 	size_t local_count;
// };

#define VM_REFCOUNT_NO_FREE (0xdeadbeef)

#define VM_FLAG_NONE (0)
#define VM_FLAG_VERBOSE (1)

#define VM_MAX_EVENTS_PER_FRAME (1024)

struct VM
{
    jmp_buf *jmp;
    Thread *thread_buffer[VM_THREAD_POOL_SIZE];
    int thread_read_idx;
    int thread_write_idx;
    
    // size_t thread_count;
    Thread *thread;
    VMEvent events[VM_MAX_EVENTS_PER_FRAME];
    size_t event_count;
	int flags;
    Variable globals[VAR_GLOB_MAX];
	// Variable level;
    // Variable game;
	// Arena arena;
    Allocator *allocator;
	Arena c_function_arena;
    uint32_t random_state; // xorshift1 state

    // Memory pools
    struct
    {
        ObjectPool threads;
        ObjectPool stack_frames;
        // ObjectPool object_fields;
        // ObjectPool variables;
        // ObjectPool objects;
        ObjectPool uo;
	} pool;
	void *ctx;
    StringTable *strings;
    HashTrie c_functions;
    HashTrie c_methods;
	CompiledFunction *(*func_lookup)(void *ctx, const char *file, const char *function);
};

// typedef struct
// {
//     VM *vm;
//     VMThreadId thread_id;
// } VMContext;

bool vm_call_function_thread(VM *vm, const char *file, const char *function, size_t nargs, Variable *self);
// bool vm_run(VM *vm, float dt);
bool vm_run_threads(VM *vm, float dt);
void vm_init(VM *vm, Allocator *allocator, StringTable *strtab);
void vm_cleanup(VM*);
void vm_register_c_function(VM *vm, const char *name, vm_CFunction callback);
void vm_register_c_method(VM *vm, const char *name, vm_CMethod callback);
const char *vm_stringify(VM *vm, Variable *v, char *buf, size_t n);
size_t vm_argc(VM *vm);
Variable *vm_argv(VM *vm, int idx);
Variable vm_create_object(VM *vm);
void vm_pushvar(VM *vm, Variable*);
void vm_pushobject(VM *vm, Object *o);
Thread *vm_thread(VM*);
void vm_print_thread_info(VM *vm);
void vm_notify(VM *vm, Object *object, const char *key, size_t nargs);
void vm_error(VM *vm, const char *fmt, ...);
// Variable* vm_dup(VM *vm, Variable* v);
ObjectField *vm_set_object_field(VM *vm, Object *o, const char *key, Variable *value);
uint32_t vm_random(VM *vm);