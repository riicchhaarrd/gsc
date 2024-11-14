#pragma once

#include "instruction.h"
#include "buf.h"
#include "arena.h"
#include "hash_trie.h"
#include "string_table.h"
#include "ast.h"

typedef struct ASTNode ASTNode;

#define LIST_FOREACH(type, head, iterator) \
    for (type* iterator = head; iterator != NULL; iterator = iterator->next)

typedef struct Node Node;

struct Node
{
    Node *next;
    void *data;
    // char data[];
};
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
	HashTrie *globals;
	jmp_buf *jmp;

    Instruction *instructions;
	int instruction_count;
	int max_instruction_count;
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
	int flags;
} Compiler;

void dump_instructions(Compiler *c, Instruction *instructions);
int compile_function(Compiler *c,
							  Arena *perm,
							  Arena temp,
							  ASTFunction *n,
							  int *local_count,
							  CompiledFunction *);
// int compile_file(const char *path, CompiledFile *cf, Arena *perm, Arena scratch, StringTable *strtab);
int compile_file(const char *path,
				 const char *data,
				 CompiledFile *cf,
				 Arena *perm,
				 Arena scratch,
				 StringTable *strtab,
				 int flags,
				 HashTrie *globals);

int compile_node(Instruction *instructions,
				 int max_instruction_count,
				 Compiler *c,
				 Arena temp,
				 ASTNode *n,
				 jmp_buf *jmp,
				 StringTable *strtab,
				 HashTrie *globals);