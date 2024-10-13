#pragma once

#include "instruction.h"
#include <core/ds/buf.h>
#include <core/arena.h>
#include <core/ds/list.h>
#include <core/ds/hash_trie.h>
#include <core/ds/hash_table.h>
#include "string_table.h"

typedef struct
{
	Node *continue_list;
	Node *break_list;
	Arena arena;
} Scope;

#define COMPILER_MAX_SCOPES (32)

typedef struct VMFunction VMFunction;
typedef struct
{
	size_t variable_index;
	HashTrie variables;
	jmp_buf *jmp;

    Instruction *instructions;
    // FILE *out;
	
	Arena arena;
	StringTable *strings;

	Scope scopes[COMPILER_MAX_SCOPES];
	size_t current_scope;

	// Makes debugging easier, set source if we still have source available and node to the statement for line number information
	const char *source;
	ASTNode *node;
	int line_number;
} Compiler;

void dump_instructions(Compiler *c, Instruction *instructions);
VMFunction *compile_function(Compiler *c, ASTFunction *func, Arena arena);
void compiler_init(Compiler *c, jmp_buf *jmp, Arena arena, Allocator*, StringTable*);
void compiler_free(Compiler *);