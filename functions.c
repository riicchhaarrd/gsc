#include "vm.h"
#include <math.h>

static HashTrie fake_vars;

static Variable integer(VM *vm, int i)
{
	Variable v = { 0 };
	v.type = VAR_INTEGER;
	v.u.ival = i;
	return v;
}
static Variable vec3(VM *vm, float x, float y, float z)
{
	Variable v = { 0 };
	v.type = VAR_VECTOR;
	v.u.vval[0] = x;
	v.u.vval[1] = y;
	v.u.vval[2] = z;
	return v;
}

Object *create_fake_entity(VM *vm)
{
	Object *o = vm_create_object(vm).u.oval;
	Variable health = integer(vm, 0);
	Variable maxhealth = integer(vm, 100);
	vm_set_object_field(vm, o, "health", &health);
	vm_set_object_field(vm, o, "maxhealth", &maxhealth);
	Variable zero_vector = vec3(vm, 0.f, 0.f, 0.f);
	vm_set_object_field(vm, o, "origin", &zero_vector);
	vm_set_object_field(vm, o, "angles", &zero_vector);
	return o;
}

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
	HashTrieNode *n = hash_trie_upsert(&fake_vars, var, vm->allocator, false);
	if(n->value)
		free(n->value);
	n->value = strdup(value);
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
		const char *str = vm_stringify(vm, vm_argv(vm, i), buf, sizeof(buf));
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
		const char *str = vm_stringify(vm, vm_argv(vm, i), buf, sizeof(buf));
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

static int getnode(VM *vm)
{
	vm_pushundefined(vm);
	return 1;
}

static int getent(VM *vm)
{
	const char *value = vm_checkstring(vm, 0);
	const char *key = vm_checkstring(vm, 1);
	if(!strcmp(key, "classname"))
	{
		if(!strcmp(value, "player"))
		{
			Object *player = create_fake_entity(vm); // TODO: add player specific fields here
			vm_pushobject(vm, player);
			return 1;
		}
	}
	// TODO: load map and return entities if they exist otherwise return undefined
	Object *ent = create_fake_entity(vm);
	vm_pushobject(vm, ent);
	return 1;
}

static int spawn(VM *vm)
{
	Variable obj = vm_create_object(vm);
	vm_pushvar(vm, &obj);
	return 1;
}

