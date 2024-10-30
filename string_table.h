#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <core/arena.h>

#define STRING_TABLE_ARY (2)

typedef struct StringTableEntry StringTableEntry;
struct StringTableEntry
{
    StringTableEntry *child[1 << STRING_TABLE_ARY];
    int offset;
};

typedef struct
{
    StringTableEntry *head;
    Arena begin; // strings
    Arena end; // entries
    char *strings;
} StringTable;

static uint64_t string_table_hash_(const char *s)
{
	uint64_t h = 0x100;
	for(ptrdiff_t i = 0; s[i]; i++)
	{
		h ^= s[i];
		h *= 1111111111111111111u;
	}
	return h;
}

static void string_table_init(StringTable *table, Arena arena)
{
    ptrdiff_t sz = arena.end - arena.beg;
    // assert(sz > 0 && (sz & (sz - 1) == 0));
    int h = sz >> 1;
    arena_init(&table->begin, arena.beg, h);
    arena_init(&table->end, arena.beg + h, h);
    table->begin.jmp_oom = arena.jmp_oom;
    table->end.jmp_oom = arena.jmp_oom;
    
    table->strings = table->begin.beg;
    table->head = NULL;
}

static float string_table_available_mib(StringTable *table)
{
    return arena_available_mib(&table->begin) + arena_available_mib(&table->end);
}

static const char *string_table_get(StringTable *table, int index)
{
    if(index < 0)
    {
        return "?";
    }
    return table->strings + index;
}

static int string_table_intern(StringTable *table, const char *string)
{
	StringTableEntry **m = &table->head;
	for(uint64_t h = string_table_hash_(string); *m; h <<= STRING_TABLE_ARY)
	{
		if(!strcmp(table->strings + (*m)->offset, string))
		{
			return (*m)->offset;
		}
		m = &(*m)->child[h >> (64 - STRING_TABLE_ARY)];
	}
    size_t n = strlen(string) + 1;
    char *duplicate = new(&table->begin, char, n);
    memcpy(duplicate, string, n);
    StringTableEntry *entry = new(&table->end, StringTableEntry, 1);
    entry->offset = duplicate - table->strings;
    *m = entry;
    return entry->offset;
}