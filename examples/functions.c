#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsc.h>
#include <inttypes.h>
#include <stdio.h>
#include <ctype.h>

typedef float vec3[3];

static int spawnstruct(gsc_Context *ctx)
{
	int ent = gsc_add_tagged_object(ctx, "spawnstruct");
	return 1;
}
static int f_strtok(gsc_Context *ctx)
{
	const char *str = gsc_get_string(ctx, 0);
	char s[1024];
	snprintf(s, sizeof(s), "%s", str);
	// const char *s = gsc_get_string(ctx,0);
	const char *delim = gsc_get_string(ctx, 1);
	int array = gsc_add_tagged_object(ctx, "array");
	char *t = strtok(s, delim);
	if(!t)
	{
		gsc_add_string(ctx, str);
		gsc_object_set_field(ctx, array, "0");
		return 1;
	}
	int n = 0;
	while(t)
	{
		char field[32];
		snprintf(field, sizeof(field), "%d", n++);
		gsc_add_string(ctx, t);
		gsc_object_set_field(ctx, array, field);
		t = strtok(NULL, delim);
	}
	return 1;
}
static int issubstr(gsc_Context *ctx)
{
	const char *haystack = gsc_get_string(ctx, 0);
	const char *needle = gsc_get_string(ctx, 1);
	gsc_add_bool(ctx, strstr(haystack, needle) != NULL);
	return 1;
}

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <sys/time.h>
	#include <unistd.h>
#endif

uint64_t ticks64()
{
#if defined(_WIN32) || defined(_WIN64)
	return GetTickCount64();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
#endif
}

static int gettime(gsc_Context *ctx)
{
	gsc_add_int(ctx, ticks64());
	return 1;
}

