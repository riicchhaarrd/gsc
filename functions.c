#include "vm.h"
#include <math.h>

static int setexpfog(VM *vm)
{
	int argc = vm_argc(vm);
	for(int i = 0; i < argc; ++i)
	{
		printf("%f\n", vm_checkfloat(vm, i));
	}
	return 0;
}
static int setcvar(VM *vm)
{
	const char *var = vm_checkstring(vm, 0);
	const char *value = vm_checkstring(vm, 1);
	printf("setcvar %s -> '%s'\n", var, value);
    return 0;
}

static void process_escape_sequences(const char *s)
{
	while(*s)
	{
		if(*s == '\\')
		{
			switch(*++s)
			{
				case 'n': putchar('\n'); break;
				case 't': putchar('\t'); break;
				case '\\': putchar('\\'); break;
				case '"': putchar('\"'); break;
				case '\'': putchar('\''); break;
				case 'r': putchar('\r'); break;
				case 'b': putchar('\b'); break;
				default:
					putchar('\\');
					putchar(*s);
					break;
			}
		}
		else
		{
			putchar(*s);
		}
		s++;
	}
}

static int dump(VM *vm)
{
	Variable *v = vm_argv(vm,0);
	if(v->type != VAR_OBJECT)
	{
		return 0;
	}
	Object *o = v->u.oval;
	char buf[256];
	printf("[object]\n");
	for(ObjectField *it = o->fields; it; it = it->next)
	{
		printf("\t'%s': %s\n", it->key, vm_stringify(vm, it->value, buf, sizeof(buf)));
	}
	return 0;
}

static int print(VM *vm)
{
	int argc = vm_argc(vm);
	// printf("[PRINT] ");
	char buf[1024];
	for(int i = 0; i < argc; ++i)
	{
		char *str = vm_stringify(vm, vm_argv(vm, i), buf, sizeof(buf));
		process_escape_sequences(str);
	}
	return 0;
}
static int println(VM *vm)
{
	int argc = vm_argc(vm);
	// printf("[PRINT] ");
	char buf[1024];
	for(int i = 0; i < argc; ++i)
	{
		char *str = vm_stringify(vm, vm_argv(vm, i), buf, sizeof(buf));
		process_escape_sequences(str);
	}
	putchar('\n');
	return 0;
}

// TODO: check bounds

static int substr(VM *vm)
{
	const char *str = vm_checkstring(vm, 0);
	int offset = vm_checkinteger(vm, 1);
	if(vm_argc(vm) > 2)
	{
		int n = vm_checkinteger(vm, 2);
    	vm_pushstring_n(vm, str + offset, n);
	} else
	{
		vm_pushstring(vm, str + offset);
	}
	return 1;
}

static int byteat(VM *vm)
{
	const char *str = vm_checkstring(vm, 0);
	int offset = vm_checkinteger(vm, 1);
	size_t n = strlen(str);
	if(n > 0)
		vm_pushinteger(vm, str[offset % n]);
	else
		vm_pushinteger(vm, 0);
	return 1;
}

static int float_(VM *vm)
{
	const char *str = vm_checkstring(vm, 0);
	vm_pushfloat(vm, atof(str));
	return 1;
}

static int int_(VM *vm)
{
	const char *str = vm_checkstring(vm, 0);
	vm_pushinteger(vm, atoi(str));
	return 1;
}

static int spawnstruct(VM *vm)
{
	Variable obj = vm_create_object(vm);
	vm_pushvar(vm, &obj);
	return 1;
}

static int spawn(VM *vm)
{
	Variable obj = vm_create_object(vm);
	vm_pushvar(vm, &obj);
	return 1;
}

static int typeof_(VM *vm)
{
	Variable *v = vm_argv(vm, 0);
	vm_pushstring(vm, variable_type_names[v->type]);
	return 1;
}

static int getcvar(VM *vm)
{
	const char *var = vm_checkstring(vm, 0);
	printf("getcvar %s\n", var);
    vm_pushstring(vm, "");
	return 1;
}

static int isdefined(VM *vm)
{
	Variable *var = vm_argv(vm, 0);
	vm_pushbool(vm, var && var->type != VAR_UNDEFINED);
	return 1;
}

static int endon(VM *vm, Object *self)
{
	const char *key = vm_checkstring(vm, 0);
	Thread *thr = vm_thread(vm);
	int idx = vm_string_index(vm, key);
	buf_push(thr->endon, idx);
	return 0;
}

static int waittill(VM *vm, Object *self)
{
	const char *key = vm_checkstring(vm, 0);
	Thread *thr = vm_thread(vm);
	thr->state = VM_THREAD_WAITING_EVENT;
	thr->waittill.arguments = NULL;
	thr->waittill.name = vm_string_index(vm, key);
	thr->waittill.object = self;
	if(thr->waittill.name == -1)
	{
		vm_error(vm, "Key '%s' not found", key);
	}
	// printf("[VM] TODO implement waittill: %s\n", key);
	return 0;
}

static int notify(VM *vm, Object *self)
{
	const char *key = vm_checkstring(vm, 0);
	Thread *thr = vm_thread(vm);
	// vm_notify(vm, vm_dup(vm, &thr->frame->self), key, vm_argc(vm));
	vm_notify(vm, self, key, 0);
	return 0;
}

