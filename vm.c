#include "vm.h"
#include <math.h>
#include "lexer.h"
#include "ast.h"
#include <signal.h>

#ifndef MAX
	#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif
enum
{
	UNION_OBJECT_SIZE =
		MAX(1, MAX(1, MAX(sizeof_ObjectField, MAX(sizeof_Variable, sizeof_Object))))
	// sizeof_Unit =
	// 	MAX(sizeof_Thread, MAX(sizeof_StackFrame, MAX(sizeof_ObjectField, MAX(sizeof_Variable, sizeof_Object))))
};
typedef struct
{
	char data[UNION_OBJECT_SIZE];
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
	if(t->bp == -1)
		vm_error(vm, "No stack frame for thread");
	return &t->frames[t->bp - 1];
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
	if(thr->bp - 1 <= 0)
	{
		printf("\tNo callstack.\n");
	}
	else
	{
		for(int i = 0; i < thr->bp - 1; i++)
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
	if(thr->bp > 0)
	{
		sf = &thr->frames[thr->bp - 1];
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
	return VM_THREAD_POOL_SIZE - (vm->thread_read_idx - vm->thread_write_idx);
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
		Thread *t = vm->thread_buffer[(vm->thread_read_idx + i) % VM_THREAD_POOL_SIZE];
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

static void print_globals(VM *vm)
{
	for(int i = 0; i < VAR_GLOB_MAX; ++i)
	{
		printf("global %s: %s\n", variable_globals[i], variable_type_names[vm->globals[i].type]);
		print_object(vm, variable_globals[i], &vm->globals[i], 1);
	}
}

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

#ifndef COUNT_OF(x)
	#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

void vm_error(VM *vm, const char *fmt, ...)
{
    Thread *thr = vm->thread;
    StackFrame *sf = NULL;
	if(thr->bp > 0)
		sf = &thr->frames[thr->bp - 1];
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
	// vm_stacktrace(vm);
	// print_globals(vm);
	// vm_print_thread_info(vm);
	abort();
	if(vm->jmp)
		longjmp(*vm->jmp, 1);
}

static Variable *local(VM *vm, size_t index)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
	if(index >= buf_size(sf->locals))
	{
		vm_error(vm, "Invalid local index %d/%d", (int)index, (int)buf_size(sf->locals));
		return NULL;
	}
	return &sf->locals[index];
}

static void print_locals(VM *vm)
{
    Thread *thr = vm->thread;
	StackFrame *sf = stack_frame(vm, thr);
	printf(" || Local variables ||\n");
	char str[1024];
	for(size_t i = 0; i < buf_size(sf->locals); ++i)
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
	print_globals(vm);
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
			free(v->u.sval);
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

Variable vm_create_object(VM *vm)
{
	Variable v = { .type = VAR_OBJECT };
	Object *o = object_pool_allocate(&vm->pool.uo);
	o->fields = NULL;
	o->tail = &o->fields;
	o->refcount = 0;
	o->field_count = 0;
	v.u.oval = o;
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

static Variable *pop_ref(VM *vm)
{
	Variable value = pop(vm);
	if(value.type != VAR_REFERENCE)
	{
		vm_error(vm, "'%s' is not a variable reference", variable_type_names[value.type]);
	}
	return value.u.refval;
}

static int cast_int(VM* vm, Variable *v)
{
	if(v->type != VAR_INTEGER)
		vm_error(vm, "'%s' is not a integer", variable_type_names[v->type]);
	return v->u.ival;
}

static Variable *top(VM *vm, int offset)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
    Variable *v = &thr->stack[thr->sp - 1 - offset];
	return v;
}

static void pop_string(VM *vm, char *str, size_t n)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
    Variable *top = &thr->stack[--thr->sp];
	switch(top->type)
	{
		// TODO: directly use a hash and memcmp for integer/other type of keys

		case VAR_INTEGER: snprintf(str, n, "%d", top->u.ival); break;
		case VAR_FLOAT: snprintf(str, n, "%f", top->u.fval); break;
		case VAR_STRING: snprintf(str, n, "%s", top->u.sval); break;
		default: vm_error(vm, "'%s' is not a string", variable_type_names[top->type]); break;
	}
	decref(vm, &top);
}

static int pop_int(VM *vm)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
    Variable *top = &thr->stack[--thr->sp];
    if(top->type != VAR_INTEGER && top->type != VAR_BOOLEAN)
		vm_error(vm, "'%s' is not a integer", variable_type_names[top->type]);
    int i = top->u.ival;
    decref(vm, &top);
    return i;
}

static int read_int(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_INT)
		vm_error(vm, "Operand '%s' is not a integer", operand_type_names[op->type]);
    return op->value.integer;
}

