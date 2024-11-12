#pragma once
#include <stddef.h>
typedef struct
{
	void *(*malloc)(void *ctx, size_t size);
	void (*free)(void *ctx, void *ptr);
	void *ctx;
} Allocator;

#ifdef ALLOCATOR_MALLOC_WRAPPER
#include <malloc.h>
static void *allocator_malloc_(void *ctx, size_t size)
{
	return malloc(size);
}
static void allocator_free_(void *ctx, void *ptr)
{
	free(ptr);
}
static Allocator malloc_allocator = { allocator_malloc_, allocator_free_, 0 };
#endif