static int proxy_(gsc_Context *ctx)
{
	int obj = gsc_get_object(ctx, 0);
	if(gsc_object_get_proxy(ctx, obj))
	{
		const char *tag = gsc_object_get_tag(ctx, gsc_top(ctx) - 1);
		gsc_pop(ctx, 1);
		gsc_add_string(ctx, tag);
		return 1;
	}
	gsc_add_string(ctx, "");
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

static int isdefined(gsc_Context *ctx)
{
	gsc_add_bool(ctx, gsc_get_type(ctx, 0) != GSC_TYPE_UNDEFINED);
	return 1;
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
static int println(gsc_Context *ctx)
{
	int argc = gsc_numargs(ctx);
	char buf[1024];
	// printf("[SCRIPT] ");
	for(int i = 0; i < argc; ++i)
	{
		const char *str = stringify(ctx, buf, sizeof(buf), i);
		process_escape_sequences(str, stdout);
	}
	putchar('\n');
	return 0;
}

static float dot(float *a, float *b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float length2(vec3 v)
{
	return sqrtf(dot(v, v));
}

static float length(vec3 v)
{
	return sqrtf(length2(v));
}

static void normalize(float *v)
{
	float l = sqrtf(dot(v, v));
	for(int k = 0; k < 3; ++k)
		v[k] /= l;
}

#define GSC_M_PI (3.14159)
#define DEG2RAD ((float)GSC_M_PI / 180.f);

static float radians(float f)
{
	return f * DEG2RAD;
}

static int vectordot(gsc_Context *ctx)
{
	vec3 a, b;
	gsc_get_vec3(ctx, 0, a);
	gsc_get_vec3(ctx, 0, b);
	gsc_add_float(ctx, dot(a, b) / (length(a) * length(b)));
	return 1;
}

static int vectornormalize(gsc_Context *ctx)
{
	float v[3];
	gsc_get_vec3(ctx, 0, v);
	normalize(v);
	gsc_add_vec3(ctx, v);
	return 1;
}
static int vectorscale(gsc_Context *ctx)
{
	float v[3];
	gsc_get_vec3(ctx, 0, v);
	float scale = gsc_get_float(ctx, 1);
	for(int i = 0; i < 3; ++i)
		v[i] *= scale;
	gsc_add_vec3(ctx, v);
	return 1;
}

static int vectortoangles(gsc_Context *ctx)
{
	float v[3];
	gsc_get_vec3(ctx, 0, v);
	float pitch = asinf(-v[1]);
	float yaw = atan2f(v[0], v[2]);
	float angles[3];
	angles[1] = yaw;
	angles[0] = pitch;
	angles[2] = 0.f;
	gsc_add_vec3(ctx, angles);
	return 1;
}

static int anglestoforward(gsc_Context *ctx)
{
	float v[3];
	gsc_get_vec3(ctx, 0, v);
	float yaw = v[1];
	float forward[3];
	forward[0] = cosf(radians(yaw));
	forward[1] = sinf(radians(yaw));
	forward[2] = 0.f;
	normalize(forward);
	gsc_add_vec3(ctx, forward);
	return 1;
}

static int tolower_(gsc_Context *ctx)
{
	const char *input = gsc_get_string(ctx, 0);
	size_t n = strlen(input);
	char *output = gsc_temp_alloc(ctx, n + 1);
	for(size_t i = 0; i < n; ++i)
	{
		output[i] = tolower(input[i]);
	}
	output[n] = 0;
	gsc_add_string(ctx, output);
	return 1;
}

static int toupper_(gsc_Context *ctx)
{
	const char *input = gsc_get_string(ctx, 0);
	size_t n = strlen(input);
	char *output = gsc_temp_alloc(ctx, n + 1);
	for(size_t i = 0; i < n; ++i)
	{
		output[i] = toupper(input[i]);
	}
	output[n] = 0;
	gsc_add_string(ctx, output);
	return 1;
}

static int typeof_(gsc_Context *ctx)
{
	int type = gsc_get_type(ctx, 0);
	gsc_add_string(ctx, gsc_type_names[type]);
	return 1;
}

// https://youtu.be/LWFzPP8ZbdU?t=970
static uint32_t xorshift1() // xorshift1
{
	static uint32_t state = 4;
	state ^= (state << 13);
	state ^= (state >> 17);
	state ^= (state << 5);
	return state;
}

static int int_(gsc_Context *ctx)
{
	int i = gsc_get_int(ctx, 0);
	gsc_add_int(ctx, i);
	return 1;
}

static int sin_(gsc_Context *ctx)
{
	float f = gsc_get_float(ctx, 0);
	gsc_add_float(ctx, sinf(f));
	return 1;
}

static int cos_(gsc_Context *ctx)
{
	float f = gsc_get_float(ctx, 0);
	gsc_add_float(ctx, cosf(f));
	return 1;
}

static int float_(gsc_Context *ctx)
{
	float f = gsc_get_float(ctx, 0);
	gsc_add_float(ctx, f);
	return 1;
}

static int randomint(gsc_Context *ctx)
{
	int max_ = gsc_get_int(ctx, 0);
	int r = xorshift1() % (max_ + 1);
	gsc_add_int(ctx, r);
	return 1;
}

static int randomfloat(gsc_Context *ctx)
{
	float max_ = gsc_get_float(ctx, 0);
	float r = ((float)xorshift1() / (float)UINT32_MAX) * max_;
	gsc_add_float(ctx, r);
	return 1;
}

static int distance_(gsc_Context *ctx)
{
	vec3 a, b;
	gsc_get_vec3(ctx, 0, a);
	gsc_get_vec3(ctx, 0, b);
	gsc_add_float(ctx, sqrtf(dot(a, b)));
	return 1;
}

static int randomintrange(gsc_Context *ctx)
{
	int min = gsc_get_int(ctx, 0);
	int max = gsc_get_int(ctx, 1);
	int64_t r = min + xorshift1() % (max - min);
	gsc_add_int(ctx, r);
	return 1;
}

static int randomfloatrange(gsc_Context *ctx)
{
	float min = gsc_get_float(ctx, 0);
	float max = gsc_get_float(ctx, 1);
	float r = min + ((float)xorshift1() / (float)UINT32_MAX) * (max - min);
	gsc_add_float(ctx, r);
	return 1;
}

static int assertex(gsc_Context *ctx)
{
	bool cond = gsc_get_bool(ctx, 0);
	const char *str = gsc_get_string(ctx, 1);
	// printf("assertEX(%d, %s)\n", cond, str);
	// getchar();
	if(!cond)
	{
		gsc_error(ctx, "Assertion failed: %s", str);
	}
	return 0;
}
static int f_assert(gsc_Context *ctx)
{
	bool cond = gsc_get_bool(ctx, 0);
	if(!cond)
	{
		if(gsc_numargs(ctx) > 1)
		{
			int argc = gsc_numargs(ctx);
			for(int i = 1; i < argc; ++i)
			{
				printf("%s\n", gsc_get_string(ctx, i));
			}
		}
		gsc_error(ctx, "Assertion failed!");
	}
	return 0;
}
static int f_getchar(gsc_Context *ctx)
{
	getchar();
	return 0;
}

static gsc_FunctionEntry functions[] = { { "getchar", f_getchar },
										 { "openfile", f_openfile },
										 { "closefile", f_closefile },
										 { "writefile", f_writefile },
										 { "issubstr", issubstr },
										 { "spawnstruct", spawnstruct },
										 { "gettime", gettime },
										 { "assertex", assertex },
										 { "assert", f_assert },
										 { "vectornormalize", vectornormalize },
										 { "vectordot", vectordot },
										 { "vectorscale", vectorscale },
										 { "vectortoangles", vectortoangles },
										 { "anglestoforward", anglestoforward },
										 { "toupper", toupper_ },
										 { "tolower", tolower_ },
										 { "strtok", f_strtok },
										 { "isdefined", isdefined },
										 { "println", println },
										 { "sin", sin_ },
										 { "cos", cos_ },
										 { "randomint", randomint },
										 { "int", int_ },
										 { "float", float_ },
										 { "randomfloat", randomfloat },
										 { "distance", distance_ },
										 { "randomfloatrange", randomfloatrange },
										 { "randomintrange", randomintrange },
										 { "typeof", typeof_ },
										 { "proxy", proxy_ },
										 { NULL, 0 } };

void register_script_functions(gsc_Context *ctx)
{
	for(int i = 0; functions[i].name; i++)
		gsc_register_function(ctx, NULL, functions[i].name, functions[i].function);
}
