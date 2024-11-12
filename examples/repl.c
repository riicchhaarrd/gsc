// gcc repl.c -o repl -I../include -L../build -lgsc -lm -lreadline
#include <stdio.h>
#include <gsc.h>
#include <malloc.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>

#include <readline/readline.h>
#include <readline/history.h>

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

static char file_input[0xffff];
static char line[0xffff];

const char *read_file(void *ctx, const char *filename, int *status)
{
	*status = GSC_OK;
	return file_input;
}

static int setmodel(gsc_Context *ctx)
{
	printf("setmodel(%s)\n", gsc_get_string(ctx, 0));
	return 0;
}

static int spawn(gsc_Context *ctx)
{
	int ent = gsc_add_object(ctx);
	int proxy = gsc_get_global(ctx, "entity");
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
static const char *mapname = "$repl";
int main(int argc, char **argv)
{
	char *mem = malloc(1024 * 1024 * 128);
	char *input;

	while(!interrupted)
	{
		input = readline("> ");

		// Check for end of input (Ctrl+D or EOF)
		if(!input)
		{
			printf("\nExiting...\n");
			break;
		}

		if(*input)
		{
			add_history(input);
		}

		snprintf(file_input, sizeof(file_input), "main()\n{\n%s\n}", input);

		heap = mem;
		gsc_CreateOptions opts = { .allocate_memory = allocate_memory,
								   .free_memory = free_memory,
								   .read_file = read_file,
								   .userdata = NULL,
								   .main_memory_size = 128 * 1024 * 1024,
								   .string_table_memory_size = 16 * 1024 * 1024,
								   .temp_memory_size = 32 * 1024 * 1024,
								   .verbose = 0 };
		gsc_Context *ctx = gsc_create(opts);
		{
			int proxy = gsc_add_tagged_object(ctx, "entity");

			int methods = gsc_add_object(ctx);

			gsc_add_function(ctx, setmodel);
			gsc_object_set_field(ctx, methods, "setmodel");

			gsc_object_set_field(ctx, proxy, "__call");

			gsc_set_global(ctx, "entity");
		}
		for(int i = 0; functions[i].name; i++)
			gsc_register_function(ctx, NULL, functions[i].name, functions[i].function);
		int result = gsc_compile(ctx, mapname, GSC_COMPILE_FLAG_PRINT_EXPRESSION);
		if(result == GSC_OK)
		{
			result = gsc_link(ctx);
			gsc_call(ctx, mapname, "main", 0);
			while(!interrupted && GSC_OK != gsc_update(ctx, 1.f / 20.f))
			{
				usleep(20000);
			}
			interrupted = false;
			gsc_destroy(ctx);
		}

		free(input);
	}
	return 0;
}
