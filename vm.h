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
size_t vm_argc(VM *vm);

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
    // HashTable locals;
    size_t local_index;
    Variable locals[MAX_LOCAL_VARS];
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
} Thread;

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
	Instruction *(*func_lookup)(void *ctx, const char *file, const char *function);
};

void vm_call_function(VM *vm, const char *file, const char *function, size_t nargs);
void vm_run(VM *vm);
VM *vm_create();
void vm_register_c_function(VM *vm, const char *name, vm_CFunction callback);