#include "vm.h"
#include <math.h>
#include "lexer.h"
#include "ast.h"
#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include "util.h"

#ifndef MAX
	#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif
// enum
// {
// 	UNION_OBJECT_SIZE =
// 		MAX(1, MAX(1, MAX(sizeof_ObjectField, MAX(sizeof_Variable, sizeof_Object))))
// 	// sizeof_Unit =
// 	// 	MAX(sizeof_Thread, MAX(sizeof_StackFrame, MAX(sizeof_ObjectField, MAX(sizeof_Variable, sizeof_Object))))
// };
typedef struct
{
	// char data[UNION_OBJECT_SIZE];
	char data[72]; // 64 so we can allocate small strings too
	// increased to 72 for Object
} UnionObject;

DEFINE_OBJECT_POOL(thread, Thread)
DEFINE_OBJECT_POOL(stack_frame, StackFrame)
DEFINE_OBJECT_POOL(uo, UnionObject)
// DEFINE_OBJECT_POOL(object_field, ObjectField)
// DEFINE_OBJECT_POOL(variable, Variable)
// DEFINE_OBJECT_POOL(object, Object)

static void info(VM *vm, const char *fmt, ...)
{
	if(!(vm->flags & VM_FLAG_VERBOSE))
		return;
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[INFO] %s\n", message);
}

static Variable undef = { .type = VAR_UNDEFINED };

static Object *object_for_var(Variable *v)
{
	return v->u.oval;
}

StackFrame *stack_frame(VM *vm, Thread *t)
{
	if(t->bp < 0)
		vm_error(vm, "No stack frame for thread");
	return &t->frames[t->bp];
}

static void free_object(VM *vm, Object *o)
{
	for(ObjectField *it = o->fields; it;)
	{
		ObjectField *field = it;
		it = it->next;
		object_pool_deallocate(&vm->pool.uo, field);
	}
	o->tail = NULL;
	o->refcount = 0;
	o->field_count = 0;
	o->fields = NULL;
}

static void strtolower(char *str)
{
	for(char *p = str; *p; ++p)
		*p = tolower(*p);
}

static const char *string(VM *vm, size_t idx)
{
	return string_table_get(vm->strings, idx);
}

int vm_string_index(VM *vm, const char *s)
{
	return string_table_intern(vm->strings, s);
}

static bool variable_is_string(Variable *v)
{
	return v->type == VAR_STRING || v->type == VAR_INTERNED_STRING;// || v->type == VAR_LOCALIZED_STRING;
}

static const char *variable_string(VM *vm, Variable *v)
{
	switch(v->type)
	{
		default: vm_error(vm, "Not a string");
		case VAR_STRING: return (const char *)v->u.sval.data;
		case VAR_INTERNED_STRING: return string(vm, v->u.ival);
	}
	return NULL;
}

Variable vm_intern_string_variable(VM *vm, const char *str)
{
	Variable v;
	v.type = VAR_INTERNED_STRING;
	v.u.ival = vm_string_index(vm, str);
	return v;
}

VariableString allocate_variable_string(VM *vm, int len) // len is including \0
{
	// if(len == -1)
	// 	len = strlen(str) + 1;
	char *ptr = NULL;
	if(len <= 64)
	{
		ptr = (char *)object_pool_allocate(&vm->pool.uo, char);
		if(!ptr)
			vm_error(vm, "No strings left");
	}
	else
	{
		// vm_error(vm, "No malloc!");
		ptr = (char *)malloc(len); // TODO: FIXME
	}
	// memcpy(ptr, str, len);
	return (VariableString) { .data = ptr, .length = len };
}

static void print_callstack(Thread *thr)
{
	printf("____________________________________________\n");
	if(thr->caller.file && thr->caller.function)
	{
		printf("\t%s::%s\n", thr->caller.file, thr->caller.function);
	}
	else
	{
		printf("\t(internal)\n");
	}
	if(thr->bp < 0)
	{
		printf("\tNo callstack.\n");
	}
	else
	{
		for(int i = 0; i < thr->bp + 1; i++)
		{
			StackFrame *prev_sf = &thr->frames[i];
			printf("\t-> %s::%s\n", prev_sf->file, prev_sf->function);
		}
	}
	printf("____________________________________________\n");
}

static void print_stackframe(Thread *thr)
{

    StackFrame *sf = NULL;
	if(thr->bp >= 0)
	{
		sf = &thr->frames[thr->bp];
	}
	// printf("========= STACK =========\n");
	if(sf)
	{
		printf("\t== ip: %d\n", sf->ip);
	}
	printf("\t== sp: %d\n", thr->sp);
	printf("\t== bp: %d\n", thr->bp);
	for(int i = thr->sp - 1; i >= 0; i--)
    {
		Variable *sv = &thr->stack[i];
		if(!sv)
			continue;
		printf("\t== %d: type:%s\n", i, variable_type_names[sv->type]);
	}
}

int thread_count(VM *vm)
{
	if(vm->thread_write_idx >= vm->thread_read_idx)
	{
		return vm->thread_write_idx - vm->thread_read_idx;
	}
	return vm->max_threads - (vm->thread_read_idx - vm->thread_write_idx);
}

#pragma pack(push, 8)
typedef struct
{
	int index;
	int state;
	StackFrame *frames;
	int bp;
} ThreadDebugInfo_;
#pragma pack(pop)

int get_thread_info_(VM *vm, ThreadDebugInfo_ *info, int i)
{
	if(vm->thread_read_idx == vm->thread_write_idx)
		return 0; // No threads
	Thread *t = vm->thread_buffer[(vm->thread_read_idx + i) % vm->max_threads];
	info->frames = t->frames;
	info->bp = t->bp;
	info->index = i;
	info->state = t->state;
	return 1;
}

void vm_print_thread_info(VM *vm)
{
	if(vm->thread_read_idx == vm->thread_write_idx)
		return; // No threads
	printf("[THREADS]\n");
	size_t n = thread_count(vm);
	printf("%d %s\n", n, n > 1 ? "threads" : "thread");
	printf("=========================================\n");
	for(size_t i = 0; i < n; i++)
	{
		Thread *t = vm->thread_buffer[(vm->thread_read_idx + i) % vm->max_threads];
    	StackFrame *sf = stack_frame(vm, t);
		printf("%d: %s %s::%s", i, vm_thread_state_names[t->state], sf->file, sf->function);
		if(t->state == VM_THREAD_WAITING_EVENT)
		{
			printf(" (event=%s)", string(vm, t->waittill.name));
		}
		printf("\n");
		print_stackframe(t);
		print_callstack(t);
		// printf("\t- %d: %s\n", i, vm_thread_state_names[t->state]);
	}
}

static void print_variable(VM *vm, const char *key, Variable *value, int indent)
{
	char buf[256];
	for(int i = 0; i < indent; ++i)
		putchar('\t');
	printf("'%s': %s\n", key, vm_stringify(vm, value, buf, sizeof(buf)));
}

static void print_object(VM *vm, const char *key, Variable *v, int indent)
{
	Object *o = object_for_var(v);
	for(ObjectField *it = o->fields; it; it = it->next)
	{
		print_variable(vm, it->key, it->value, indent);
		if(it->value->type == VAR_OBJECT)
			print_object(vm, it->key, it->value, indent + 1);
	}
}

// static void print_globals(VM *vm)
// {
// 	for(int i = 0; i < VAR_GLOB_MAX; ++i)
// 	{
// 		printf("global %s: %s\n", variable_globals[i], variable_type_names[vm->globals[i].type]);
// 		print_object(vm, variable_globals[i], &vm->globals[i], 1);
// 	}
// }

static void vm_stacktrace(VM *vm)
{
	// TODO: print instructions

    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
    // printf("========= STACK =========\n");
	printf("\t== sp: %d\n", thr->sp);
	printf("\t== ip: %d\n", sf->ip);
	printf("\t== bp: %d\n", thr->bp);
	for(int i = thr->sp - 1; i >= 0; i--)
    {
		Variable *sv = &thr->stack[i];
		if(!sv)
			continue;
		printf("\t== %d: type:%s\n", i, variable_type_names[sv->type]);
	}
	print_callstack(thr);
    // printf("====== END OF STACK =====\n");
}

#ifndef COUNT_OF
	#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

void vm_error(VM *vm, const char *fmt, ...)
{
    Thread *thr = vm->thread;
    StackFrame *sf = NULL;
	if(thr->bp >= 0)
		sf = &thr->frames[thr->bp];
	Instruction *current = sf && sf->instructions ? &sf->instructions[sf->ip > 0 ? sf->ip - 1 : 0] : NULL;
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[VM] ERROR: %s on line %d (%s::%s)\n",
		   message,
		   current ? current->line : -1,
		   sf && sf->file ? sf->file : "?",
		   sf && sf->function ? sf->function : "?");
	vm_stacktrace(vm);
	// print_globals(vm);
	// vm_print_thread_info(vm);
	abort();
	if(vm->jmp)
		longjmp(*vm->jmp, 1);
	else
		abort();
}

static Variable *local(VM *vm, size_t index)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
	if(index >= sf->local_count)
	{
		vm_error(vm, "Invalid local index %d/%d", (int)index, (int)sf->local_count);
		return NULL;
	}
	return sf->locals[index];
}

static void print_locals(VM *vm)
{
    Thread *thr = vm->thread;
	StackFrame *sf = stack_frame(vm, thr);
	printf(" || Local variables ||\n");
	char str[1024];
	for(size_t i = 1; i < sf->local_count; ++i)
	{
		Variable *lv = local(vm, i);
		if(lv->type == VAR_UNDEFINED)
			continue;
		printf("\t|| local %d (%s): %s\n", i, variable_type_names[lv->type], vm_stringify(vm, lv, str, sizeof(str)));
		if(lv->type == VAR_OBJECT)
		{
			Object *o = object_for_var(lv);
			printf("%d fields\n", o->field_count);
			for(ObjectField *it = o->fields; it; it = it->next)
			{
				char buf[32];
				printf("\t\t'%s': %s\n", it->key, vm_stringify(vm, it->value, buf, sizeof(buf)));
			}
		}
	}
}

