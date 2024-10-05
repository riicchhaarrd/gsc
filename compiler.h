#pragma once

#include "instruction.h"
#include <core/ds/buf.h>
#include <core/arena.h>
#include <core/ds/list.h>

typedef struct
{
	Node *continue_list;
	Node *break_list;
	Arena arena;
} Scope;

#define COMPILER_MAX_SCOPES (32)

typedef struct
{
	size_t variable_index;
	HashTable variables;
	jmp_buf *jmp;

	size_t string_index;
	HashTable strings;
    Instruction *instructions;
    // FILE *out;
    char **string_table;
	
	Arena arena;

	Scope scopes[COMPILER_MAX_SCOPES];
	size_t current_scope;
} Compiler;

void dump_instructions(Compiler *c, Instruction *instructions);
Instruction *compile_function(Compiler *c, ASTFunction *func);
void compiler_init(Compiler *c, jmp_buf *jmp, Arena arena);
void compile_file(Compiler *c, ASTFile *f);