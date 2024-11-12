#pragma once
#include <stdint.h>

// https://stackoverflow.com/questions/28460987/c-get-type-alignment-portably
#if __STDC_VERSION__ >= 201112L
	#include <stdalign.h>
#else
	// If this is not portable then just default to 16
	#ifndef alignof
		#define alignof(type) \
			((size_t)         \
			 & ((struct {     \
				   char c;    \
				   type d;    \
			   } *)0)         \
				   ->d)
	#endif
#endif

#include "allocator.h"

typedef struct ObjectPool ObjectPool;

struct ObjectPool
{
	int struct_size;
	int size;
	int capacity;
	Allocator *allocator;
	void *free_list;
	void *initial_memory;
};

#ifndef align_up
	#define align_up(addr, align) (((addr) + ((align) - 1)) & ~((align) - 1))
#endif

static bool object_pool_init(ObjectPool *pool,
							 int struct_size,
							 size_t alignment,
							 int initial_size,
							 int capacity,
							 Allocator *allocator)
{
	pool->struct_size = struct_size;
	pool->size = initial_size;
	pool->capacity = capacity == -1 ? initial_size : capacity;
	pool->allocator = allocator;
	pool->free_list = NULL;
	pool->initial_memory = NULL;

	if(initial_size > 0)
	{
		size_t N = initial_size * struct_size + alignment - 1;
		pool->initial_memory = allocator->malloc(allocator->ctx, N);
		if(!pool->initial_memory)
			return false;

		char *ptr = (char *)align_up((uintptr_t)pool->initial_memory, alignment);

		pool->free_list = ptr;

		for(size_t i = 0; i < initial_size - 1; ++i)
		{
			*(void **)ptr = ptr + struct_size;
			ptr += struct_size;
		}
		*(void **)ptr = NULL;
	}
	return true;
}

static void object_pool_destroy(ObjectPool *pool)
{
	if(pool->initial_memory)
	{
		pool->allocator->free(pool->allocator->ctx, pool->initial_memory);
	}
}
#define object_pool_allocate(pool, type) object_pool_allocate_(pool, sizeof(type))
static void *object_pool_allocate_(ObjectPool *pool, size_t size)
{
	#if 1
	if(size > pool->struct_size)
	{
		printf("size %d > struct size %d", size, pool->struct_size);
		// getchar();
		abort();
	}
	if(!pool->free_list)
	{
		if(pool->capacity == 0 || pool->size < pool->capacity)
		{
			++pool->size;
			return pool->allocator->malloc(pool->allocator->ctx, pool->struct_size);
		}
		return NULL;
	}
	void *ptr = pool->free_list;
	pool->free_list = *(void **)ptr;
	return ptr;
	#else
	return pool->allocator->malloc(pool->allocator->ctx, pool->struct_size);
	#endif
}

static void object_pool_deallocate(ObjectPool *pool, void *ptr)
{
	*(void **)ptr = pool->free_list;
	pool->free_list = ptr;
}

#define DEFINE_OBJECT_POOL(NAME, TYPE)                                              \
	bool NAME##_init(ObjectPool *pool, int count, int cap, Allocator *allocator)    \
	{                                                                               \
		return object_pool_init(pool, sizeof(TYPE), alignof(TYPE), count, cap, allocator); \
	}