void vm_debugger(VM *vm)
{
	printf("========= BREAKPOINT =========\n");
	// TODO
	vm_stacktrace(vm);
	// print_globals(vm);
	print_locals(vm);
	vm_print_thread_info(vm);
	printf("========= BREAKPOINT =========\n");
#ifdef _WIN32
	__debugbreak();
#else
	raise(SIGTRAP);
#endif
}

static void incref(VM *vm, Variable *v)
{
	if(!v)
		return;
	if(v->type != VAR_OBJECT)
		return;
	Object *o = object_for_var(v);
	if(o->refcount == VM_REFCOUNT_NO_FREE)
		return;
	++o->refcount;
}

static void decref(VM *vm, Variable *v)
{
	if(!v)
		return;
	if(v->type != VAR_OBJECT)
		return;
	Object *o = object_for_var(v);
	if(o->refcount == VM_REFCOUNT_NO_FREE)
		return;
	--o->refcount;
}

static void free_var(VM *vm, Variable *v)
{
	switch(v->type)
	{
		case VAR_STRING:
		{
			if(v->u.sval.length <= 64)
			{
				object_pool_deallocate(&vm->pool.uo, v->u.sval.data);
			} else
			{
				vm_error(vm, "No free!");
				free(v->u.sval.data);
			}
			v->u.sval.length = 0;
		}
		break;

		case VAR_OBJECT:
		{
			Object *o = object_for_var(v);
			for(ObjectField *it = o->fields; it; it = it->next)
			{
				Variable *value = it->value;
				free_var(vm, value);
			}
			free_object(vm, o);
		}
		break;
	}
}

// static Variable *variable(VM *vm)
// {
// 	Variable *var = alloc_var(vm);
// 	var->type = VAR_UNDEFINED;
// 	var->refcount = 0;
// 	var->u.ival = 0;
//     return var;
// }

static Variable var(VM *vm)
{
	return (Variable) { .type = VAR_UNDEFINED };
}

static Variable ref(VM *vm, Variable *referee)
{
	Variable v = var(vm);
	v.type = VAR_REFERENCE;
	v.u.refval = referee;
	return v;
}

Object *vm_allocate_object(VM *vm)
{
	Object *o = object_pool_allocate(&vm->pool.uo, Object);
	if(!o)
		vm_error(vm, "No objects left");
	o->fields = NULL;
	o->tail = &o->fields;
	o->refcount = 0;
	o->field_count = 0;
	o->proxy = NULL;
	o->debug_info = vm->debug_info;
	return o;
}

Variable vm_create_object(VM *vm)
{
	Variable v = { .type = VAR_OBJECT };
	v.u.oval = vm_allocate_object(vm);
	return v;
}

// static Variable *dup(VM *vm, Variable *v)
// {
// 	Variable *copy = variable(vm);
// 	memcpy(copy, v, sizeof(Variable));
// 	copy->refcount = 0;
// 	return copy;
// }

// Variable* vm_dup(VM *vm, Variable* v)
// {
// 	return dup(vm, v);
// }

static void push_thread(VM *vm, Thread *thr, Variable v)
{
    StackFrame *sf = stack_frame(vm, thr);
	if(thr->sp < 0)
		vm_error(vm, "stack ptr < 0");
	if(thr->sp >= COUNT_OF(thr->stack))
		vm_error(vm, "stack ptr > max");
    thr->stack[thr->sp++] = v;
}

static void push(VM *vm, Variable v)
{
    Thread *thr = vm->thread;
	push_thread(vm, thr, v);
}

int vm_sp(VM *vm)
{
	return vm->thread->sp;
}

static Variable pop_thread(VM *vm, Thread *thr)
{
	if(thr->sp <= 0)
		vm_error(vm, "stack ptr < 0");
	if(thr->sp > COUNT_OF(thr->stack))
		vm_error(vm, "stack ptr > max");
    Variable *top = &thr->stack[--thr->sp];
    // Variable ret = *top;
    // decref(vm, &top);
    return *top;
}

static Variable pop(VM *vm)
{
    Thread *thr = vm->thread;
	return pop_thread(vm, thr);
}

Variable vm_pop(VM *vm)
{
	return pop(vm);
}

static Variable *pop_ref(VM *vm)
{
	Variable value = pop(vm);
	if(value.type != VAR_REFERENCE)
	{
		vm_error(vm, "'%s' is not a variable reference", variable_type_names[value.type]);
	}
	return value.u.refval;
}

// static Variable *top(VM *vm, int offset)
// {
//     Thread *thr = vm->thread;
//     StackFrame *sf = stack_frame(vm, thr);
//     Variable *v = &thr->stack[thr->sp - 1 - offset];
// 	return v;
// }

static void pop_string(VM *vm, char *str, size_t n)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
    Variable *top = &thr->stack[--thr->sp];
	switch(top->type)
	{
		// TODO: directly use a hash and memcmp for integer/other type of keys

		case VAR_BOOLEAN:
		case VAR_INTEGER: snprintf(str, n, "%" PRId64, top->u.ival); break;
		case VAR_FLOAT: snprintf(str, n, "%f", top->u.fval); break;
		case VAR_INTERNED_STRING:
		case VAR_STRING: snprintf(str, n, "%s", variable_string(vm, top)); break;
		default: vm_error(vm, "'%s' is not a string", variable_type_names[top->type]); break;
	}
	decref(vm, top);
}

static int64_t pop_int(VM *vm)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
    Variable *top = &thr->stack[--thr->sp];
    if(top->type != VAR_INTEGER && top->type != VAR_BOOLEAN)
		vm_error(vm, "'%s' is not a integer", variable_type_names[top->type]);
    int64_t i = top->u.ival;
    decref(vm, top);
    return i;
}

static void validate_operand_type(VM *vm, Operand *op, OperandType expected_type, const char *type_name)
{
    if(op->type != expected_type)
        vm_error(vm, "Operand '%s' is not a %s", operand_type_names[op->type], type_name);
}

static int64_t read_int(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    validate_operand_type(vm, op, OPERAND_TYPE_INT, "integer");
    return op->value.integer;
}

static int read_string_index(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    validate_operand_type(vm, op, OPERAND_TYPE_INDEXED_STRING, "string");
    return op->value.string_index;
}

static const char* read_string(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    validate_operand_type(vm, op, OPERAND_TYPE_INDEXED_STRING, "string");
    return string_table_get(vm->strings, op->value.string_index);
}

static float read_float(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    validate_operand_type(vm, op, OPERAND_TYPE_FLOAT, "float");
    return op->value.number;
}

static bool check_operand(Instruction *ins, size_t idx, OperandType type)
{
    return ins->operands[idx].type == type;
}

static void print_operand(VM *vm, Operand *operand, FILE *fp)
{
	if(operand->type == OPERAND_TYPE_NONE)
	{
		return;
	}
	switch(operand->type)
	{
		case OPERAND_TYPE_INDEXED_STRING: fprintf(fp, "%s ", string(vm, operand->value.string_index)); break;
		case OPERAND_TYPE_INT: fprintf(fp, "%d ", operand->value.integer); break;
		case OPERAND_TYPE_FLOAT: fprintf(fp, "%f ", operand->value.number); break;
	}
	// fprintf(fp, "%s ", operand_type_names[instr->operands[i].type]);
}

static void print_instruction(VM *vm, Instruction *instr, FILE *fp)
{
	fprintf(fp, "%s ", opcode_names[instr->opcode]);
	switch(instr->opcode)
	{
		case OP_PUSH:
		{
			static const char *_[] = {

				"STRING", "INTEGER", "BOOLEAN", "FLOAT", "FUNCTION", "LOCALIZED_STRING", "UNDEFINED", NULL
			};
			fprintf(fp, "%s ", _[instr->operands[0].value.integer]);
			if(instr->operands[0].value.integer == AST_LITERAL_TYPE_STRING)
			{
				fprintf(fp, "%s", string(vm, instr->operands[1].value.string_index));
			}
			else
			{
				print_operand(vm, &instr->operands[1], fp);
			}
		}
		break;
		default:
		{
			for(size_t i = 0; i < MAX_OPERANDS; ++i)
			{
				print_operand(vm, &instr->operands[i], fp);
			}
		}
		break;
	}
	fprintf(fp, "\n");
}

static VariableType promote_type(VariableType lhs, VariableType rhs)
{
	if(lhs == VAR_UNDEFINED || rhs == VAR_UNDEFINED)
	{
		return VAR_UNDEFINED;
	}
	if(lhs == VAR_STRING || rhs == VAR_STRING)
	{
		return VAR_STRING;
	}
	if(lhs == VAR_INTERNED_STRING || rhs == VAR_INTERNED_STRING)
	{
		return VAR_STRING;
	}
	if(lhs == VAR_VECTOR || rhs == VAR_VECTOR)
	{
		return VAR_VECTOR;
	}
	if(lhs == VAR_FLOAT || rhs == VAR_FLOAT)
	{
		return VAR_FLOAT;
	}
	if(lhs == VAR_INTEGER || rhs == VAR_INTEGER)
	{
		return VAR_INTEGER;
	}
	if(lhs == VAR_BOOLEAN || rhs == VAR_BOOLEAN)
	{
		return VAR_INTEGER;
	}
	return lhs;
}