static int dummy_0(VM *vm)
{
	return 0;
}
static int dummy_1(VM *vm)
{
	vm_pushundefined(vm);
	return 1;
}
static int dummy_ret_1(VM *vm)
{
	vm_pushinteger(vm, 1);
	return 1;
}

float dot(float *a, float *b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void normalize(float *v)
{
	float l = sqrtf(dot(v, v));
	for(int k = 0; k < 3; ++k)
		v[k] /= l;
}

#define M_PI (3.14159)
#define DEG2RAD ((float)M_PI / 180.f);

static float radians(float f)
{
	return f * DEG2RAD;
}

// TODO: test results

static int vectornormalize(VM *vm)
{
	float v[3];
	vm_checkvector(vm, 0, v);
	normalize(v);
	vm_pushvector(vm, v);
	return 1;
}
static int vectorscale(VM *vm)
{
	float v[3];
	vm_checkvector(vm, 0, v);
	float scale = vm_checkfloat(vm, 1);
	for(int i = 0; i < 3; ++i)
		v[i] *= scale;
	vm_pushvector(vm, v);
	return 1;
}

static int vectortoangles(VM *vm)
{
	float v[3];
	vm_checkvector(vm, 0, v);
	float pitch = asinf(-v[1]);
	float yaw = atan2f(v[0], v[2]);
	float angles[3];
	angles[1] = yaw;
	angles[0] = pitch;
	angles[2] = 0.f;
	vm_pushvector(vm, angles);
	return 1;
}

static int vec3_(VM *vm)
{
	float v[3] = { 0.f, 0.f, 0.f };
	size_t n = vm_argc(vm);
	switch(n)
	{
		case 0: break;
		case 2: vm_error(vm, "Please use either 0, 1 or 3 initializers"); break;
		case 1:
		{
			float f = vm_checkfloat(vm, 0);
			for(size_t i = 0; i < 3; ++i)
			{
				v[i] = f;
			}
		}
		break;
		// case 2:
		// {
		// 	v[0] = vm_checkfloat(vm, 0);
		// 	v[1] = vm_checkfloat(vm, 1);
		// }
		// break;
		default:
		{
			for(size_t i = 0; i < vm_argc(vm); ++i)
			{
				v[i] = vm_checkfloat(vm, i);
			}
		}
		break;
	}
	vm_pushvector(vm, v);
	return 1;
}

static int anglestoforward(VM *vm)
{
	float v[3];
	vm_checkvector(vm, 0, v);
	float yaw = v[1];
	float forward[3];
	forward[0] = cosf(radians(yaw));
	forward[1] = sinf(radians(yaw));
	forward[2] = 0.f;
	normalize(forward);
	vm_pushvector(vm, forward);
	return 1;
}

static int randomint(VM *vm)
{
	vm_pushinteger(vm, 4); 	// chosen by fair dice roll.
							// guaranteed to be random.
	return 1;
}

static int breakpoint(VM *vm)
{
	void vm_debugger(VM *vm);
	vm_debugger(vm);
	return 0;
}

static int getchar_(VM *vm)
{
	vm_pushinteger(vm, getchar());
	return 1;
}

void register_c_functions(VM *vm)
{
	vm_register_c_function(vm, "breakpoint", breakpoint);
	vm_register_c_function(vm, "getchar", getchar_);
	vm_register_c_function(vm, "print", print);
	vm_register_c_function(vm, "println", println);
	vm_register_c_function(vm, "setexpfog", setexpfog);
	vm_register_c_function(vm, "getcvar", getcvar);
	vm_register_c_function(vm, "isdefined", isdefined);
	vm_register_c_function(vm, "setcvar", setcvar);
	vm_register_c_function(vm, "setsavedcvar", setcvar);
	vm_register_c_function(vm, "typeof", typeof_);
	vm_register_c_function(vm, "dump", dump);
	vm_register_c_function(vm, "spawnstruct", spawnstruct);
	vm_register_c_function(vm, "getentarray", spawnstruct);
	vm_register_c_function(vm, "substr", substr);
	vm_register_c_function(vm, "byteat", byteat);
	vm_register_c_function(vm, "int", int_);
	vm_register_c_function(vm, "float", float_);
	vm_register_c_function(vm, "spawn", spawn);
	vm_register_c_method(vm, "endon", endon);
	vm_register_c_method(vm, "waittill", waittill);
	vm_register_c_method(vm, "notify", notify);

	vm_register_c_function(vm, "randomint", randomint);
	vm_register_c_function(vm, "vectornormalize", vectornormalize);
	vm_register_c_function(vm, "vectorscale", vectorscale);
	vm_register_c_function(vm, "vectortoangles", vectortoangles);
	vm_register_c_function(vm, "vec3", vec3_);
	vm_register_c_function(vm, "anglestoforward", anglestoforward);
	vm_register_c_function(vm, "assertex", dummy_0);
	vm_register_c_function(vm, "precachemodel", dummy_0);
	vm_register_c_function(vm, "precachevehicle", dummy_0);
	vm_register_c_function(vm, "precacheturret", dummy_0);
	vm_register_c_function(vm, "precacheshader", dummy_0);
	vm_register_c_function(vm, "loadfx", dummy_ret_1);
	vm_register_c_function(vm, "playloopedfx", dummy_ret_1);
}