static int read_string_index(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_INDEXED_STRING)
		vm_error(vm, "Operand '%s' is not a string", operand_type_names[op->type]);
	return op->value.string_index;
}

static const char* read_string(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_INDEXED_STRING)
		vm_error(vm, "Operand '%s' is not a string", operand_type_names[op->type]);
	return string_table_get(vm->strings, op->value.string_index);
}

static float read_float(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_FLOAT)
		vm_error(vm, "Operand '%s' is not a float", operand_type_names[op->type]);
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

				"STRING",	 "INTEGER",	 "BOOLEAN",			 "FLOAT",	 "VECTOR",
				"FUNCTION", "LOCALIZED_STRING", "UNDEFINED"
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
	switch(v->type)
	{
		case VAR_UNDEFINED: return "undefined";
		case VAR_BOOLEAN: return v->u.ival == 0 ? "0" : "1";
		// case VAR_BOOLEAN: return v->u.ival == 0 ? "false" : "true";
		case VAR_FLOAT: snprintf(buf, n, "%.2f", v->u.fval); return buf;
		case VAR_INTEGER: snprintf(buf, n, "%d", v->u.ival); return buf;
		case VAR_LOCALIZED_STRING:
		// case VAR_ANIMATION:
		case VAR_STRING: return v->u.sval;
		case VAR_OBJECT: return "[object]";
		case VAR_FUNCTION: return "[function]";
		case VAR_VECTOR: snprintf(buf, n, "(%.2f, %.2f, %.2f)", v->u.vval[0], v->u.vval[1], v->u.vval[2]); return buf;
	}
	return NULL;
}

static Variable coerce_int(VM *vm, Variable *v)
{
	Variable result = { .type = VAR_INTEGER };
	switch(v->type)
	{
		case VAR_BOOLEAN:
		case VAR_INTEGER: result = *v; break;
		case VAR_FLOAT: result.u.ival = (int)v->u.fval; break;
		case VAR_STRING: result.u.ival = atoi(v->u.sval); break;
		default: vm_error(vm, "Cannot coerce '%s' to integer", variable_type_names[v->type]); break;
	}
	return result;
}

static Variable coerce_vector(VM *vm, Variable *v)
{
	Variable result = { .type = VAR_VECTOR };
	switch(v->type)
	{
		case VAR_INTEGER: for(int i = 0; i < 3; ++i) result.u.vval[i] = (float)v->u.ival; break;
		case VAR_FLOAT: for(int i = 0; i < 3; ++i) result.u.vval[i] = v->u.fval; break;
		case VAR_VECTOR: return *v;
		default: vm_error(vm, "Cannot coerce '%s' to vector", variable_type_names[v->type]); break;
	}
	return result;
}

static Variable coerce_float(VM *vm, Variable *v)
{
	Variable result = { .type = VAR_FLOAT };
	switch(v->type)
	{
		case VAR_INTEGER: result.u.fval = (float)v->u.ival; break;
		case VAR_FLOAT: result = *v; break;
		case VAR_STRING: result.u.fval = atof(v->u.sval); break;
		default: vm_error(vm, "Cannot coerce '%s' to float", variable_type_names[v->type]); break;
	}
	return result;
}

