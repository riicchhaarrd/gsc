#pragma once

#include "instruction.h"
#include <core/ds/buf.h>

typedef struct
{
	size_t variable_index;
	HashTable variables;
	jmp_buf *jmp;

	size_t string_index;
	HashTable strings;
	size_t label;
    Instruction *instructions;
    // FILE *out;
    char **string_table;
} Compiler;

void dump_instructions(Compiler *c, Instruction *instructions);
Instruction *compile_function(Compiler *c, ASTFunction *func);
void compiler_init(Compiler *c, jmp_buf *jmp);
void compile_file(Compiler *c, ASTFile *f);