const char *vm_stringify(VM *vm, Variable *v, char *buf, size_t n)
{
	#define fixnan(x) (isnan(x) ? 0.f : (x))
	switch(v->type)
	{
		case VAR_UNDEFINED: return "undefined";
		case VAR_BOOLEAN: return v->u.ival == 0 ? "0" : "1";
		// case VAR_BOOLEAN: return v->u.ival == 0 ? "false" : "true";
		case VAR_FLOAT: snprintf(buf, n, "%.2f", fixnan(v->u.fval)); return buf;
		case VAR_INTEGER: snprintf(buf, n, "%" PRId64, v->u.ival); return buf;
		// case VAR_ANIMATION:
		case VAR_INTERNED_STRING:
		case VAR_STRING: return variable_string(vm, v);
		case VAR_OBJECT: snprintf(buf, n, "[object 0x%x]", v->u.oval); return buf;
		case VAR_FUNCTION: return "[function]";
		case VAR_VECTOR: snprintf(buf, n, "(%.2f, %.2f, %.2f)", fixnan(v->u.vval[0]), fixnan(v->u.vval[1]), fixnan(v->u.vval[2])); return buf;
	}
	return NULL;
}

static Variable coerce_type(VM *vm, Variable *v, VariableType target_type)
{
	Variable result = { .type = target_type };
	
	switch(target_type) {
		case VAR_INTEGER:
			switch(v->type) {
				case VAR_BOOLEAN:
				case VAR_INTEGER: result = *v; break;
				case VAR_FLOAT: result.u.ival = (int64_t)v->u.fval; break;
				case VAR_INTERNED_STRING:
				case VAR_STRING: result.u.ival = strtoll(variable_string(vm, v), NULL, 10); break;
				default: vm_error(vm, "Cannot coerce '%s' to integer", variable_type_names[v->type]); break;
			}
			break;
			
		case VAR_FLOAT:
			switch(v->type) {
				case VAR_INTEGER: result.u.fval = (float)v->u.ival; break;
				case VAR_FLOAT: result = *v; break;
				case VAR_INTERNED_STRING:
				case VAR_STRING: result.u.fval = atof(variable_string(vm, v)); break;
				default: vm_error(vm, "Cannot coerce '%s' to float", variable_type_names[v->type]); break;
			}
			break;
			
		case VAR_VECTOR:
			switch(v->type) {
				case VAR_INTEGER: for(int i = 0; i < 3; ++i) result.u.vval[i] = (float)v->u.ival; break;
				case VAR_FLOAT: for(int i = 0; i < 3; ++i) result.u.vval[i] = v->u.fval; break;
				case VAR_VECTOR: return *v;
				default: vm_error(vm, "Cannot coerce '%s' to vector", variable_type_names[v->type]); break;
			}
			break;
			
		default:
			vm_error(vm, "Unsupported coercion target type");
			break;
	}
	return result;
}

#define coerce_int(vm, v) coerce_type(vm, v, VAR_INTEGER)
#define coerce_float(vm, v) coerce_type(vm, v, VAR_FLOAT)
#define coerce_vector(vm, v) coerce_type(vm, v, VAR_VECTOR)

static Variable unary(VM *vm, Variable *arg, int op)
{
	bool err = false;
	char temp[64];
	Variable result = { .type = arg->type };
	switch(arg->type)
	{
		case VAR_UNDEFINED:
		{
			if(op == '!')
			{
				result.type = VAR_BOOLEAN;
				result.u.ival = 1;
			}
			else
				err = true;
		}
		break;

		case VAR_BOOLEAN:
		{
			if(op == '!')
			{
				result.u.ival = !arg->u.ival;
			}
			else
				err = true;
		}
		break;

		case VAR_INTEGER:
		{
			int64_t a = coerce_int(vm, arg).u.ival;
			int64_t *b = &result.u.ival;
			switch(op)
			{
				case '!': *b = !a; break; // if(!array1.size)
				case '~': *b = ~a; break;
				case '-': *b = -a; break;
				case '+': *b = +a; break;
				default: err = true; break;
			}
		}
		break;

		case VAR_FLOAT:
		{
			float a = coerce_float(vm, arg).u.fval;
			float *b = &result.u.fval;
			switch(op)
			{
				case '-': *b = -a; break;
				case '+': *b = +a; break;
				default: err = true; break;
			}
		}
		break;

		default: err = true; break;
	}
	if(err)
	{
		vm_error(vm,
				 "Unsupported operator '%s' for type '%s'",
				 token_type_to_string(op, temp, sizeof(temp)),
				 variable_type_names[arg->type]);
	}
	return result;
}

// #define VM_ERROR_ON_DIVIDE_BY_ZERO

static Variable binop(VM *vm, Variable *lhs, Variable *rhs, int op)
{
	char temp[64];
	VariableType type = promote_type(lhs->type, rhs->type);
	Variable result = { .type = type };
	switch(type)
	{
		default:
		{
			switch(op)
			{
				case TK_EQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = lhs->type == rhs->type && !memcmp(&lhs->u, &rhs->u, sizeof(lhs->u));
					break;
				case TK_NEQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = !(lhs->type == rhs->type && !memcmp(&lhs->u, &rhs->u, sizeof(lhs->u)));
					break;
				default:
				{
					vm_error(vm,
							 "Unsupported operator '%s' for type '%s'",
							 token_type_to_string(op, temp, sizeof(temp)),
							 variable_type_names[type]);
				}
				break;
			}
		}
		break;
		case VAR_UNDEFINED:
		{
			switch(op)
			{
				case TK_EQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = lhs->type == VAR_UNDEFINED && rhs->type == VAR_UNDEFINED;
					break;
				case TK_NEQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = lhs->type != VAR_UNDEFINED || rhs->type != VAR_UNDEFINED;
					break;
				default:
				{
					vm_error(vm,
							 "Unsupported operator '%s' for type '%s'",
							 token_type_to_string(op, temp, sizeof(temp)),
							 variable_type_names[type]);
				}
				break;
			}
		}
		break;
		case VAR_INTEGER:
		{
			int64_t a = coerce_int(vm, lhs).u.ival;
			int64_t b = coerce_int(vm, rhs).u.ival;
			int64_t *c = &result.u.ival;
			switch(op)
			{
				case TK_PLUS_ASSIGN:
				case '+': *c = a + b; break;
				case TK_DIV_ASSIGN:
				case '/':
					if(b == 0)
					{
						#ifdef VM_ERROR_ON_DIVIDE_BY_ZERO
							vm_error(vm, "Divide by zero");
						#else
							*c = 0;
						#endif
					} else
					{
						*c = a / b;
					}
					break;
				case TK_MUL_ASSIGN:
				case '*': *c = a * b; break;
				case TK_MINUS_ASSIGN:
				case '-': *c = a - b; break;
				case TK_OR_ASSIGN:
				case '|': *c = a | b; break;
				case TK_AND_ASSIGN:
				case '&': *c = a & b; break;
				case TK_XOR_ASSIGN:
				case '^': *c = a ^ b; break;
				case TK_GEQUAL:
					*c = a >= b;
					result.type = VAR_BOOLEAN;
					break;
				case TK_LEQUAL:
					*c = a <= b;
					result.type = VAR_BOOLEAN;
					break;
				case TK_NEQUAL:
					*c = a != b;
					result.type = VAR_BOOLEAN;
					break;
				case TK_EQUAL:
					*c = a == b;
					result.type = VAR_BOOLEAN;
					break;
				case '>':
					*c = a > b;
					result.type = VAR_BOOLEAN;
					break;
				case '<':
					*c = a < b;
					result.type = VAR_BOOLEAN;
					break;
				case TK_LOGICAL_AND:
					*c = a && b;
					result.type = VAR_BOOLEAN;
					break;
				case TK_LOGICAL_OR:
					*c = a || b;
					result.type = VAR_BOOLEAN;
					break;
				case TK_MOD_ASSIGN:
				case '%': *c = a % b; break;
				case TK_LSHIFT_ASSIGN:
				case TK_LSHIFT: *c = a << b; break;
				case TK_RSHIFT_ASSIGN:
				case TK_RSHIFT: *c = a >> b; break;
				default:
				{
					vm_error(vm, "Unsupported operator '%s' for type '%s'", token_type_to_string(op, temp, sizeof(temp)), variable_type_names[type]);
				}
				break;
			}
		}
		break;
		case VAR_FLOAT:
		{
			float a = coerce_float(vm, lhs).u.fval;
			float b = coerce_float(vm, rhs).u.fval;
			float *c = &result.u.fval;
			switch(op)
			{
				case TK_PLUS_ASSIGN:
				case '+': *c = a + b; break;
				case TK_DIV_ASSIGN:
				case '/':
					if(b == 0.0)
					{
						#ifdef VM_ERROR_ON_DIVIDE_BY_ZERO
							vm_error(vm, "Divide by zero");
						#else
							*c = 0.f;
						#endif
					}
					else
					{
						*c = a / b;
					}
					break;
				case TK_MUL_ASSIGN:
				case '*': *c = a * b; break;
				case TK_MINUS_ASSIGN:
				case '-': *c = a - b; break;
				case TK_GEQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = a >= b;
					break;
				case TK_NEQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = a != b;
					break;
				case TK_EQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = a == b;
					break;
				case TK_LEQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = a <= b;
					break;
				case '>':
					result.type = VAR_BOOLEAN;
					result.u.ival = a > b;
					break;
				case '<':
					result.type = VAR_BOOLEAN;
					result.u.ival = a < b;
					break;
				case TK_MOD_ASSIGN:
				case '%': *c = fmod(a, b); break;
				default:
				{
					vm_error(vm, "Unsupported operator '%s' for type '%s'", token_type_to_string(op, temp, sizeof(temp)), variable_type_names[type]);
				}
				break;
			}
		}
		break;
		case VAR_VECTOR:
		{

			Variable vec_a, vec_b;
			vec_a = coerce_vector(vm, lhs);
			vec_b = coerce_vector(vm, rhs);
			float *a = vec_a.u.vval;
			float *b = vec_b.u.vval;
			float *c = result.u.vval;
			switch(op)
			{
				case TK_EQUAL:
				case TK_NEQUAL:
				{
					bool eq = true;
					for(int i = 0; i < 3; ++i)
					{
						if(fabs(b[i] - a[i]) > 0.001f) // replace with epsilon
						{
							eq = false;
							break;
						}
					}
					result.type = VAR_BOOLEAN;
					result.u.ival = op == TK_EQUAL ? eq : !eq;
				}
				break;
				case TK_PLUS_ASSIGN:
				case '+':
					for(int i = 0; i < 3; ++i)
						c[i] = a[i] + b[i];
					break;
				case TK_DIV_ASSIGN:
				case '/':
					for(int i = 0; i < 3; ++i)
						c[i] = a[i] / b[i];
					break;
				case TK_MUL_ASSIGN:
				case '*':
					for(int i = 0; i < 3; ++i)
						c[i] = a[i] * b[i];
					break;
				case TK_MINUS_ASSIGN:
				case '-':
					for(int i = 0; i < 3; ++i)
						c[i] = a[i] - b[i];
					break;
				default:
				{
					vm_error(vm,
							 "Unsupported operator '%s' for type '%s'",
							 token_type_to_string(op, temp, sizeof(temp)),
							 variable_type_names[type]);
				}
				break;
			}
		}
		break;
		case VAR_INTERNED_STRING:
		case VAR_STRING:
		{
			// TODO: FIXME
			static char a_buf[4096];
			static char b_buf[4096];
			const char *a = vm_stringify(vm, lhs, a_buf, sizeof(a_buf));
			const char *b = vm_stringify(vm, rhs, b_buf, sizeof(b_buf));
			switch(op)
			{
				case TK_PLUS_ASSIGN:
				case '+':
				{
					size_t n = strlen(a) + strlen(b) + 1;
					VariableString vs = allocate_variable_string(vm, n);
					// char *str = malloc(n);
					snprintf(vs.data, n, "%s%s", a, b);
					result.type = VAR_STRING;
					result.u.sval = vs;
				}
				break;
				case TK_EQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = 0 == strcmp(a, b);
					break;
				case TK_NEQUAL:
					result.type = VAR_BOOLEAN;
					result.u.ival = 0 != strcmp(a, b);
					break;
				default:
				{
					vm_error(vm, "Unsupported operator '%s' for type '%s'", token_type_to_string(op, temp, sizeof(temp)), variable_type_names[type]);
				}
				break;
			}
		}
		break;
	}
	return result;
}

