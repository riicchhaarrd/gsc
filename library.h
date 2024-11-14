#pragma once
#include "arena.h"
#include "string_table.h"
#include "vm.h"
#include "include/gsc.h"
#include "hash_trie.h"

struct gsc_Context
{
	HashTrie files;
	
	gsc_CreateOptions options;
	Allocator allocator;
	char *heap;
	Arena perm;
	Arena temp;
	// HashTrie c_functions;
	// HashTrie c_methods;

	// Variable small_stack[SMALL_STACK_SIZE];
	int sp;

	bool error;
	char error_message[2048];

	// Arena strtab_arena;
	StringTable strtab;

	VM *vm;

	jmp_buf jmp_oom;

	Object *default_object_proxy;

	HashTrie ast_globals;
};