static Variable unary(VM *vm, Variable *arg, int op)
{
	bool err = false;
	char temp[64];
	Variable result = { .type = arg->type };
	switch(arg->type)
	{
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
			int a = coerce_int(vm, arg).u.ival;
			int *b = &result.u.ival;
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
			int a = coerce_int(vm, lhs).u.ival;
			int b = coerce_int(vm, rhs).u.ival;
			int *c = &result.u.ival;
			switch(op)
			{
				case TK_PLUS_ASSIGN:
				case '+': *c = a + b; break;
				case TK_DIV_ASSIGN:
				case '/': *c = a / b; break;
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
				case '/': *c = a / b; break;
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
		case VAR_STRING:
		{
			// TODO: FIXME
			static char a_buf[4096];
			static char b_buf[4096];
			char *a = vm_stringify(vm, lhs, a_buf, sizeof(a_buf));
			char *b = vm_stringify(vm, rhs, b_buf, sizeof(b_buf));
			switch(op)
			{
				case TK_PLUS_ASSIGN:
				case '+':
				{
					size_t n = strlen(a) + strlen(b) + 1;
					char *str = malloc(n);
					snprintf(str, n, "%s%s", a, b);

					result.u.sval = str;
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

static Variable integer(VM *vm, int i)
{
	Variable v = var(vm);
	v.type = VAR_INTEGER;
	v.u.ival = i;
	return v;
}

void add_thread(VM *vm, Thread *t)
{
	if((vm->thread_write_idx + 1) % VM_THREAD_POOL_SIZE == vm->thread_read_idx)
	{
		vm_error(vm, "Maximum amount of threads reached");
	}
	vm->thread_buffer[vm->thread_write_idx] = t;
	vm->thread_write_idx = (vm->thread_write_idx + 1) % VM_THREAD_POOL_SIZE;
}

Thread *remove_thread(VM *vm)
{
	if(vm->thread_read_idx == vm->thread_write_idx)
		return NULL;
	Thread *t = vm->thread_buffer[vm->thread_read_idx];
	vm->thread_read_idx = (vm->thread_read_idx + 1) % VM_THREAD_POOL_SIZE;
	return t;
}

static bool call_function(VM *vm, Thread*, const char *file, const char *function, size_t nargs, bool, int);
#define ASSERT_STACK(X)                                                              \
	do                                                                               \
	{                                                                                \
		if(thr->sp != sp + (X))                                                      \
			vm_error(vm, "Stack cookie failed for '%s'! Expected %d, got %d", opcode_names[ins->opcode], sp + (X), thr->sp); \
	} while(0)

// To "computed goto" or not to "computed goto" portability wise...
// GCC feature "labels as values"
static bool execute_instruction(VM *vm, Instruction *ins)
{
    Thread *thr = vm->thread;
    StackFrame *sf = stack_frame(vm, thr);
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

		case OP_GLOB:
		{
			int idx = read_int(vm, ins, 0);
			bool as_ref = read_int(vm, ins, 1) > 0;
			if(idx < 0 || idx >= VAR_GLOB_MAX)
				vm_error(vm, "Invalid index for global");
			Variable *glob = &vm->globals[idx];

			if(as_ref)
				push(vm, ref(vm, glob));
			else
				push(vm, *glob);
			ASSERT_STACK(1);
		}
		break;

		case OP_FIELD_REF:
		{
			char prop[256] = { 0 };
			pop_string(vm, prop, sizeof(prop));
			Variable *obj = pop_ref(vm);
			if(obj->type != VAR_OBJECT)
			{
				if(obj->type == VAR_UNDEFINED) // Coerce to object... Just make this a new object
				{
					*obj = vm_create_object(vm);
				}
				else
				{
					vm_error(vm, "'%s' is not a object", variable_type_names[obj->type]);
				}
			}
			Object *o = object_for_var(obj);
			if(!o)
			{
				vm_error(vm, "object is null");
			}
			int idx = vm_string_index(vm, prop);
			ObjectField *entry = vm_object_upsert(vm, o, string(vm, idx)); // We're using the StringTable unique char* pointer to pass to upsert, prop wouldn't work
			push(vm, ref(vm, entry->value));
			ASSERT_STACK(-1);
		}
		break;

		case OP_LOAD_FIELD:
		{
			Variable obj = pop(vm);
			if(obj.type == VAR_STRING)
			{
				Variable key = pop(vm);
				const char *str = obj.u.sval;
				size_t n = strlen(str);
				if(key.type == VAR_STRING)
				{
					if(!strcmp(key.u.sval, "length") || !strcmp(key.u.sval, "size"))
					{
						vm_pushinteger(vm, n);
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

				if(obj.type == VAR_UNDEFINED)
				{
					push(vm, undef);
				}
				else if(obj.type != VAR_OBJECT)
				{
					vm_error(vm, "'%s' is not a object", variable_type_names[obj.type]);
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
			ASSERT_STACK(-1);
		}
		break;

		case OP_STORE:
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
					v.type = VAR_STRING;
					v.u.sval = string(vm, read_string_index(vm, ins, 1));
				}
				break;
                case AST_LITERAL_TYPE_LOCALIZED_STRING:
				{
					v.type = VAR_LOCALIZED_STRING;
					v.u.sval = string(vm, read_string_index(vm, ins, 1));
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
			push(vm, vm_create_object(vm));
		}
		break;

		case OP_RET:
		{
			if(thr->bp < 0)
				vm_error(vm, "bp < 0");

			buf_free(sf->locals);
			if(--thr->bp < 0)
			{
				thr->state = VM_THREAD_INACTIVE;
				pop(vm); // retval
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
			const char *function = NULL;
			const char *file = NULL;
			if(ins->opcode == OP_CALL_PTR)
			{
				Variable func = pop(vm);
				if(func.type != VAR_FUNCTION)
				{
					vm_error(vm, "'%s' is not a function pointer", variable_type_names[func.type]);
				}
				function = string(vm, func.u.funval.function);
				if(func.u.funval.file != -1)
					file = string(vm, func.u.funval.file);
			} else
			{
				function = read_string(vm, ins, 0);
				if(check_operand(ins, 1, OPERAND_TYPE_INDEXED_STRING))
					file = read_string(vm, ins, 1);
			}
			int call_flags = read_int(vm, ins, 3);
			// Object *object = NULL;
			if(call_flags & VM_CALL_FLAG_METHOD)
			{
				// Variable ov = pop(vm);
				// if(ov.type != VAR_OBJECT)
				// 	vm_error(vm, "'%s' is not a object", variable_type_names[ov.type]);
				// object = object_for_var(&ov);
				// object = pop_ref(vm);
			}
            int nargs = cast_int(vm, top(vm, 0));
            info(vm, "CALLING -> %s::%s %d\n", file, function, nargs);

			if(call_flags & VM_CALL_FLAG_THREADED)
			{
				Thread *nt = object_pool_allocate(&vm->pool.threads);
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
				push_thread(vm, nt, integer(vm, nargs));
				call_function(vm, nt, file ? file : sf->file, function, nargs, true, call_flags);
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
				if(!call_function(vm, thr, file ? file : sf->file, function, nargs, false, call_flags))
					thr->bp--;
			}
			// ASSERT_STACK(-nargs);
		}
		break;

		case OP_BINOP:
		{
			int op = read_int(vm, ins, 0);
			Variable a = pop(vm);
			Variable b = pop(vm);
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

static uint64_t vm_hash_string(const char *s)
{
	uint64_t h = 0x100;
	for(ptrdiff_t i = 0; s[i]; i++)
	{
		h ^= s[i];
		h *= 1111111111111111111u;
	}
	return h;
}

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

			ObjectField *new_node = object_pool_allocate(&vm->pool.uo);
			o->field_count++;
			memset(new_node, 0, sizeof(ObjectField));
			new_node->key = key;
			Variable *v = object_pool_allocate(&vm->pool.uo);
			v->type = VAR_UNDEFINED;
			new_node->value = v;
			new_node->next = NULL;
			
			*m = new_node;
			*o->tail = new_node;
			o->tail = &new_node->next;
			return new_node;
		}
		if(!strcmp((*m)->key, key))
		{
			return *m;
		}
		m = &(*m)->child[h >> 62];
	}
	return NULL;
}

void vm_init(VM *vm, Allocator *allocator)
{
	memset(vm, 0, sizeof(vm));
	vm->allocator = allocator;

	uo_init(&vm->pool.uo, (1 << 16), -1, allocator);
	// variable_init(&vm->pool.variables, (1 << 19), -1, allocator);
	// object_init(&vm->pool.objects, (1 << 19), -1, allocator);
	// object_field_init(&vm->pool.object_fields, (1 << 19), -1, allocator);
	thread_init(&vm->pool.threads, VM_THREAD_POOL_SIZE, -1, allocator);
	stack_frame_init(&vm->pool.stack_frames, VM_THREAD_POOL_SIZE * VM_FRAME_SIZE, -1, allocator);

	size_t N = 16384;
	arena_init(&vm->c_function_arena, allocator->malloc(allocator->ctx, N), N);

	// hash_trie_init(&vm->c_functions);
	// hash_trie_init(&vm->c_methods);
	// hash_table_init(&vm->c_functions, 10, &allocator);
    // hash_table_init(&vm->c_methods, 10, &allocator);
	for(size_t i = 0; i < VAR_GLOB_MAX; ++i)
	{
		vm->globals[i] = vm_create_object(vm);
		object_for_var(&vm->globals[i])->refcount = VM_REFCOUNT_NO_FREE;
	}

	// size_t n = 64 * 10000 * 10000;
	// char *buf = malloc(n); // TODO: free memory
	// arena_init(&vm->arena, buf, n);
}

void vm_register_c_function(VM *vm, const char *name, vm_CFunction callback)
{
    char lower[256];
	snprintf(lower, sizeof(lower), "%s", name);
	strtolower(lower);
	// hash_table_insert(&vm->c_functions, lower)->value = callback;
	hash_trie_upsert(&vm->c_functions, lower, vm->allocator)->value = callback;
}

static vm_CFunction get_c_function(VM *vm, const char *name)
{
    char lower[256];
	snprintf(lower, sizeof(lower), "%s", name);
	strtolower(lower);
	HashTrieNode *entry = hash_trie_upsert(&vm->c_functions, lower, NULL);
	// HashTableEntry *entry = hash_table_find(&vm->c_functions, lower);
	if(!entry)
		return NULL;
	return entry->value;
}

void vm_register_c_method(VM *vm, const char *name, vm_CMethod callback)
{
    char lower[256];
	snprintf(lower, sizeof(lower), "%s", name);
	strtolower(lower);
	hash_trie_upsert(&vm->c_methods, lower, vm->allocator)->value = callback;
}

static vm_CMethod get_c_method(VM *vm, const char *name)
{
    char lower[256];
	snprintf(lower, sizeof(lower), "%s", name);
	strtolower(lower);
	HashTrieNode *entry = hash_trie_upsert(&vm->c_methods, lower, NULL);
	if(!entry)
		return NULL;
	return entry->value;
}

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

Variable *vm_argv(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	int nargs = cast_int(vm, top(vm, 0));
	return &thr->stack[thr->sp - 3 - idx];
	// return thr->stack[thr->sp - nargs - 1 + idx];
}

size_t vm_argc(VM *vm)
{
	return cast_int(vm, top(vm, 0));
}

const char *vm_checkstring(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = stack_frame(vm, thr);
	Variable *arg = vm_argv(vm, idx);
	switch(arg->type)
	{
		case VAR_STRING: return arg->u.sval;
		case VAR_FLOAT:
		{
			char *str = new(&vm->c_function_arena, char, 32);
			snprintf(str, 32, "%.2f", arg->u.fval);
			return str;
		}
		break;
		case VAR_UNDEFINED: return "undefined";
		// case VAR_BOOLEAN: return arg->u.ival ? "true" : "false";
		case VAR_BOOLEAN: return arg->u.ival ? "1" : "0";
		case VAR_INTEGER:
		{
			char *str = new(&vm->c_function_arena, char, 32);
			snprintf(str, 32, "%d", arg->u.ival);
			return str;
		}
		break;
		default: vm_error(vm, "Not a string");
	}
	return "";
}

void vm_checkvector(VM *vm, int idx, float *outvec)
{
	Thread *thr = vm->thread;
	StackFrame *sf = stack_frame(vm, thr);
	Variable *arg = vm_argv(vm, idx);
	if(arg->type != VAR_VECTOR)
		vm_error(vm, "Not a vector");
	memcpy(outvec, arg->u.vval, sizeof(arg->u.vval));
}

float vm_checkfloat(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = stack_frame(vm, thr);
	Variable *arg = vm_argv(vm, idx);
	switch(arg->type)
	{
		case VAR_INTEGER: return (float)arg->u.ival;
		case VAR_FLOAT: return arg->u.fval;
		default: vm_error(vm, "Not a number");
	}
	return 0;
}

int vm_checkinteger(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = stack_frame(vm, thr);
	Variable *arg = vm_argv(vm, idx);
	if(arg->type != VAR_INTEGER)
		vm_error(vm, "Not a integer");
	return arg->u.ival;
}

void vm_pushstring_n(VM *vm, const char *str, size_t n)
{
	Variable v = var(vm);
	v.type = VAR_STRING;
	v.u.sval = malloc(n + 1);
	memcpy(v.u.sval, str, n);
	v.u.sval[n] = 0;
	push(vm, v);
}

void vm_pushstring(VM *vm, const char *str)
{
	Variable v = var(vm);
	v.type = VAR_STRING;
	v.u.sval = strdup(str);
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

void vm_pushobject(VM *vm, Object *o)
{
	Variable v = var(vm);
	v.type = VAR_OBJECT;
	v.u.oval = o;
	push(vm, v);
}

Thread *vm_thread(VM *vm)
{
	return vm->thread;
}

void vm_pushinteger(VM *vm, int val)
{
    Variable v = var(vm);
    v.type = VAR_INTEGER;
	v.u.ival = val;
    push(vm, v);
}

void vm_pushfloat(VM *vm, float val)
{
    Variable v = var(vm);
    v.type = VAR_FLOAT;
	v.u.fval = val;
    push(vm, v);
}


void vm_pushbool(VM *vm, bool b)
{
    Variable v = var(vm);
    v.type = VAR_BOOLEAN;
	v.u.ival = b ? 1 : 0;
    push(vm, v);
}

void vm_pushundefined(VM *vm)
{
    Variable v = var(vm);
    v.type = VAR_UNDEFINED;
    push(vm, v);
}

// TODO: make use of namespace
static void call_c_function(VM *vm, const char *namespace, const char *function, size_t nargs, int call_flags)
{
	if(call_flags & VM_CALL_FLAG_THREADED)
	{
		vm_error(vm, "Can't call builtin functions threaded");
	}
	Arena rollback = vm->c_function_arena;
	int nret;
	if(!(call_flags & VM_CALL_FLAG_METHOD))
	{
		vm_CFunction cfunc = get_c_function(vm, function);
		if(!cfunc)
		{
			vm_error(vm, "No builtin function '%s'", function);
		}
		nret = cfunc(vm);
	}
	else
	{
		vm_CMethod cmethod = get_c_method(vm, function);
		if(!cmethod)
		{
			vm_error(vm, "No builtin method '%s'", function);
		}
		Variable *self = vm_argv(vm, -1);
		if(self->type != VAR_OBJECT)
		{
			vm_error(vm, "not a object");
		}
		nret = cmethod(vm, object_for_var(self));
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

static bool call_function(VM *vm, Thread *thr, const char *file, const char *function, size_t nargs, bool reversed, int call_flags)
{
	VMFunction *vmf = vm->func_lookup(vm->ctx, file, function);
    if(!vmf)
    {
		call_c_function(vm, file, function, nargs, call_flags);
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
	sf->locals = NULL;
	// sf->self.u.oval = self ? self : prev_self;
	// sf->local_count = vmf->local_count;
	// sf->locals = new(&vm->arena, Variable, vmf->local_count);
	for(size_t i = 0; i < vmf->local_count; ++i)
	{
		buf_push(sf->locals, (Variable) { 0 });
		sf->locals[i].type = VAR_UNDEFINED;
		sf->locals[i].u.ival = 0;
	}
	pop_thread(vm, thr); //nargs
	sf->locals[0] = pop_thread(vm, thr);
	for(size_t i = 0; i < nargs; ++i)
	{
		Variable arg = pop_thread(vm, thr);
		// decref(vm, &arg);
		if(i < vmf->parameter_count)
		{
			size_t local_idx = reversed ? nargs - i - 1 : i;
			sf->locals[local_idx + 1] = arg;
		}
	}
	sf->file = file;
    sf->function = function;
    sf->instructions = vmf->instructions;
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

void vm_call_function_thread(VM *vm, const char *file, const char *function, size_t nargs, Variable *self)
{
	vm->thread = object_pool_allocate(&vm->pool.threads);
	memset(vm->thread, 0, sizeof(Thread));
	vm->thread->bp = 0;
	vm->thread->state = VM_THREAD_ACTIVE;
	push_thread(vm, vm->thread, self ? *self : vm->globals[VAR_GLOB_LEVEL]);
	push_thread(vm, vm->thread, integer(vm, nargs));
	if(self && self->type != VAR_OBJECT)
		vm_error(vm, "'%s' is not a object", variable_type_names[self->type]);
	call_function(vm, vm->thread, file, function, nargs, false, 0);
	add_thread(vm, vm->thread);
	vm->thread = NULL;
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

void vm_notify(VM *vm, Object *object, const char *key, size_t nargs)
{
	// TODO: args

	int name = vm_string_index(vm, key);
	if(name == -1)
	{
		vm_error(vm, "Can't find string '%s'", key);
	}
	VMEvent ev = { .object = object, .name = name, .arguments = NULL };
	if(vm->event_count >= VM_MAX_EVENTS_PER_FRAME)
		vm_error(vm, "event_count >= VM_MAX_EVENTS_PER_FRAME");
	vm->events[vm->event_count++] = ev;
	// buf_push(vm->events, ev);
}

static void run_thread(VM *vm)
{
	while(vm->thread->state == VM_THREAD_ACTIVE)
    {
		StackFrame *sf = stack_frame(vm, vm->thread);
		if(sf->ip >= buf_size(sf->instructions))
		{
			vm_error(vm, "ip oob %d/%d", sf->ip, buf_size(sf->instructions));
		}
	    Instruction *current = &sf->instructions[sf->ip++];
		if(!execute_instruction(vm, current))
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
		if(t->bp != -1)
			sf = &t->frames[t->bp - 1];
		// printf("Processing thread %s::%s (%s)\n", sf ? sf->file : "?", sf ? sf->function : "?", vm_thread_state_names[t->state]);
		// getchar();
		for(size_t j = 0; j < vm->event_count; j++)
		{
			VMEvent *ev = &vm->events[j];
			for(size_t k = 0; k < buf_size(t->endon); ++k)
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
				vm->thread = NULL;
			}
			break;

			case VM_THREAD_WAITING_EVENT:
			{
				for(size_t j = 0; j < vm->event_count; j++)
				{
					VMEvent *ev = &vm->events[j];
					if(ev->name == t->waittill.name && ev->object == t->waittill.object)
					{
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
	
	vm->event_count = 0; // Reset for next frame

	return vm->thread_read_idx != vm->thread_write_idx;
}