static Variable integer(VM *vm, int64_t i)
{
	Variable v = var(vm);
	v.type = VAR_INTEGER;
	v.u.ival = i;
	return v;
}

void add_thread(VM *vm, Thread *t)
{
	if((vm->thread_write_idx + 1) % vm->max_threads == vm->thread_read_idx)
	{
		vm_error(vm, "Maximum amount of threads reached");
	}
	vm->thread_buffer[vm->thread_write_idx] = t;
	vm->thread_write_idx = (vm->thread_write_idx + 1) % vm->max_threads;
}

Thread *remove_thread(VM *vm)
{
	if(vm->thread_read_idx == vm->thread_write_idx)
		return NULL;
	Thread *t = vm->thread_buffer[vm->thread_read_idx];
	vm->thread_read_idx = (vm->thread_read_idx + 1) % vm->max_threads;
	return t;
}

gsc_Function object_get_function(VM *vm, Object *object, const char *function)
{
	ObjectField *entry = vm_object_upsert(NULL, object, function);
	if(!entry)
		return NULL;
	Variable *val = entry->value;
	if(val->type != VAR_FUNCTION)
		vm_error(vm, "'%s' is not a function", function);
	return val->u.funval.native_function;
}

gsc_Function object_find_callable(VM *vm, Object *object, const char *callable, const char *function)
{
	Object *proxy = object->proxy;
	while(proxy)
	{
		ObjectField *entry = vm_object_upsert(NULL, proxy, callable);
		if(entry)
		{
			Variable *call = entry->value;
			if(call->type != VAR_OBJECT)
				vm_error(vm, "%s is not an object", callable);
			gsc_Function f = object_get_function(vm, call->u.oval, function);
			if(f)
				return f;
		}
		proxy = proxy->proxy;
	}
	return NULL;
}

static void op_load_field_object_(VM *vm, Variable obj, const char *prop)
{
	if(obj.type == VAR_UNDEFINED)
	{
		push(vm, undef);
	}
	else if(obj.type != VAR_OBJECT)
	{
		vm_error(vm, "'%s' is not an object", variable_type_names[obj.type]);
	}
	else
	{
		Object *o = object_for_var(&obj);
		if(!o)
		{
			vm_error(vm, "object is null");
		}
		if(!strcmp(prop, "size"))
		{
			push(vm, integer(vm, o->field_count));
			// Variable *v = variable(vm);
			// v->type = VAR_INTEGER;
			// v->u.ival = o->fields.length;
			// push(vm, v);
		}
		else
		{
			bool handled = false;
			if(o->proxy)
			{
				gsc_Function func = object_find_callable(vm, o, "__get", prop);
				if(func)
				{
					push(vm, obj);
					push(vm, integer(vm, 0));
					vm->fsp = vm->thread->sp;
					if(func(vm->ctx) <= 0)
					{
						vm_pushundefined(vm);
						// vm_error(vm, "'%s' must return value", prop);
					}
					Variable result = pop(vm);
					pop(vm);
					pop(vm);
					push(vm, result);
					handled = true;
				}
			}
			if(!handled)
			{
				ObjectField *entry = vm_object_upsert(NULL, o, prop);
				if(!entry)
				{
					push(vm, undef);
				}
				else
				{
					push(vm, *entry->value);
				}
			}
		}
	}
}

static bool call_function(VM *vm, Thread*, const char *file, const char *function, int function_string_index, size_t nargs, bool, int);
#define ASSERT_STACK(X)                                                              \
	do                                                                               \
	{                                                                                \
		if(thr->sp != sp + (X))                                                      \
			vm_error(vm, "Stack cookie failed for '%s'! Expected %d, got %d", opcode_names[ins->opcode], sp + (X), thr->sp); \
	} while(0)

