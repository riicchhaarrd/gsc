#pragma once

#include <stdbool.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "allocator.h"

typedef struct
{
	char *beg;
	char *end;
	jmp_buf *jmp_oom;
} Arena;

static void *arena_allocate_memory_(Arena *a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count)
{
	ptrdiff_t padding = -(uintptr_t)a->beg & (align - 1);
	ptrdiff_t available = a->end - a->beg - padding;
	if(available < 0 || count > available / size)
	{
		if(a->jmp_oom)
			longjmp(*a->jmp_oom, 1);
		return NULL;
	}
	void *p = a->beg + padding;
	a->beg += padding + count * size;
	return memset(p, 0, count * size);
}
#define new(a, t, n) (t *)arena_allocate_memory_(a, sizeof(t), _Alignof(t), n)

static void arena_init(Arena *a, char *buffer, size_t size)
{
	a->beg = buffer;
	a->end = buffer + size;
	a->jmp_oom = NULL;
}

static float arena_available_mib(Arena *a)
{
	return (float)(a->end - a->beg) / 1024.f / 1024.f;
}

static Arena arena_split(Arena *base, int size)
{
	Arena a = {0};
	arena_init(&a, base->beg, size);
	base->beg += size;
	a.jmp_oom = base->jmp_oom;
	return a;
}

static void *arena_malloc_(void *ctx, size_t size)
{
	Arena *arena = (Arena*)ctx;
	return new(arena, char, size);
}

static void arena_free_(void *ctx, void *ptr)
{
}

static Allocator arena_allocator(Arena *arena)
{
	return (Allocator) { arena_malloc_, arena_free_, arena };
}

#define ARENA_ALLOCATOR(ARENA)               \
	(Allocator)                              \
	{                                        \
		arena_malloc_, arena_free_, &(ARENA) \
	}
