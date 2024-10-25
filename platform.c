#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

int execute_file(const char *base_path, const char *input_file, bool verbose);
int execute(const char *source, bool verbose);

bool frame(float dt);

#ifdef EMSCRIPTEN
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE void gsc_execute_file(const char *input_file)
{
	execute_file(input_file, true);
}

EMSCRIPTEN_KEEPALIVE void gsc_execute(const char *source)
{
	execute(source, false);
}

static double previous;
void em_frame()
{
	double current = emscripten_get_now();
	double dt = (current - previous) / 1000.0;
	previous = current;
	if(!frame(dt))
	{
		emscripten_cancel_main_loop();
	}
}
#endif

typedef struct
{
	char *input_file;
	bool verbose;
} Opts;

static Opts opts = { .verbose = false, .input_file = NULL };

static void parse_opts(int argc, char **argv)
{
	for(int i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-v"))
		{
			opts.verbose = true;
		}
		else // if(i == argc - 1)
		{
			opts.input_file = argv[i];
		}
	}
}
static const char *base_path = "scripts";

void find_gsc_files(const char *dir_path, int depth);

int main(int argc, char **argv)
{
	find_gsc_files(base_path, 0);
	parse_opts(argc, argv);
#ifndef EMSCRIPTEN
	int status = execute_file(base_path, opts.input_file, opts.verbose);
    while(status == 0)
    {
		float dt = 1.f / 20.f;
		if(!frame(dt))
			break;
		usleep(20000);
    }
    return status;
#else
	previous = emscripten_get_now();
	emscripten_set_main_loop(em_frame, 0, 1);
	return 0; // unreachable
#endif
}