// To "computed goto" or not to "computed goto" portability wise...
// GCC feature "labels as values"
bool vm_execute_instruction(VM *vm, Instruction *ins)
{
    Thread *thr = vm->thread;
	StackFrame *sf = stack_frame(vm, thr);
	vm->debug_info.line = ins->line;
	vm->debug_info.file = sf->file;
	if(vm->flags & VM_FLAG_VERBOSE)
	{
    	print_instruction(vm, ins, stdout);
	}
	int sp = thr->sp;
    switch(ins->opcode)
    {
		case OP_POP:
		{
            Variable v = pop(vm);
            decref(vm, &v);
			ASSERT_STACK(-1);
		}
		break;

		case OP_PRINT_EXPR:
		{
			char buf[1024];
            Variable v = pop(vm);
			const char *str = vm_stringify(vm, &v, buf, sizeof(buf));
			process_escape_sequences(str, stdout);
			putchar('\n');
            decref(vm, &v);
			ASSERT_STACK(-1);
		}
		break;

		case OP_JMP:
		{
            int rel = read_int(vm, ins, 0);
			sf->ip += rel;
			ASSERT_STACK(0);
		}
		break;

		case OP_UNDEF:
		{
			push(vm, undef);
		}
		break;

		case OP_JZ:
		{
            int rel = read_int(vm, ins, 0);
			if(thr->result == 0)
			{
				sf->ip += rel;
			}
			ASSERT_STACK(0);
		}
		break;
		
		// case OP_SELF:
		// {
		// 	bool as_ref = read_int(vm, ins, 0) > 0;
		// 	if(as_ref)
		// 		push(vm, ref(vm, &sf->self));
		// 	else
		// 		push(vm, sf->self);
		// 	ASSERT_STACK(1);
		// }
		// break;
		
		case OP_GLOBAL:
		{
			Variable *glob = &vm->global_object;
			if(glob->type != VAR_OBJECT)
				vm_error(vm, "Error! Corrupted global object");
			bool as_ref = read_int(vm, ins, 0) > 0;
			if(as_ref)
				push(vm, ref(vm, glob));
			else
				push(vm, *glob);
		}
		break;

		// case OP_GLOB:
		// {
		// 	int idx = read_int(vm, ins, 0);
		// 	bool as_ref = read_int(vm, ins, 1) > 0;
		// 	if(idx < 0 || idx >= VAR_GLOB_MAX)
		// 		vm_error(vm, "Invalid index for global");
		// 	Variable *glob = &vm->globals[idx];

		// 	if(as_ref)
		// 		push(vm, ref(vm, glob));
		// 	else
		// 		push(vm, *glob);
		// 	ASSERT_STACK(1);
		// }
		// break;

		case OP_FIELD_REF:
		{
			Variable *obj = pop_ref(vm);
			char prop[256] = { 0 };
			pop_string(vm, prop, sizeof(prop));
			if(obj->type != VAR_OBJECT)
			{
				if(obj->type == VAR_UNDEFINED) // Coerce to object... Just make this a new object
				{
					gsc_add_tagged_object(vm->ctx, "UNDEFINED coerced to OBJECT");
					*obj = pop(vm);
					// *obj = vm_create_object(vm);
				}
				else
				{
					vm_error(vm, "'%s' is not an object", variable_type_names[obj->type]);
				}
			}
			Object *o = object_for_var(obj);
			if(!o)
			{
				vm_error(vm, "object is null");
			}

			bool handled = false;
			if(o->proxy)
			{
				gsc_Function func = object_find_callable(vm, o, "__set", prop);
				if(func)
				{
					push(vm, *obj);
					Variable v = var(vm);
					v.type = VAR_FUNCTION;
					v.u.funval.native_function = func;
					push(vm, v);
					handled = true;
				}
			}
			if(!handled)
			{
				int idx = vm_string_index(vm, prop);
				ObjectField *entry = vm_object_upsert(vm, o, string(vm, idx)); // We're using the StringTable unique char* pointer to pass to upsert, prop wouldn't work
				push(vm, ref(vm, entry->value));
			}
			// ASSERT_STACK(-1);
		}
		break;

		case OP_LOAD_FIELD:
		{
			Variable obj = pop(vm);
			if(obj.type == VAR_VECTOR)
			{
				int idx = pop_int(vm);
				if(idx < 0 || idx > 2)
					vm_error(vm, "Index %d out of bounds for vector", idx);
				vm_pushfloat(vm, obj.u.vval[idx]);
			} else if(obj.type == VAR_STRING)
			{
				Variable key = pop(vm);
				const char *str = variable_string(vm, &obj);
				size_t n = strlen(str);
				if( variable_is_string(&key))
				{
					const char *keystr = variable_string(vm, &key);
					if(!strcmp(keystr, "length") || !strcmp(keystr, "size")) // TODO: optimize?
					{
						vm_pushinteger(vm, n);
					} else
					{
						vm_error(vm, "'%s' is not an object", variable_type_names[obj.type]);
					}
				} else if(key.type == VAR_INTEGER)
				{
					size_t idx = key.u.ival;
					// size_t idx = (size_t)pop_int(vm);
					if(idx > n)
						vm_error(vm, "%d out bounds for string '%s' (length %d)", idx, str, n);
					vm_pushstring_n(vm, str + idx, 1);
				} else
				{
					vm_error(vm, "Unsupported key type '%s' for string", variable_type_names[key.type]);
				}
			}
			else
			{
				char prop[256] = { 0 };
				pop_string(vm, prop, sizeof(prop));
				op_load_field_object_(vm, obj, prop);
			}
			ASSERT_STACK(-1);
		}
		break;

		case OP_STORE:
		{
			if(gsc_type(vm->ctx, -1) == VAR_FUNCTION)
			{
				Variable dst = pop(vm);

				// Variable obj = pop(vm);
				// Variable src = pop(vm);
				// push(vm, obj);
				push(vm, integer(vm, 1));
				vm->fsp = vm->thread->sp;
				if(dst.u.funval.native_function(vm->ctx) != 0)
					vm_error(vm, "Must not return value");
				pop(vm); //nargs
				pop(vm); //obj
				// src

			} else
			{
				Variable *dst = pop_ref(vm);
				Variable src = pop(vm);
				incref(vm, &src);
				// TODO: move
				// if(dst->type == VAR_OBJECT)
				// {
				// 	free(dst->u.oval);
				// }
				// else if(dst->type == VAR_STRING)
				// {
				// 	free(dst->u.sval);
				// }
				dst->type = src.type;
				memcpy(&dst->u, &src.u, sizeof(dst->u));
				// decref(vm, &dst);
				push(vm, *dst);
				ASSERT_STACK(-1);
			}

			if(vm->flags & VM_FLAG_VERBOSE)
				print_locals(vm);
		}
		break;

		case OP_LOAD:
		{
			// TODO: check out of bounds
			// allocate dynamically
			int slot = read_int(vm, ins, 0);
			Variable *lv = local(vm, slot);
			// lv->refcount = 0xdeadbeef;
			push(vm, *lv);
			ASSERT_STACK(1);
		}
		break;

		case OP_CONST_0:
		case OP_CONST_1:
		{
			push(vm, integer(vm, ins->opcode == OP_CONST_1 ? 1 : 0));
			ASSERT_STACK(1);
		}
		break;

		case OP_REF:
		{
			// TODO: check out of bounds
			// allocate dynamically
			int slot = read_int(vm, ins, 0);
			Variable *lv = local(vm, slot);
			// lv->refcount = 0xdeadbeef;
			push(vm, ref(vm, lv));
			// push(vm, lv);
			ASSERT_STACK(1);
		}
		break;

		case OP_JNZ:
		{
            int rel = read_int(vm, ins, 0);
			if(thr->result != 0)
			{
				sf->ip += rel;
			}
			ASSERT_STACK(0);
		}
		break;

		case OP_TEST:
		{
			thr->result = pop_int(vm);
		}
		break;

		case OP_PUSH:
		{
            int var_type = read_int(vm, ins, 0);
            Variable v = var(vm);
			switch(var_type)
			{
				case AST_LITERAL_TYPE_FUNCTION:
				{
					v.type = VAR_FUNCTION;
					v.u.funval.function = read_string_index(vm, ins, 1);
					if(check_operand(ins, 2, OPERAND_TYPE_INDEXED_STRING))
						v.u.funval.file = read_string_index(vm, ins, 2);
					else
						v.u.funval.file = -1;
				}
				break;
				case AST_LITERAL_TYPE_FLOAT:
				{
					v.type = VAR_FLOAT;
					v.u.fval = read_float(vm, ins, 1);
				}
				break;
				// case AST_LITERAL_TYPE_ANIMATION:
				// {
				// 	v.type = VAR_ANIMATION;
				// 	v.u.sval = string(vm, read_string_index(vm, ins, 1));
				// }
				// break;
                case AST_LITERAL_TYPE_STRING:
				{
					v.type = VAR_INTERNED_STRING;
					v.u.ival = read_string_index(vm, ins, 1);
				}
				break;
                case AST_LITERAL_TYPE_LOCALIZED_STRING:
				{
					v.type = VAR_INTERNED_STRING;
					v.u.ival = read_string_index(vm, ins, 1);
				}
				break;
				case AST_LITERAL_TYPE_BOOLEAN:
				{
					v.type = VAR_BOOLEAN;
					v.u.ival = read_int(vm, ins, 1);
				}
				break;
				case AST_LITERAL_TYPE_INTEGER:
				{
					v.type = VAR_INTEGER;
					v.u.ival = read_int(vm, ins, 1);
				}
				break;
				case AST_LITERAL_TYPE_UNDEFINED:
				{
					v.type = VAR_UNDEFINED;
				}
				break;
				default:
				{
                    vm_error(vm, "Unhandled var type %d", var_type);
				}
				break;
			}
            push(vm, v);
			ASSERT_STACK(1);
		}
		break;
		
		case OP_WAIT:
		{
			Variable v = pop(vm);
			if(v.type != VAR_UNDEFINED)
			{
				Variable duration = coerce_float(vm, &v);
				thr->wait = duration.u.fval;
				thr->state = VM_THREAD_WAITING_TIME;
			}
			else
			{
				thr->state = VM_THREAD_WAITING_FRAME;
			}
		}
		break;

		case OP_UNARY:
		{
			int op = read_int(vm, ins, 0);
			Variable arg = pop(vm);
			Variable result = unary(vm, &arg, op);
			push(vm, result);
			ASSERT_STACK(0);
		}
		break;

		case OP_TABLE:
		{
			gsc_add_tagged_object(vm->ctx, "OP_TABLE");
			// push(vm, vm_create_object(vm));
		}
		break;

		case OP_RET:
		{
			if(thr->bp < 0)
				vm_error(vm, "bp < 0");

			// buf_free(sf->locals);
			for(int i = 0; i < sf->local_count; i++)
			{
				object_pool_deallocate(&vm->pool.uo, sf->locals[i]);
			}
			if(--thr->bp < 0)
			{
				thr->state = VM_THREAD_INACTIVE;
				if(thr->return_value) // TODO: FIXME
				{
					*thr->return_value = pop(vm);
				}
				else
				{
					pop(vm); // retval
				}
				return false;
			}
		}
		break;

		case OP_VECTOR:
		{
			int nelements = read_int(vm, ins, 0);
			if(nelements != 3)
				vm_error(vm, "Vector must have 3 components");
			Variable v = var(vm);
			v.type = VAR_VECTOR;
			for(int k = 0; k < nelements; ++k)
			{
				Variable el = pop(vm);
				float f = coerce_float(vm, &el).u.fval;
				v.u.vval[k] = f;
			}
			push(vm, v);
		}
		break;

		case OP_CALL_PTR:
		case OP_CALL:
		{
			int function = -1;
			const char *file = NULL;
			if(ins->opcode == OP_CALL_PTR)
			{
				Variable func = pop(vm);
				if(func.type != VAR_FUNCTION)
				{
					vm_error(vm, "'%s' is not a function pointer", variable_type_names[func.type]);
				}
				function = func.u.funval.function;
				if(func.u.funval.file != -1)
					file = string(vm, func.u.funval.file);
			} else
			{
				function = read_string_index(vm, ins, 0);
				if(check_operand(ins, 1, OPERAND_TYPE_INDEXED_STRING))
					file = read_string(vm, ins, 1);
			}
			int call_flags = read_int(vm, ins, 3);
			// Object *object = NULL;
			if(call_flags & VM_CALL_FLAG_METHOD)
			{
				// Variable ov = pop(vm);
				// if(ov.type != VAR_OBJECT)
				// 	vm_error(vm, "'%s' is not an object", variable_type_names[ov.type]);
				// object = object_for_var(&ov);
				// object = pop_ref(vm);
			}
            int nargs = vm_cast_int(vm, vm_stack_top(vm, -1));
			if(!file)
				file = sf->file;
			// info(vm, "CALLING -> %s::%s %d\n", file, function, nargs);
			const char *function_name = string(vm, function);
			vm->debug_info.function = function_name;

			if(call_flags & VM_CALL_FLAG_THREADED)
			{
				Thread *nt = object_pool_allocate(&vm->pool.threads, Thread);
				if(!nt)
					vm_error(vm, "No threads left");
				memset(nt, 0, sizeof(Thread));
				nt->bp = 0;
				nt->state = VM_THREAD_ACTIVE;
				pop_thread(vm, thr); //nargs
				
				for(size_t k = 0; k < nargs + 1; ++k)
				{
					Variable arg = pop_thread(vm, thr);
					push_thread(vm, nt, arg);
				}
				push_thread(vm, thr, undef); // return value for caller thread
				nt->return_value = &thr->stack[thr->sp - 1]; // TODO: FIXME
				push_thread(vm, nt, integer(vm, nargs));
				call_function(vm, nt, file, function_name, function, nargs, true, call_flags);
				nt->caller.file = sf->file;
				nt->caller.function = sf->function;
				add_thread(vm, nt);
				// vm->thread = nt;
				// return false;
			}
			else
			{
				if(++thr->bp >= VM_FRAME_SIZE)
					vm_error(vm, "thr->bp >= VM_FRAME_SIZE");
				if(!call_function(vm, thr, file, function_name, function, nargs, false, call_flags))
					thr->bp--;
			}
			// ASSERT_STACK(-nargs);
		}
		break;

		case OP_BINOP:
		{
			int op = read_int(vm, ins, 0);
			Variable b = pop(vm);
			Variable a = pop(vm);
			Variable result = binop(vm, &a, &b, op);
			info(vm, "binop result: %d\n", result.u.ival);
			push(vm, result);
			ASSERT_STACK(-1);
		}
		break;

		default:
		{
            vm_error(vm, "Opcode %s unhandled", opcode_names[ins->opcode]);
		}
		break;
	}
	if(vm->flags & VM_FLAG_VERBOSE)
	{
		// print_locals(vm);
		// print_globals(vm);
		vm_stacktrace(vm);
		// vm_print_thread_info(vm);
		// getchar();
	}
	return true;
}