#include <sys/time.h>
static int gettime(VM *vm)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long ms = (long long)(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
	vm_pushinteger(vm, ms);
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
	HashTrieNode *n = hash_trie_upsert(&fake_vars, var, NULL, false);
	// printf("getcvar %s\n", var);
	if(n)
	{
		vm_pushstring(vm, n->value);
		return 1;
	}
    vm_pushstring(vm, "");
	return 1;
}
static int getcvarint(VM *vm)
{
	const char *var = vm_checkstring(vm, 0);
	HashTrieNode *n = hash_trie_upsert(&fake_vars, var, NULL, false);
	// printf("getcvarint %s\n", var);
	if(n)
	{
		vm_pushinteger(vm, atoi(n->value));
		return 1;
	}
	vm_pushinteger(vm, 0);
	return 1;
}
static int getcvarfloat(VM *vm)
{
	const char *var = vm_checkstring(vm, 0);
	HashTrieNode *n = hash_trie_upsert(&fake_vars, var, NULL, false);
	// printf("getcvarfloat %s\n", var);
	if(n)
	{
		vm_pushfloat(vm, atof(n->value));
		return 1;
	}
	vm_pushfloat(vm, 0.f);
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

// TODO: wait for animation event / notetracks
// Hack: prefix with anim_ and notify when animation is done / encounters a notetrack

static int waittillmatch(VM *vm, Object *self)
{
	const char *key = vm_checkstring(vm, 0);
	char fake_key[256];
	snprintf(fake_key, sizeof(fake_key), "$nt_%s", key);
	Thread *thr = vm_thread(vm);
	thr->state = VM_THREAD_WAITING_EVENT;
	thr->waittill.arguments = NULL;
	thr->waittill.name = vm_string_index(vm, fake_key);
	thr->waittill.object = self;
	if(thr->waittill.name == -1)
	{
		vm_error(vm, "Key '%s' not found", fake_key);
	}
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

static int assert_(VM *vm)
{
	bool cond = vm_checkbool(vm, 0);
	if(!cond)
	{
		vm_error(vm, "Assert failed!");
	}
	return 0;
}

static int gettagorigin(VM *vm, Object *self)
{
	float v[3] = { 0.f, 0.f, 0.f };
	vm_pushvector(vm, v);
	return 1;
}

static int gettagangles(VM *vm, Object *self)
{
	float v[3] = { 0.f, 0.f, 0.f };
	vm_pushvector(vm, v);
	return 1;
}

static int objective_string(VM *vm)
{
	vm_pushstring(vm, "");
	return 1;
}

static int getweaponmodel(VM *vm)
{
	vm_pushstring(vm, "");
	return 1;
}

static int isalive(VM *vm)
{
	vm_pushbool(vm, false);
	return 1;
}

static int setmodel(VM *vm, Object *self)
{
	Variable str = { .type = VAR_STRING, .u.sval = (char*)vm_checkstring(vm, 0) };
	vm_set_object_field(vm, self, "model", &str);
	return 0;
}

static int giveweapon(VM *vm, Object *self)
{
	Variable str = { .type = VAR_STRING, .u.sval = (char*)vm_checkstring(vm, 0) };
	vm_set_object_field(vm, self, "weapon", &str);
	return 0;
}

static int dummy_method(VM *vm, Object *self)
{
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
static int dummy_ret_0(VM *vm)
{
	vm_pushinteger(vm, 0);
	return 1;
}
static int dummy_ret_1(VM *vm)
{
	vm_pushinteger(vm, 1);
	return 1;
}

static int issentient(VM *vm)
{
	vm_pushbool(vm, false);
	return 1;
}

static int getkeybinding(VM *vm)
{
	Variable obj = vm_create_object(vm);
	Variable tmp = { .type = VAR_INTEGER, .u.ival = 0 };
	vm_set_object_field(vm, obj.u.oval, "count", &tmp);
	vm_pushvar(vm, &obj);
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

// #define M_PI (3.14159)
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

static int tolower_(VM *vm)
{
	const char *input = vm_checkstring(vm, 0);
	size_t n = strlen(input);
	char *output = new(&vm->c_function_arena, char, n + 1);
	for(size_t i = 0; i < n; ++i)
	{
		output[i] = tolower(input[i]);
	}
	output[n] = 0;
	vm_pushstring(vm, output);
	return 1;
}

static int toupper_(VM *vm)
{
	const char *input = vm_checkstring(vm, 0);
	size_t n = strlen(input);
	char *output = new(&vm->c_function_arena, char, n + 1);
	for(size_t i = 0; i < n; ++i)
	{
		output[i] = toupper(input[i]);
	}
	output[n] = 0;
	vm_pushstring(vm, output);
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
	int max_ = vm_checkinteger(vm, 0);
	int r = vm_random(vm) % (max_ + 1);
	vm_pushinteger(vm, r);
	return 1;
}

static int randomfloat(VM *vm)
{
	float max_ = vm_checkfloat(vm, 0);
	float r = ((float)vm_random(vm) / (float)UINT32_MAX) * max_;
	vm_pushfloat(vm, r);
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

void register_dummy_c_functions(VM *vm)
{
	hash_trie_init(&fake_vars);

	vm_register_c_function(vm, "breakpoint", breakpoint);
	vm_register_c_function(vm, "getchar", getchar_);
	vm_register_c_function(vm, "print", print);
	vm_register_c_function(vm, "println", println);
	vm_register_c_function(vm, "setexpfog", setexpfog);
	vm_register_c_function(vm, "getcvar", getcvar);
	vm_register_c_function(vm, "getcvarint", getcvarint);
	vm_register_c_function(vm, "getcvarfloat", getcvarfloat);
	vm_register_c_function(vm, "isdefined", isdefined);
	vm_register_c_function(vm, "setcvar", setcvar);
	vm_register_c_function(vm, "setsavedcvar", setcvar);
	vm_register_c_function(vm, "typeof", typeof_);
	vm_register_c_function(vm, "dump", dump);
	vm_register_c_function(vm, "spawnstruct", spawnstruct);
	vm_register_c_function(vm, "getentarray", spawnstruct);
	vm_register_c_function(vm, "getaiarray", spawnstruct);
	vm_register_c_function(vm, "getspawnerarray", spawnstruct);
	vm_register_c_function(vm, "getallvehiclenodes", spawnstruct);
	vm_register_c_function(vm, "getvehiclenodearray", spawnstruct);
	vm_register_c_function(vm, "getnodearray", spawnstruct);
	vm_register_c_function(vm, "getspawnerteamarray", spawnstruct);
	vm_register_c_function(vm, "getallnodes", spawnstruct);
	vm_register_c_function(vm, "newhudelem", spawnstruct);
	vm_register_c_function(vm, "getnode", getnode);
	vm_register_c_function(vm, "getent", getent);
	vm_register_c_function(vm, "substr", substr);
	vm_register_c_function(vm, "byteat", byteat);
	vm_register_c_function(vm, "int", int_);
	vm_register_c_function(vm, "float", float_);
	vm_register_c_function(vm, "spawn", spawn);
	vm_register_c_function(vm, "gettime", gettime);

	vm_register_c_function(vm, "randomint", randomint);
	vm_register_c_function(vm, "vectornormalize", vectornormalize);
	vm_register_c_function(vm, "vectorscale", vectorscale);
	vm_register_c_function(vm, "vectortoangles", vectortoangles);
	vm_register_c_function(vm, "vec3", vec3_);
	vm_register_c_function(vm, "anglestoforward", anglestoforward);
	vm_register_c_function(vm, "tolower", tolower_);
	vm_register_c_function(vm, "toupper", toupper_);
	vm_register_c_function(vm, "assertex", dummy_0);
	vm_register_c_function(vm, "precachemodel", dummy_0);
	vm_register_c_function(vm, "assert", assert_);
	vm_register_c_function(vm, "precacheshellshock", dummy_0);
	vm_register_c_function(vm, "precacherumble", dummy_0);
	vm_register_c_function(vm, "precachestring", dummy_0);
	vm_register_c_function(vm, "precacheitem", dummy_0);
	vm_register_c_function(vm, "precachevehicle", dummy_0);
	vm_register_c_function(vm, "precacheturret", dummy_0);
	vm_register_c_function(vm, "precachemenu", dummy_0);
	vm_register_c_function(vm, "precacheshader", dummy_0);
	// TODO: FIXME this shouldn't be a builtin function I think, there's probably some bug
	// in the compiler/"linker" or a standard included file isn't set, I don't know
	vm_register_c_function(vm, "donotetracks", dummy_0);
	vm_register_c_function(vm, "prof_begin", dummy_0);
	vm_register_c_function(vm, "prof_end", dummy_0);
	vm_register_c_function(vm, "playfx", spawnstruct); // should return a FX entity
	vm_register_c_function(vm, "playfxontag", spawnstruct); // should return a FX entity
	vm_register_c_function(vm, "objective_add", spawnstruct);
	vm_register_c_function(vm, "objective_current", spawnstruct);
	vm_register_c_function(vm, "spawnvehicle", getent);
	vm_register_c_function(vm, "ambientplay", dummy_0);
	vm_register_c_function(vm, "getvehiclenode", dummy_0);
	vm_register_c_function(vm, "earthquake", dummy_0);
	vm_register_c_function(vm, "issentient", issentient);
	vm_register_c_function(vm, "musicplay", dummy_0);
	vm_register_c_function(vm, "loadfx", dummy_ret_1);
	vm_register_c_function(vm, "playloopedfx", dummy_ret_1);
	vm_register_c_function(vm, "getdifficulty", dummy_ret_0);
	vm_register_c_function(vm, "getkeybinding", getkeybinding);
	vm_register_c_function(vm, "getweaponmodel", getweaponmodel);
	vm_register_c_function(vm, "objective_string", objective_string);
	vm_register_c_function(vm, "isalive", isalive);
	vm_register_c_function(vm, "randomfloat", randomfloat);

	vm_register_c_method(vm, "endon", endon);
	vm_register_c_method(vm, "waittill", waittill);
	vm_register_c_method(vm, "waittillmatch", waittillmatch);
	vm_register_c_method(vm, "notify", notify);
	vm_register_c_method(vm, "giveweapon", giveweapon);
	vm_register_c_method(vm, "switchtoweapon", dummy_method);
	vm_register_c_method(vm, "switchtooffhand", dummy_method);
	vm_register_c_method(vm, "setnormalhealth", dummy_method);
	vm_register_c_method(vm, "takeallweapons", dummy_method);
	vm_register_c_method(vm, "setweaponclipammo", dummy_method);
	vm_register_c_method(vm, "pushplayer", dummy_method);
	vm_register_c_method(vm, "setviewmodel", dummy_method);
	vm_register_c_method(vm, "rotateyaw", dummy_method);
	vm_register_c_method(vm, "rotatepitch", dummy_method);
	vm_register_c_method(vm, "rotateroll", dummy_method);
	vm_register_c_method(vm, "hide", dummy_method);
	vm_register_c_method(vm, "show", dummy_method);
	vm_register_c_method(vm, "setcandamage", dummy_method);
	vm_register_c_method(vm, "notsolid", dummy_method);
	vm_register_c_method(vm, "solid", dummy_method);
	vm_register_c_method(vm, "animscripted", dummy_method);
	vm_register_c_method(vm, "useanimtree", dummy_method);
	vm_register_c_method(vm, "setflaggedanim", dummy_method);
	vm_register_c_method(vm, "setmodel", setmodel);
	vm_register_c_method(vm, "setorigin", dummy_method);
	vm_register_c_method(vm, "setplayerangles", dummy_method);
	vm_register_c_method(vm, "disconnectpaths", dummy_method);
	vm_register_c_method(vm, "gettagorigin", gettagorigin);
	vm_register_c_method(vm, "gettagangles", gettagangles);
	vm_register_c_method(vm, "linkto", dummy_method);
	vm_register_c_method(vm, "playsound", dummy_method);
	vm_register_c_method(vm, "setturretignoregoals", dummy_method);
	vm_register_c_method(vm, "playloopsound", dummy_method);
	vm_register_c_method(vm, "sethintstring", dummy_method);
	vm_register_c_method(vm, "playerlinktodelta", dummy_method);
	vm_register_c_method(vm, "allowleanleft", dummy_method);
	vm_register_c_method(vm, "allowleanright", dummy_method);
	vm_register_c_method(vm, "allowprone", dummy_method);
	vm_register_c_method(vm, "setshadowhint", dummy_method);
	vm_register_c_method(vm, "setturrettargetent", dummy_method);
	vm_register_c_method(vm, "attachpath", dummy_method);
	vm_register_c_method(vm, "startpath", dummy_method);
	vm_register_c_method(vm, "setspeed", dummy_method);
	vm_register_c_method(vm, "setwaitnode", dummy_method);

	// hud element methods
	vm_register_c_method(vm, "setshader", dummy_method);
	vm_register_c_method(vm, "fadeovertime", dummy_method);
}
