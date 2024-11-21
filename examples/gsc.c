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

static int setmodel(gsc_Context *ctx)
{
	printf("setmodel(%s)\n", gsc_get_string(ctx, 0));
	return 0;
}

static int spawn(gsc_Context *ctx)
{
	int ent = gsc_add_object(ctx);
	int proxy = gsc_get_global(ctx, "#entity");
	gsc_object_set_proxy(ctx, ent, proxy);
	gsc_pop(ctx, 1);
	return 1;
}

static void process_escape_sequences(const char *s, FILE *fp)
{
	while(*s)
	{
		if(*s == '\\')
		{
			switch(*++s)
			{
				case 'n': fputc('\n', fp); break;
				case 't': fputc('\t', fp); break;
				case '\\': fputc('\\', fp); break;
				case '"': fputc('\"', fp); break;
				case '\'': fputc('\'', fp); break;
				case 'r': fputc('\r', fp); break;
				case 'b': fputc('\b', fp); break;
				default:
					fputc('\\', fp);
					fputc(*s, fp);
					break;
			}
		}
		else
		{
			fputc(*s, fp);
		}
		s++;
	}
}

static int f_openfile(gsc_Context *ctx)
{
	const char *filename = gsc_get_string(ctx, 0);
	const char *mode = gsc_get_string(ctx, 1);
	FILE *fp = fopen(filename, mode);
	if(fp)
	{
		int obj = gsc_add_tagged_object(ctx, "FILE*");
		gsc_object_set_userdata(ctx, obj, fp);
		return 1;
	}
	return 0;
}
static const char *stringify(gsc_Context *ctx, char *buf, size_t n, int index)
{
#define fixnan(x) (isnan(x) ? 0.f : (x))
	switch(gsc_get_type(ctx, index))
	{
		case GSC_TYPE_UNDEFINED: return "undefined";
		case GSC_TYPE_BOOLEAN: return gsc_get_bool(ctx, index) == 0 ? "0" : "1";
		case GSC_TYPE_FLOAT: snprintf(buf, n, "%.2f", fixnan(gsc_get_float(ctx, index))); return buf;
		case GSC_TYPE_INTEGER: snprintf(buf, n, "%" PRId64, gsc_get_int(ctx, index)); return buf;
		case GSC_TYPE_INTERNED_STRING:
		case GSC_TYPE_STRING: return gsc_get_string(ctx, index);
		case GSC_TYPE_OBJECT: snprintf(buf, n, "[object 0x%x]", gsc_get_ptr(ctx, index)); return buf;
		case GSC_TYPE_FUNCTION: return "[function]";
		case GSC_TYPE_VECTOR:
		{
			float vec[3];
			gsc_get_vec3(ctx, index, vec);
			snprintf(buf, n, "(%.2f, %.2f, %.2f)", fixnan(vec[0]), fixnan(vec[1]), fixnan(vec[2]));
			return buf;
		}
		break;
	}
	return NULL;
}
static int f_println(gsc_Context *ctx)
{
	int argc = gsc_numargs(ctx);
	char buf[1024];
	// printf("[SCRIPT] ");
	for(int i = 0; i < argc; ++i)
	{
		const char *str = stringify(ctx, buf, sizeof(buf), i);
		process_escape_sequences(str, stdout);
		putchar(' ');
	}
	putchar('\n');
	return 0;
}
static int f_writefile(gsc_Context *ctx)
{
	int obj = gsc_get_object(ctx, 0);
	FILE *fp = gsc_object_get_userdata(ctx, obj);
	for(int i = 1; i < gsc_numargs(ctx); i++)
	{
		const char *s = gsc_get_string(ctx, i);
		process_escape_sequences(s, fp);
		// fprintf(fp, "%s", s);
	}
	return 0;
}

static int f_closefile(gsc_Context *ctx)
{
	int obj = gsc_get_object(ctx, 0);
	FILE *fp = gsc_object_get_userdata(ctx, obj);
	fclose(fp);
	return 0;
}

static gsc_FunctionEntry functions[] = { { "spawn", spawn },
										 { "println", f_println },
										 { "openfile", f_openfile },
										 { "closefile", f_closefile },
										 { "writefile", f_writefile },
										 { NULL, 0 } };

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

		gsc_add_function(ctx, setmodel);
		gsc_object_set_field(ctx, methods, "setmodel");

		gsc_object_set_field(ctx, proxy, "__call");

		gsc_set_global(ctx, "entity");
	}
	for(int i = 0; functions[i].name; i++)
		gsc_register_function(ctx, NULL, functions[i].name, functions[i].function);
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