void vm_cleanup(VM* vm)
{
}

// static uint64_t permute64(uint64_t x)
// {
//     x += 1111111111111111111u; x ^= x >> 32;
//     x *= 1111111111111111111u; x ^= x >> 32;
//     return x;
// }
// Maybe use integer keys instead?

#include <ctype.h>

static uint64_t vm_hash_string(const char *s)
{
	uint64_t h = 0x100;
	for(ptrdiff_t i = 0; s[i]; i++)
	{
		h ^= tolower(s[i]);
		h *= 1111111111111111111u;
	}
	return h;
}

#ifndef _WIN32
	#include <strings.h>
	#ifndef stricmp
		#define stricmp strcasecmp
	#endif
#endif

ObjectField *vm_object_upsert(VM *vm, Object *o, const char *key)
{
	ObjectField **m = &o->fields;
	for(uint64_t h = vm_hash_string(key);; h <<= 2)
	{
		if(!*m)
		{
			if(!vm)
			{
				return NULL;
			}

			if(!o->tail)
			{
				o->fields = NULL;
				o->tail = &o->fields;
			}

			ObjectField *new_node = object_pool_allocate(&vm->pool.uo, ObjectField);
			if(!new_node)
				vm_error(vm, "No object fields left");
			o->field_count++;
			memset(new_node, 0, sizeof(ObjectField));
			new_node->key = key;
			Variable *v = object_pool_allocate(&vm->pool.uo, Variable);
			if(!v)
				vm_error(vm, "No variables left");
			v->type = VAR_UNDEFINED;
			new_node->value = v;
			new_node->next = NULL;
			
			*m = new_node;
			*o->tail = new_node;
			o->tail = &new_node->next;
			return new_node;
		}
		if(!stricmp((*m)->key, key))
		{
			return *m;
		}
		m = &(*m)->child[h >> 62];
	}
	return NULL;
}

void get_object_field(VM *vm, Variable *ov, const char *key)
{
	op_load_field_object_(vm, *ov, key);
	// int idx = vm_string_index(vm, key);
	// Object *o = object_for_var(ov);
	// ObjectField *entry = vm_object_upsert(NULL, o, string(vm, idx));
	// if(!entry)
	// {
	// 	push(vm, undef);
	// } else
	// {
	// 	push(vm, *entry->value);
	// }
}

void vm_get_object_field(VM *vm, int obj_index, const char *key)
{
	int idx = vm_string_index(vm, key);
	Variable *ov = vm_stack(vm, obj_index);
	if(ov->type != VAR_OBJECT)
		vm_error(vm, "'%s' is not an object", variable_type_names[ov->type]);
	// Object *o = object_for_var(ov);
	get_object_field(vm, ov, key);
	// ObjectField *entry = vm_object_upsert(NULL, o, string(vm, idx));
	// if(!entry)
	// {
	// 	push(vm, undef);
	// } else
	// {
	// 	push(vm, *entry->value);
	// }
}

void set_object_field(VM *vm, Variable *ov, const char *key)
{
	int idx = vm_string_index(vm, key);
	Object *o = object_for_var(ov);
	ObjectField *entry = vm_object_upsert(vm, o, string(vm, idx));
	*entry->value = pop(vm);
}

void vm_set_object_field(VM *vm, int obj_index, const char *key)
{
	int idx = vm_string_index(vm, key);
	Variable *ov = vm_stack(vm, obj_index);
	if(ov->type != VAR_OBJECT)
		vm_error(vm, "'%s' is not an object", variable_type_names[ov->type]);
	Object *o = object_for_var(ov);
	ObjectField *entry = vm_object_upsert(vm, o, string(vm, idx));
	*entry->value = pop(vm);
}

void vm_init(VM *vm, Allocator *allocator, StringTable *strtab, const char *default_self, int max_threads)
{
	memset(vm, 0, sizeof(vm));
	vm->thread = &vm->temp_thread;
	vm->max_threads = max_threads;
	vm->allocator = allocator;
	vm->strings = strtab;
	vm->random_state = time(0);
	vm->frame = 0;
	vm->thread_buffer = allocator->malloc(allocator->ctx, sizeof(Thread*) * max_threads);
	snprintf(vm->default_self, sizeof(vm->default_self), "%s", default_self);
	for(int i = 0; i < VM_MAX_EVENTS_PER_FRAME; ++i)
	{
		vm->events[i].frame = -1;
	}
	if(!uo_init(&vm->pool.uo, (1 << 16), -1, allocator))
		vm_error(vm, "Failed to initialize union objects");
	// variable_init(&vm->pool.variables, (1 << 19), -1, allocator);
	// object_init(&vm->pool.objects, (1 << 19), -1, allocator);
	// object_field_init(&vm->pool.object_fields, (1 << 19), -1, allocator);
	if(!thread_init(&vm->pool.threads, max_threads, -1, allocator))
		vm_error(vm, "Failed to initialize threads");
	if(!stack_frame_init(&vm->pool.stack_frames, max_threads * VM_FRAME_SIZE, -1, allocator))
		vm_error(vm, "Failed to initialize stack frames");

	size_t N = 16384;
	char *arena_mem = allocator->malloc(allocator->ctx, N);
	if(!arena_mem)
		vm_error(vm, "Failed to allocate function memory");
	arena_init(&vm->c_function_arena, arena_mem, N);

	// hash_trie_init(&vm->c_functions);
	// hash_trie_init(&vm->c_methods);
	// hash_table_init(&vm->c_functions, 10, &allocator);
    // hash_table_init(&vm->c_methods, 10, &allocator);

	vm->global_object = vm_create_object(vm);
	vm->global_object.u.oval->refcount = VM_REFCOUNT_NO_FREE;

	// for(size_t i = 0; i < VAR_GLOB_MAX; ++i)
	// {
	// 	vm->globals[i] = vm_create_object(vm);
	// 	object_for_var(&vm->globals[i])->tag = variable_globals[i];
	// 	object_for_var(&vm->globals[i])->refcount = VM_REFCOUNT_NO_FREE;
	// }

	// {
	// 	int level = vm_sp(vm);
	// 	push(vm, vm->globals[VAR_GLOB_LEVEL]); // TODO: FIXME is actually an array
	// 	Variable empty = vm_create_object(vm);
	// 	vm_pushobject(vm, empty.u.oval);
	// 	vm_set_object_field(vm, level, "struct");
	// }

	// {
	// 	int anim = vm_sp(vm);
	// 	push(vm, vm->globals[VAR_GLOB_ANIM]);
	// 	vm_pushbool(vm, false);
	// 	vm_set_object_field(vm, anim, "chatInitialized");
	// }

	// size_t n = 64 * 10000 * 10000;
	// char *buf = malloc(n); // TODO: free memory
	// arena_init(&vm->arena, buf, n);
}

typedef struct
{
	void *callback;
	void *ctx;
} CallbackFunction;

void vm_register_callback_function(VM *vm, const char *name, void *callback, void *ctx)
{
	CallbackFunction *f = vm->allocator->malloc(vm->allocator->ctx, sizeof(CallbackFunction));
	f->callback = callback;
	f->ctx = ctx ? ctx : vm;
	hash_trie_upsert(&vm->callback_functions, name, vm->allocator, false)->value = f;
}

void vm_register_c_function(VM *vm, const char *name, vm_CFunction callback)
{
	vm_register_callback_function(vm, name, (void *)callback, vm);
}

static CallbackFunction *get_callback_function(VM *vm, const char *name)
{
	HashTrieNode *entry = hash_trie_upsert(&vm->callback_functions, name, NULL, false);
	if(!entry)
		return NULL;
	return entry->value;
}

// void vm_register_callback_method(VM *vm, const char *name, void *callback, void *ctx)
// {
// 	CallbackFunction *f = vm->allocator->malloc(vm->allocator->ctx, sizeof(CallbackFunction));
// 	f->callback = callback;
// 	f->ctx = ctx ? ctx : vm;
// 	hash_trie_upsert(&vm->callback_methods, name, vm->allocator, false)->value = f;
// }

// void vm_register_c_method(VM *vm, const char *name, vm_CMethod callback)
// {
// 	vm_register_callback_method(vm, name, (void *)callback, vm);
// }

