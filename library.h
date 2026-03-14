#pragma once
#include "arena.h"
#include "string_table.h"
#include "vm.h"
#include "include/gsc.h"
#include "hash_trie.h"

/* One slot in the reference registry.
   When free: value.type is undefined, next_free points to next free slot.
   When occupied: value holds the Variable, next_free == -1 as sentinel. */
typedef struct
{
	Variable value;
	int      next_free; /* index of next free slot, or -1 if occupied */
} gsc_RefSlot;

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

	/* Reference registry — dynamically allocated, free-list for O(1) alloc/free. */
	gsc_RefSlot *ref_slots;
	int          ref_capacity;
	int          ref_free; /* head of free list, -1 = full */
};
