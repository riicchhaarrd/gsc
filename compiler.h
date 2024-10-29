#pragma once

#include "instruction.h"
#include <core/ds/buf.h>
#include <core/arena.h>
#include <core/ds/list.h>
#include <core/ds/hash_trie.h>
#include <core/ds/hash_table.h>
#include "string_table.h"
#include "ast.h"

typedef struct ASTNode ASTNode;

typedef struct
{
	Node *continue_list;
	Node *break_list;
	ASTNode *node;
} Scope;

#define COMPILER_MAX_SCOPES (32)

// typedef struct VMFunction VMFunction;
typedef struct
{
	size_t variable_index;
	HashTrie variables;
	jmp_buf *jmp;

    Instruction *instructions;
	Arena *arena;
	StringTable *strings;
	Scope scopes[COMPILER_MAX_SCOPES];
	size_t current_scope;
	// Makes debugging easier, set source if we still have source available and node to the statement for line number information
	// const char *source;
	ASTNode *node;
	// int line_number;
	char string[2048];
	const char *source;
	const char *path;
} Compiler;

void dump_instructions(Compiler *c, Instruction *instructions);
Instruction *compile_function(Compiler *c,
							  Arena *perm,
							  Arena temp,
							  ASTFunction *n,
							  int *local_count,
							  CompiledFunction *);
// int compile_file(const char *path, CompiledFile *cf, Arena *perm, Arena scratch, StringTable *strtab);
int compile_file(const char *path, const char *data, CompiledFile *cf, Arena *perm, Arena scratch, StringTable *strtab);