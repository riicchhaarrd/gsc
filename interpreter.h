#pragma once

#include <core/ds/hash_table.h>
#include <setjmp.h>

enum
{
	VAR_STRING,
	VAR_INTEGER,
	VAR_BOOLEAN,
	VAR_FLOAT,
	VAR_VECTOR,
	VAR_ANIMATION,
	VAR_FUNCTION,
	VAR_LOCALIZED_STRING,
	VAR_UNDEFINED,
    VAR_OBJECT
};

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
} VariableValue;

struct Variable
{
	int type;
	VariableValue u;
};

#define STACK_FRAME_LIMIT (256)

typedef struct
{
    HashTable locals;
    size_t local_index;
    // Variable *locals[MAX_LOCAL_VARS];
} StackFrame;

#define STACK_LIMIT (256)

typedef struct
{
    Variable *stack[STACK_LIMIT];
    StackFrame frames[STACK_FRAME_LIMIT];
    StackFrame *frame;
    int sp;
} Thread;

typedef struct
{
	size_t string_index;
    HashTable stringtable;
    jmp_buf *jmp;
    Thread *thread;
} Interpreter;