// static CallbackFunction *get_callback_method(VM *vm, const char *name)
// {
// 	HashTrieNode *entry = hash_trie_upsert(&vm->callback_methods, name, NULL, false);
// 	if(!entry)
// 		return NULL;
// 	return entry->value;
// }

// bool vm_run(VM *vm, float dt)
// {
// 	for(;;)
//     {
// 		Thread *thr = vm->thread;
// 		if(thr->wait > 0.f)
// 		{
// 			thr->wait -= dt;
// 			return true;
// 		}
// 		thr->wait = 0.f;
// 		StackFrame *sf = stack_frame(vm, thr);
// 		if(sf->ip >= buf_size(sf->instructions))
// 		{
// 			// vm_error(vm, "ip oob");
// 			return false;
// 		}
// 	    Instruction *current = &sf->instructions[sf->ip++];
// 		vm_execute(vm, current);
// 		if(current->opcode == OP_RET)
// 		{
// 			break;
// 		}
//     }
// 	return true;
// }


size_t vm_argc(VM *vm)
{
	return vm->nargs;
}

Variable *vm_argv(VM *vm, int idx)
{
	return &vm->thread->stack[vm->fsp - 3 - idx];
}

Variable *vm_stack(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	return &thr->stack[idx];
}

Variable *vm_stack_top(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	return &thr->stack[thr->sp + idx];
}

const char *vm_cast_string(VM *vm, Variable *arg)
{
	switch(arg->type)
	{
		case VAR_INTERNED_STRING:
		case VAR_STRING: return variable_string(vm, arg);
		case VAR_FLOAT:
		{
			char *str = new(&vm->c_function_arena, char, 32);
			snprintf(str, 32, "%f", arg->u.fval);
			return str;
		}
		break;
		case VAR_UNDEFINED: return "undefined";
		// case VAR_BOOLEAN: return arg->u.ival ? "true" : "false";
		case VAR_BOOLEAN: return arg->u.ival ? "1" : "0";
		case VAR_INTEGER:
		{
			char *str = new(&vm->c_function_arena, char, 32);
			snprintf(str, 32, "%" PRId64, arg->u.ival);
			return str;
		}
		break;
		default: vm_error(vm, "Not a string");
	}
	return "";
}

const char *vm_checkstring(VM *vm, int idx)
{
	return vm_cast_string(vm, vm_argv(vm, idx));
}

// https://youtu.be/LWFzPP8ZbdU?t=970
uint32_t vm_random(VM *vm) // xorshift1
{
	uint32_t *state = &vm->random_state;
	*state ^= (*state << 13);
	*state ^= (*state >> 17);
	*state ^= (*state << 5);
	return *state;
}

void vm_cast_vector(VM *vm, Variable *arg, float *outvec)
{
	if(arg->type != VAR_VECTOR)
		vm_error(vm, "'%s' is not a vector", variable_type_names[arg->type]);
	memcpy(outvec, arg->u.vval, sizeof(arg->u.vval));
}

void vm_checkvector(VM *vm, int idx, float *outvec)
{
	vm_cast_vector(vm, vm_argv(vm, idx), outvec);
}

float vm_cast_float(VM *vm, Variable *arg)
{
	switch(arg->type)
	{
		case VAR_INTEGER: return (float)arg->u.ival;
		case VAR_FLOAT: return arg->u.fval;
		default: vm_error(vm, "Not a number");
	}
	return 0;
}

Object *vm_cast_object(VM *vm, Variable *arg)
{
	if(arg->type != VAR_OBJECT)
		vm_error(vm, "'%s' is not an object", variable_type_names[arg->type]);
	return arg->u.oval;
}

int vm_checkobject(VM *vm, int idx)
{
	Object *o = vm_cast_object(vm, vm_argv(vm, idx));
	return vm->fsp - 3 - idx;
}

#define DEFINE_VM_CHECK_FUNC(name, type, cast_func) \
	type vm_check##name(VM *vm, int idx) { \
		return cast_func(vm, vm_argv(vm, idx)); \
	}

DEFINE_VM_CHECK_FUNC(float, float, vm_cast_float)
DEFINE_VM_CHECK_FUNC(bool, bool, vm_cast_bool)
DEFINE_VM_CHECK_FUNC(integer, int64_t, vm_cast_int)

bool vm_cast_bool(VM *vm, Variable *arg)
{
	if(arg->type != VAR_BOOLEAN && arg->type != VAR_INTEGER)
		vm_error(vm, "Not a boolean");
	return arg->u.ival;
}

int64_t vm_cast_int(VM *vm, Variable *arg)
{
	if(arg->type != VAR_INTEGER)
		vm_error(vm, "Not a integer");
	return arg->u.ival;
}

void vm_pushstring_n(VM *vm, const char *str, size_t n) // n is without \0
{
	Variable v = var(vm);
	v.type = VAR_STRING;
	v.u.sval = allocate_variable_string(vm, n + 1);
	// v.u.sval = malloc(n + 1);
	memcpy(v.u.sval.data, str, n);
	v.u.sval.data[n] = 0;
	push(vm, v);
}

void vm_pushstring(VM *vm, const char *str)
{
	Variable v = var(vm);
	v.type = VAR_STRING;
	int n = strlen(str);
	v.u.sval = allocate_variable_string(vm, n + 1);
	memcpy(v.u.sval.data, str, n);
	v.u.sval.data[n] = 0;
	// v.u.sval = strdup(str);
	push(vm, v);
}

void vm_pushvector(VM *vm, float *vec)
{
	Variable v = var(vm);
	v.type = VAR_VECTOR;
	for(int k = 0; k < 3; ++k)
		v.u.vval[k] = vec[k];
	push(vm, v);
}

void vm_pushvar(VM *vm, Variable *v)
{
	push(vm, *v);
}

int vm_pushobject(VM *vm, Object *o)
{
	Variable v = var(vm);
	v.type = VAR_OBJECT;
	v.u.oval = o;
	int sp = vm_sp(vm);
	push(vm, v);
	return sp;
}

Thread *vm_thread(VM *vm)
{
	return vm->thread;
}

#define DEFINE_VM_PUSH_FUNC(name, ctype, var_type, assignment) \
	void vm_push##name(VM *vm, ctype val) { \
		Variable v = var(vm); \
		v.type = var_type; \
		assignment; \
		push(vm, v); \
	}

DEFINE_VM_PUSH_FUNC(integer, int64_t, VAR_INTEGER, v.u.ival = val)
DEFINE_VM_PUSH_FUNC(float, float, VAR_FLOAT, v.u.fval = val)
DEFINE_VM_PUSH_FUNC(bool, bool, VAR_BOOLEAN, v.u.ival = val ? 1 : 0)

void vm_pushundefined(VM *vm)
{
    Variable v = var(vm);
    v.type = VAR_UNDEFINED;
    push(vm, v);
}

// TODO: make use of namespace
static void call_c_function(VM *vm, const char *namespace, const char *function, int function_string_index, size_t nargs, int call_flags)
{
	vm->nargs = nargs;
	vm->fsp = vm->thread->sp;
	// if(call_flags & VM_CALL_FLAG_THREADED)
	// {
	// 	vm_error(vm, "Can't call builtin functions threaded");
	// }
	Arena rollback = vm->c_function_arena;
	int nret;
	if(!(call_flags & VM_CALL_FLAG_METHOD))
	{
		CallbackFunction *cfunc = get_callback_function(vm, function);
		if(!cfunc)
		{
			vm_error(vm, "No builtin function '%s::%s'", namespace, function);
		}
		vm_CFunction fun = (vm_CFunction)cfunc->callback;
		nret = fun(cfunc->ctx);
	}
	else
	{
		Variable *self = vm_argv(vm, -1);
		if(self->type != VAR_OBJECT)
		{
			vm_error(vm, "'%s' is not an object", variable_type_names[self->type]);
		}
		Object *o = object_for_var(self);
		if(!o->proxy)
		{
			vm_error(vm,
					 "no methods for %s created at %s line %d in function %s, method: %s",
					 o->tag,
					 o->debug_info.file,
					 o->debug_info.line,
					 o->debug_info.function,
					 function);
		}
		gsc_Function func = object_find_callable(vm, o, "__call", function);
		if(!func)
		{
			vm_error(vm, "No builtin method '%s::%s' for %s", namespace, function, o->proxy->tag);
		}
		nret = func(vm->ctx);
	}
	if(nret == 0)
	{
		push(vm, undef);
	}
	Variable ret = pop(vm);

	// Pop all args
	for(size_t i = 0; i < nargs + 2; ++i)
	{
		Variable arg = pop(vm);
		// decref(vm, &arg);
	}

	// Put ret back on stack
	push(vm, ret);

	vm->c_function_arena = rollback;
}

void vm_serialize_variable(VM *vm, Stream *s, Variable *v)
{
    if(v->type != VAR_OBJECT)
    {
        char buf[256];
        vm_stringify(vm, v, buf, sizeof(buf));
		// if(v->type == VAR_STRING)
			stream_printf(s, "%s", buf);
		// else
		// 	stream_printf(s, "%s", buf);
	} else
    {
        Object *o = v->u.oval;
        stream_printf(s, "{");
        size_t n = o->field_count;
        size_t i = 0;
        for(ObjectField *it = o->fields; it; it = it->next)
        {
            stream_printf(s, "%s: ", it->key);
            vm_serialize_variable(vm, s, it->value);
            if(i++ < n - 1)
            stream_printf(s, ",");
        }
        stream_printf(s, "}");
    }
}

const char *variable_type_name(Variable *v)
{
	return variable_type_names[v->type];
}

int vm_line_number_for_function(VM *vm, const char *file, const char *function)
{
	CompiledFunction *vmf = vm->func_lookup(vm->ctx, file, function);
	if(!vmf)
		return -1;
	return vmf->line;
}

