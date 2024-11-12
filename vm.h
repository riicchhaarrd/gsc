#pragma once

#include "buf.h"
#include "arena.h"
#include <stdarg.h>
#include <setjmp.h>
#include "variable.h"
#include "instruction.h"
#include "allocator.h"
#include "hash_trie.h"
#include "object_pool.h"
#include "string_table.h"
#include "include/gsc.h"

// #define VM_THREAD_ID_INVALID (-1)
// typedef int VMThreadId;

typedef struct VM VM;
typedef int (*vm_CFunction)(VM *);

int vm_checkobject(VM *vm, int idx);
int64_t vm_checkinteger(VM *vm, int idx);
bool vm_checkbool(VM *vm, int idx);
void vm_pushinteger(VM*, int64_t);
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
    // void *getter, *setter;
	ObjectField *next;
};
enum { sizeof_ObjectField = sizeof(ObjectField) };

// Lua has metatables and JavaScript has prototypes and other languages have other namings for it like magic functions
// I would call it a template, but this keyword is reserved in C++
// It all boils down to the same thing really but I guess I'll just go with the word proxy or prototype, an alternative would be blueprint
// Prototypes in JavaScript are objects, this allows for defining them in the language itself, but in this case I'll just opt for the simpler pointer interface
// I guess I could change it in the future if need be, considering object prototypes are more powerful

typedef struct Object Object;

typedef struct
{
	const char *file;
	const char *function;
	int line;
} gsc_DebugInfo;

struct Object
{
    ObjectField **tail;
    ObjectField *fields;
    int field_count;
    // Maybe set it _on_ the object itself as a ObjectField with a underscore post/pre fix?
    // Just to save 4/8 bytes lol
    int refcount;
    const char *tag;
    void *userdata;
    // Object *base;
    Object *proxy;
    gsc_DebugInfo debug_info;
};
enum { sizeof_Object = sizeof(Object) };

ObjectField *vm_object_upsert(VM *vm, Object *obj, const char *key);

typedef struct
{
    size_t length;
    char *data;
} VariableString;

#pragma pack(push, 1)
typedef union
{
    int64_t ival;
    float fval;
    VariableString sval;
    Object *oval;
    float vval[3];
    struct
    {
        bool is_native;
        union
		{
			struct
			{
				int file;
				int function;
			};
            gsc_Function native_function;
		};
	} funval;
    Variable *refval;
    //void *getter, *setter;
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

#define VM_MAX_EVENT_ARGS (16)

typedef struct
{
    int name;
    Object *object;
    Variable arguments[VM_MAX_EVENT_ARGS];
    int numargs;
    int frame;
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

#define VM_STACK_SIZE (64)
#define VM_FRAME_SIZE (16)
// #define VM_THREAD_POOL_SIZE (2048)
#define VM_THREAD_POOL_SIZE (8192)

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
    Variable *return_value;
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
    Thread temp_thread;
    VMEvent events[VM_MAX_EVENTS_PER_FRAME];
    // size_t event_count;
	int flags;
    Variable globals[VAR_GLOB_MAX];
    Variable global_object;
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
    HashTrie callback_functions;
    // HashTrie callback_methods;
	CompiledFunction *(*func_lookup)(void *ctx, const char *file, const char *function);

    gsc_DebugInfo debug_info;

    int nargs, fsp;

    // struct
    // {
    //     int __call;
    // } string_index;

    int frame;
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

void vm_register_callback_function(VM *vm, const char *name, void *callback, void *ctx);
void vm_register_c_function(VM *vm, const char *name, vm_CFunction callback);

const char *vm_stringify(VM *vm, Variable *v, char *buf, size_t n);
size_t vm_argc(VM *vm);
Variable *vm_argv(VM *vm, int idx);
Variable vm_create_object(VM *vm);
void vm_pushvar(VM *vm, Variable*);
int vm_pushobject(VM *vm, Object *o);
Thread *vm_thread(VM*);
void vm_print_thread_info(VM *vm);
void vm_notify(VM *vm, Object *object, const char *key, size_t nargs);
void vm_error(VM *vm, const char *fmt, ...);
// Variable* vm_dup(VM *vm, Variable* v);
void vm_set_object_field(VM *vm, int obj_index, const char *key);
void vm_get_object_field(VM *vm, int obj_index, const char *key);
uint32_t vm_random(VM *vm);
Variable vm_pop(VM *vm);
Variable *vm_stack(VM *vm, int idx);
Variable *vm_stack_top(VM *vm, int idx);

bool vm_cast_bool(VM *vm, Variable *arg);
int64_t vm_cast_int(VM *vm, Variable *arg);
float vm_cast_float(VM *vm, Variable *arg);
void vm_cast_vector(VM *vm, Variable *arg, float *outvec);
const char *vm_cast_string(VM *vm, Variable *arg);
Object *vm_cast_object(VM *vm, Variable *arg);
Object *vm_allocate_object(VM *vm);
