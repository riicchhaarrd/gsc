// gcc gsc.c -o gsc -I../include -L../build -lgsc -lm
#include <stdio.h>
#include <stdlib.h>
#include <gsc.h>
#include <malloc.h>
#ifndef _WIN32
	#include <unistd.h>
#else
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
#endif
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h>

static char *read_text_file(const char *path)
{
	FILE *fp = fopen(path, "r");
	if(!fp)
		return NULL;
	long n = 0;
	fseek(fp, 0, SEEK_END);
	n = ftell(fp);
	char *data = calloc(1, n + 1);
	rewind(fp);
	fread(data, 1, n, fp);
	fclose(fp);
	return data;
}

char *heap;
void *allocate_memory(void *ctx, int size)
{
	char *current = heap;
	heap += size;
	return current;
}

void free_memory(void *ctx, void *ptr)
{
	// free(ptr);
}

const char *read_file(void *ctx, const char *filename, int *status)
{
	char temp[256];
	snprintf(temp, sizeof(temp), "%s.gsc", filename);
	char *data = read_text_file(temp);
	if(!data)
	{
		*status = GSC_NOT_FOUND;
		return NULL;
	}
	*status = GSC_OK;
	return data;
}

static int f_example_entity_method(gsc_Context *ctx)
{
	printf("example_entity_method(%s)\n", gsc_get_string(ctx, 0));
	return 0;
}

#include <signal.h>
static volatile bool interrupted = false;
static int signal_handler(int sig)
{
	if(sig == SIGINT)
	{
		interrupted = true;
	}
}

static const char *base_path = ".";
int main(int argc, char **argv)
{
	assert(argc > 1);
	const char *input_file = argv[1];
	char *mem = malloc(1024 * 1024 * 128);

	heap = mem;
	gsc_CreateOptions opts = { .allocate_memory = allocate_memory,
							   .free_memory = free_memory,
							   .read_file = read_file,
							   .userdata = NULL,
							   .main_memory_size = 128 * 1024 * 1024,
							   .string_table_memory_size = 16 * 1024 * 1024,
							   .temp_memory_size = 32 * 1024 * 1024,
							   .verbose = 0,
							   .max_threads = 1024,
							   .default_self = "level" };
	gsc_Context *ctx = gsc_create(opts);
	if(!ctx)
	{
		fprintf(stderr, "Failed to create context\n");
		exit(1);
	}
	{
		int level = gsc_add_tagged_object(ctx, "#level");
		gsc_set_global(ctx, "level");
		
		int proxy = gsc_add_tagged_object(ctx, "#entity");

		int methods = gsc_add_object(ctx);

		gsc_add_function(ctx, f_example_entity_method);
		gsc_object_set_field(ctx, methods, "example_entity_method");

		gsc_object_set_field(ctx, proxy, "__call");

		gsc_set_global(ctx, "entity");
	}

	void register_script_functions(gsc_Context *ctx);
	register_script_functions(ctx);
	
	int result = gsc_compile(ctx, input_file, 0);
	if(result == GSC_OK)
	{
		result = gsc_link(ctx);
		gsc_call(ctx, input_file, "main", 0);
		while(!interrupted && GSC_OK != gsc_update(ctx, 1.f / 20.f))
		{
			#ifdef _WIN32
				Sleep(20);
			#else
				usleep(20000);
			#endif
		}
		interrupted = false;
		gsc_destroy(ctx);
	} else
	{
		fprintf(stderr, "Failed to execute script '%s' (result: %d)\n", input_file, result);
		exit(1);
	}
	return 0;
}
