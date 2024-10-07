#pragma once

#include <core/ds/hash_table.h>
#include <core/ds/buf.h>
#include <core/arena.h>
#include <stdarg.h>
#include <setjmp.h>
#include "variable.h"
#include "instruction.h"

typedef struct VM VM;
typedef int (*vm_CFunction)(VM *);

int vm_checkinteger(VM*, int);
void vm_pushinteger(VM*, int);
float vm_checkfloat(VM *vm, int idx);
const char *vm_checkstring(VM *vm, int idx);
void vm_pushstring(VM *vm, const char *str);

typedef struct Variable Variable;
typedef struct
{
    HashTable fields; // Variable*
} Object;

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
} VariableValue;

struct Variable
{
	int type;
    int refcount;
	VariableValue u;
};

#define STACK_FRAME_LIMIT (256)
#define MAX_LOCAL_VARS (64)

typedef struct
{
    Variable *locals;
    size_t local_count;
    Instruction *instructions;
    const char *file, *function;
    int ip;
    Variable self;
} StackFrame;

#define STACK_LIMIT (256)

typedef struct
{
    Variable *stack[STACK_LIMIT];
    StackFrame frames[STACK_FRAME_LIMIT];
    StackFrame *frame;
    int sp, bp;
    int result;
    float wait;
} Thread;

typedef struct VMFunction VMFunction;
struct VMFunction
{
	Instruction *instructions;
	size_t parameter_count;
	size_t local_count;
};

#define VM_FLAG_NONE (0)
#define VM_FLAG_VERBOSE (1)

struct VM
{
	// size_t string_index;
    // HashTable stringtable;
    jmp_buf *jmp;
    Thread *thread;
	int flags;
	Variable level;
    Variable game;
	Arena arena;
	Arena c_function_arena;
	void *ctx;
    char **string_table;
    HashTable c_functions;
	VMFunction *(*func_lookup)(void *ctx, const char *file, const char *function);
};

void vm_call_function_thread(VM *vm, const char *file, const char *function, size_t nargs, Variable *self);
bool vm_run(VM *vm, float dt);
VM *vm_create();
void vm_register_c_function(VM *vm, const char *name, vm_CFunction callback);
const char *vm_stringify(VM *vm, Variable *v, char *buf, size_t n);
size_t vm_argc(VM *vm);
Variable *vm_argv(VM *vm, size_t idx);
Variable *vm_create_object();
void vm_pushvar(VM *vm, Variable*);
Thread *vm_thread(VM*);