const char *vm_stack_frame_variable_name(VM *vm, StackFrame *sf, int index)
{
	CompiledFunction *vmf = vm->func_lookup(vm->ctx, sf->file, sf->function);
	if(!vmf)
		return NULL;
	return vmf->variable_names[index];
}

static bool call_function(VM *vm, Thread *thr, const char *file, const char *function, int function_string_index, size_t nargs, bool reversed, int call_flags)
{
	// printf("call_function(%s::%s)\n", file, function);
	CompiledFunction *vmf = vm->func_lookup(vm->ctx, file, function);
    if(!vmf)
    {
		call_c_function(vm, file, function, function_string_index, nargs, call_flags);
        return false;
	}
	// Object *prev_self = object_for_var(&vm->globals[VAR_GLOB_LEVEL]);
	// if(thr->bp != 0)
	// {
	// 	prev_self = object_for_var(&thr->frames[thr->bp - 1].self);
	// }
	// if(thr->bp >= VM_FRAME_SIZE)
	// 	vm_error(vm, "thr->bp >= VM_FRAME_SIZE");
	// thr->frame = &thr->frames[thr->bp++];
	// thr->bp++;
	StackFrame *sf = stack_frame(vm, thr);
	// memset(sf, 0, sizeof(StackFrame));
	// sf->locals = NULL;
	// sf->self.u.oval = self ? self : prev_self;
	// sf->local_count = vmf->local_count;
	// sf->locals = new(&vm->arena, Variable, vmf->local_count);
	sf->local_count = vmf->local_count;
	for(size_t i = 0; i < vmf->local_count; ++i)
	{
		Variable *v = object_pool_allocate(&vm->pool.uo, Variable);
		if(!v)
			vm_error(vm, "No variables left");
		v->type = VAR_UNDEFINED;
		v->u.ival = 0;
		sf->locals[i] = v;
		// buf_push(sf->locals, (Variable) { 0 });
		// sf->locals[i].type = VAR_UNDEFINED;
		// sf->locals[i].u.ival = 0;
	}
	pop_thread(vm, thr); //nargs
	// + 1 for implicit self parameter
	for(size_t i = 0; i < nargs + 1; ++i)
	{
		Variable arg = pop_thread(vm, thr);
		// decref(vm, &arg);
		if(i < vmf->parameter_count + 1)
		{
			size_t local_idx = reversed ? (nargs + 1) - i - 1 : i;
			*sf->locals[local_idx] = arg;
		}
	}
	sf->file = file;
    sf->function = function;
    sf->instructions = vmf->instructions;
	sf->instruction_count = vmf->instruction_count;
	sf->ip = 0;
	// static char asm_filename[256];
	// snprintf(asm_filename, sizeof(asm_filename), "debug/%s_%s.gscasm", file, function);
	// FILE *fp = fopen(asm_filename, "w");
	// for(size_t i = 0; i < buf_size(vmf->instructions); ++i)
	// 	print_instruction(vm, &vmf->instructions[i], fp);
	// fclose(fp);
	// getchar();
	return true;
}

bool vm_call_function_thread(VM *vm, const char *file, const char *function, size_t nargs, Variable *self)
{
	vm->thread = object_pool_allocate(&vm->pool.threads, Thread);
	if(!vm->thread)
		vm_error(vm, "No threads left");
	memset(vm->thread, 0, sizeof(Thread));
	vm->thread->bp = 0;
	vm->thread->return_value = NULL;
	vm->thread->state = VM_THREAD_ACTIVE;
	// push_thread(vm, vm->thread, self ? *self : vm->globals[VAR_GLOB_LEVEL]);
	if(self)
	{
		push_thread(vm, vm->thread, *self);
	}
	else
	{
		gsc_get_global(vm->ctx, vm->default_self);
	}
	push_thread(vm, vm->thread, integer(vm, nargs));
	if(self && self->type != VAR_OBJECT)
		vm_error(vm, "'%s' is not an object", variable_type_names[self->type]);
	bool result = call_function(vm, vm->thread, file, function, vm_string_index(vm, function), nargs, false, 0);
	add_thread(vm, vm->thread);
	vm->thread = &vm->temp_thread;
	return result;
}

static bool variable_eq(Variable *a, Variable *b)
{
	if(a->type != b->type)
		return false;
	float eps = 0.0001f;
	switch(a->type)
	{
		case VAR_FLOAT: return fabs(a->u.fval - b->u.fval) < eps;
	}
	return !memcmp(&a->u, &b->u, sizeof(a->u));
}

static VMEvent *get_free_event(VM *vm)
{
	for(int i = 0; i < VM_MAX_EVENTS_PER_FRAME; ++i)
	{
		if(vm->events[i].frame == -1)
			return &vm->events[i];
	}
	vm_error(vm, "No free events");
	return NULL;
}

static void free_event(VMEvent *ev)
{
	ev->frame = -1;
}

void vm_notify(VM *vm, Object *object, const char *key, size_t nargs)
{
	// TODO: args

	int name = vm_string_index(vm, key);
	if(name == -1)
	{
		vm_error(vm, "Can't find string '%s'", key);
	}
	printf("Notifying '%s'\n", key);
	VMEvent *ev = get_free_event(vm);
	ev->object = object;
	ev->name = name;
	for(int i = 1; i < nargs; i++)
	{
		ev->arguments[i - 1] = *vm_argv(vm, i);
	}
	ev->numargs = nargs - 1;
	ev->frame = vm->frame;
	// VMEvent ev = { .object = object, .name = name, .arguments = NULL };
	// if(vm->event_count >= VM_MAX_EVENTS_PER_FRAME)
	// 	vm_error(vm, "event_count >= VM_MAX_EVENTS_PER_FRAME");
	// vm->events[vm->event_count++] = ev;
	// buf_push(vm->events, ev);
}

static void run_thread(VM *vm)
{
	while(vm->thread->state == VM_THREAD_ACTIVE)
    {
		StackFrame *sf = stack_frame(vm, vm->thread);
		if(sf->ip >= sf->instruction_count)
		{
			vm_error(vm, "ip oob %d/%d", sf->ip, sf->instruction_count);
		}
	    Instruction *current = &sf->instructions[sf->ip++];
		if(!vm_execute_instruction(vm, current))
		{
			break;
		}
    }
}

bool vm_run_threads(VM *vm, float dt)
{
	int N = thread_count(vm);
	for(int i = 0; i < N; ++i)
	{
		Thread *t = remove_thread(vm);
		if(!t)
			break;
		StackFrame *sf = NULL;
		if(t->bp >= 0)
			sf = &t->frames[t->bp];
		// printf("Processing thread %s::%s (%s)\n", sf ? sf->file : "?", sf ? sf->function : "?", vm_thread_state_names[t->state]);
		// getchar();
		// for(size_t j = 0; j < vm->event_count; j++)
		for(size_t j = 0; j < VM_MAX_EVENTS_PER_FRAME; j++)
		{
			VMEvent *ev = &vm->events[j];
			if(ev->frame == -1 || ev->frame == vm->frame)
				continue;
			for(size_t k = 0; k < t->endon_string_count; ++k)
			{
				if(ev->name == t->endon[k])
				{
					t->state = VM_THREAD_INACTIVE;
					// printf("Thread %d killed by event '%s'\n", i, string(vm, ev->name));
					break;
				}
			}
		}
		// if(t->state != VM_THREAD_INACTIVE)
		// {
		// 	add_thread(vm, t);
		// }
		switch(t->state)
		{
			case VM_THREAD_WAITING_TIME:
			{
				if(t->wait <= 0.f)
				{
					t->state = VM_THREAD_ACTIVE;
					// printf("Thread %d resumed after wait\n", i);
				}
				t->wait -= dt;
			}
			break;

			case VM_THREAD_WAITING_FRAME:
			{
				t->state = VM_THREAD_ACTIVE;
			}
			break;

			case VM_THREAD_INACTIVE:
			{
				// TODO: cleanup
				// for(size_t k = 0; k < t->bp; ++k)
				// {
				// 	StackFrame *sf = &t->frames[--t->bp];
				// 	buf_free(sf->locals);
				// }
				object_pool_deallocate(&vm->pool.threads, t);
				t = NULL;
			}
			break;

			case VM_THREAD_ACTIVE:
			{
				vm->thread = t;
				run_thread(vm);
				vm->thread = &vm->temp_thread;
			}
			break;

			case VM_THREAD_WAITING_EVENT:
			{
				// for(size_t j = 0; j < vm->event_count; j++)
				for(size_t j = 0; j < VM_MAX_EVENTS_PER_FRAME; j++)
				{
					VMEvent *ev = &vm->events[j];
					if(ev->frame == -1 || ev->frame == vm->frame)
						continue;
					if(ev->name == t->waittill.name && ev->object == t->waittill.object)
					{
						int min = t->waittill.numargs;
						if(ev->numargs < min)
							min = ev->numargs;
						for(int k = 0; k < min; k++)
						{
							Variable *dst = t->waittill.arguments[k].u.refval;
							Variable *src = &ev->arguments[k];
							dst->type = src->type;
							memcpy(&dst->u, &src->u, sizeof(dst->u));
						}
						t->state = VM_THREAD_ACTIVE;
						// printf("Thread %d resumed on event '%s'\n", i, string(vm, ev->name));
					}
				}
			}
			break;
		}
		
		if(t)
			add_thread(vm, t);
	}

	for(size_t j = 0; j < VM_MAX_EVENTS_PER_FRAME; j++)
	{
		VMEvent *ev = &vm->events[j];
		if(ev->frame == -1 || ev->frame == vm->frame)
			continue;
		free_event(ev);
	}
	
	// vm->event_count = 0; // Reset for next frame
	vm->frame++;
	return vm->thread_read_idx != vm->thread_write_idx;
}
