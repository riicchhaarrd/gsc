#include "vm.h"
#include <math.h>
#include "lexer.h"
#include "ast.h"

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

static Variable undef = { .type = VAR_UNDEFINED, .refcount = 0xdeadbeef };

static void strtolower(char *str)
{
	for(char *p = str; *p; ++p)
		*p = tolower(*p);
}

static void vm_stacktrace(VM *vm)
{
	// TODO: print instructions

    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    // printf("========= STACK =========\n");
	printf("\t== sp: %d\n", thr->sp);
	for(int i = thr->sp - 1; i >= 0; i--)
    {
		Variable *sv = thr->stack[i];
		if(!sv)
			continue;
		printf("\t== %d: type:%s, refcount:%d\n", i, variable_type_names[sv->type], sv->refcount);
	}
    // printf("====== END OF STACK =====\n");
}

#ifndef COUNT_OF(x)
	#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

static const char *stringify(VM * vm, Variable * v, char *buf, size_t n);
static void print_locals(VM *vm)
{
    Thread *thr = vm->thread;
	StackFrame *sf = thr->frame;
	printf(" || Local variables ||\n");
	char str[1024];
	for(size_t i = 0; i < COUNT_OF(sf->locals); ++i)
	{
		Variable *lv = &sf->locals[i];
		if(lv->type == VAR_UNDEFINED)
			continue;
		stringify(vm, lv, str, sizeof(str));
		printf("\t|| local %d (%s): %s\n", i, variable_type_names[lv->type], str);
	}
}

static void vm_error(VM *vm, const char *fmt, ...)
{
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[VM] ERROR: %s\n", message);
	// abort();
	vm_stacktrace(vm);
	if(vm->jmp)
		longjmp(*vm->jmp, 1);
}

static const char *string(VM *vm, size_t idx)
{
    return vm->string_table[idx];
}

static void incref(VM *vm, Variable **v)
{
	if((*v)->refcount == 0xdeadbeef)
		return;
	(*v)->refcount++;
}

static void decref(VM *vm, Variable **v)
{
	if(!v || !*v)
		return;
	if((*v)->refcount == 0xdeadbeef)
		return;
	if(--(*v)->refcount <= 0)
    {
        free(*v);
        *v = NULL;
    }
}

static Variable *variable()
{
    Variable *var = malloc(sizeof(Variable));
	var->type = VAR_UNDEFINED;
	var->refcount = 0;
	var->u.ival = 0;
    return var;
}

static Variable *dup(Variable *v)
{
	Variable *copy = variable();
	memcpy(copy, v, sizeof(Variable));
	copy->refcount = 0;
	return copy;
}

static void push(VM *vm, Variable *v)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;

    thr->stack[thr->sp++] = v;
}

static Variable *pop(VM *vm)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    Variable *top = thr->stack[--thr->sp];
    // Variable ret = *top;
    // decref(vm, &top);
    return top;
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
    StackFrame *sf = thr->frame;
    Variable *v = thr->stack[thr->sp - 1 - offset];
	return v;
}

static void pop_string(VM *vm, char *str, size_t n)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    Variable *top = thr->stack[--thr->sp];
    if(top->type != VAR_STRING)
		vm_error(vm, "'%s' is not a string", variable_type_names[top->type]);
	snprintf(str, n, "%s", top->u.sval);
	decref(vm, &top);
}

static int pop_int(VM *vm)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    Variable *top = thr->stack[--thr->sp];
    if(top->type != VAR_INTEGER)
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
	return vm->string_table[op->value.string_index];
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

static void print_operand(VM *vm, Operand *operand)
{
	if(operand->type == OPERAND_TYPE_NONE)
	{
		return;
	}
	switch(operand->type)
	{
		case OPERAND_TYPE_INDEXED_STRING: printf("%s ", vm->string_table[operand->value.string_index]); break;
		case OPERAND_TYPE_INT: printf("%d ", operand->value.integer); break;
		case OPERAND_TYPE_FLOAT: printf("%f ", operand->value.number); break;
	}
	// printf("%s ", operand_type_names[instr->operands[i].type]);
}

static void print_instruction(VM *vm, Instruction *instr)
{
	printf("%d: %s ", instr->offset, opcode_names[instr->opcode]);
	switch(instr->opcode)
	{
		case OP_PUSH:
		{
			static const char *_[] = {

				"STRING",	 "INTEGER",	 "BOOLEAN",			 "FLOAT",	 "VECTOR",
				"ANIMATION", "FUNCTION", "LOCALIZED_STRING", "UNDEFINED"
			};
			printf("%s ", _[instr->operands[0].value.integer]);
			if(instr->operands[0].value.integer == AST_LITERAL_TYPE_STRING)
			{
				printf("%s", string(vm, instr->operands[1].value.string_index));
			}
			else
			{
				print_operand(vm, &instr->operands[1]);
			}
		}
		break;
		default:
		{
			for(size_t i = 0; i < MAX_OPERANDS; ++i)
			{
				print_operand(vm, &instr->operands[i]);
			}
		}
		break;
	}
	printf("\n");
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
	if(lhs == VAR_VECTOR || rhs == VAR_VECTOR)
	{
		return VAR_VECTOR;
	}
	return lhs;
}

static const char *stringify(VM *vm, Variable *v, char *buf, size_t n)
{
	switch(v->type)
	{
		case VAR_UNDEFINED: return "undefined";
		case VAR_BOOLEAN: return v->u.ival == 0 ? "false" : "true";
		case VAR_FLOAT: snprintf(buf, n, "%.2f", v->u.fval); return buf;
		case VAR_INTEGER: snprintf(buf, n, "%d", v->u.ival); return buf;
		case VAR_LOCALIZED_STRING:
		case VAR_ANIMATION:
		case VAR_STRING: return v->u.sval;
		case VAR_OBJECT: return "[object]";
		case VAR_FUNCTION: return "[function]";
		case VAR_VECTOR: snprintf(buf, n, "(%.2f %.2f %.2f)", v->u.vval[0], v->u.vval[1], v->u.vval[2]);
	}
	return NULL;
}

static Variable coerce_int(VM *vm, Variable *v)
{
	Variable result = { .type = VAR_INTEGER };
	switch(v->type)
	{
		case VAR_INTEGER: result = *v; break;
		case VAR_FLOAT: result.u.ival = (int)v->u.fval; break;
		case VAR_STRING: result.u.ival = atoi(v->u.sval); break;
		default: vm_error(vm, "Cannot coerce '%s' to integer", variable_type_names[v->type]); break;
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

static Variable binop(VM *vm, Variable *lhs, Variable *rhs, int op)
{
	char temp[64];
	VariableType type = promote_type(lhs->type, rhs->type);
	Variable result = { .type = type };
	switch(type)
	{
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
				case TK_GEQUAL: *c = a >= b; break;
				case TK_LEQUAL: *c = a <= b; break;
				case TK_NEQUAL: *c = a != b; break;
				case TK_EQUAL: *c = a == b; break;
				case '>': *c = a > b; break;
				case '<': *c = a < b; break;
				case TK_LOGICAL_AND: *c = a && b; break;
				case TK_LOGICAL_OR: *c = a || b; break;
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
					result.type = VAR_INTEGER;
					result.u.ival = a >= b;
					break;
				case TK_NEQUAL:
					result.type = VAR_INTEGER;
					result.u.ival = a != b;
					break;
				case TK_EQUAL:
					result.type = VAR_INTEGER;
					result.u.ival = a == b;
					break;
				case TK_LEQUAL:
					result.type = VAR_INTEGER;
					result.u.ival = a <= b;
					break;
				case '>':
					result.type = VAR_INTEGER;
					result.u.ival = a > b;
					break;
				case '<':
					result.type = VAR_INTEGER;
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
		case VAR_STRING:
		{
			// TODO: FIXME
			static char a[4096];
			static char b[4096];
			stringify(vm, lhs, a, sizeof(a));
			stringify(vm, lhs, b, sizeof(b));
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
					result.type = VAR_INTEGER;
					result.u.ival = 0 == strcmp(a, b);
					break;
				case TK_NEQUAL:
					result.type = VAR_INTEGER;
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

		default:
		{
			vm_error(vm, "Unsupported operator '%s' for type '%s'", token_type_to_string(op, temp, sizeof(temp)), variable_type_names[type]);
		}
		break;
	}
	return result;
}

void vm_call_function(VM *vm, const char *file, const char *function, size_t nargs);
#define ASSERT_STACK(X)                                                              \
	do                                                                               \
	{                                                                                \
		if(thr->sp != sp + (X))                                                      \
			vm_error(vm, "Stack cookie failed for '%s'! Expected %d, got %d", opcode_names[ins->opcode], sp + (X), thr->sp); \
	} while(0)
static void vm_execute(VM *vm, Instruction *ins)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
	if(vm->flags & VM_FLAG_VERBOSE)
	{
    	print_instruction(vm, ins);
	}
	int sp = thr->sp;
    switch(ins->opcode)
    {
		case OP_POP:
		{
            Variable *v = pop(vm);
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

		case OP_GAME:
		{
			push(vm, &vm->game);
			ASSERT_STACK(1);
		}
		break;

		case OP_LEVEL:
		{
			push(vm, &vm->level);
			ASSERT_STACK(1);
		}
		break;

		case OP_SELF:
		{
			push(vm, &sf->self);
			ASSERT_STACK(1);
		}
		break;

		case OP_FIELD_REF:
		{
			char prop[256] = { 0 };
			pop_string(vm, prop, sizeof(prop));
			Variable *obj = pop(vm);
			if(obj->type != VAR_OBJECT)
			{
				vm_error(vm, "'%s' is not a object", variable_type_names[obj->type]);
			}
			Object *o = obj->u.oval;
			if(!o)
			{
				vm_error(vm, "object is null");
			}
			HashTableEntry *entry = hash_table_find(&o->fields, prop);
			if(!entry)
			{
				entry = hash_table_insert(&o->fields, prop);
				entry->value = variable();
			}
			push(vm, entry->value);
			ASSERT_STACK(-1);
		}
		break;

		case OP_LOAD_FIELD:
		{
			char prop[256] = { 0 };
			pop_string(vm, prop, sizeof(prop));
			Variable *obj = pop(vm);
			if(obj->type != VAR_OBJECT)
			{
				vm_error(vm, "'%s' is not a object", variable_type_names[obj->type]);
			}
			Object *o = obj->u.oval;
			if(!o)
			{
				vm_error(vm, "object is null");
			}
			HashTableEntry *entry = hash_table_find(&o->fields, prop);
			if(!entry)
			{
				push(vm, &undef);
			} else
			{
				push(vm, entry->value);
			}
			ASSERT_STACK(-1);
		}
		break;

		case OP_STORE:
		{
			Variable *dst = pop(vm);
			Variable *src = pop(vm);
			incref(vm, &src);
			// TODO: move
			if(dst->type == VAR_OBJECT)
			{
				free(dst->u.oval);
			}
			else if(dst->type == VAR_STRING)
			{
				free(dst->u.sval);
			}
			dst->type = src->type;
			memcpy(&dst->u, &src->u, sizeof(dst->u));
			// decref(vm, &dst);
			push(vm, dup(dst));
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
			Variable *lv = &sf->locals[slot];
			// lv->refcount = 0xdeadbeef;
			push(vm, dup(lv));
			ASSERT_STACK(1);
		}
		break;

		case OP_CONST_0:
		case OP_CONST_1:
		{
			Variable *v = variable();
			v->type = VAR_INTEGER;
			v->u.ival = ins->opcode == OP_CONST_1 ? 1 : 0;
			push(vm, v);
			ASSERT_STACK(1);
		}
		break;

		case OP_REF:
		{
			// TODO: check out of bounds
			// allocate dynamically
			int slot = read_int(vm, ins, 0);
			Variable *lv = &sf->locals[slot];
			// lv->refcount = 0xdeadbeef;
			push(vm, lv);
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
            Variable *v = variable();
			switch(var_type)
			{
				case AST_LITERAL_TYPE_FLOAT:
				{
					v->type = VAR_FLOAT;
					v->u.fval = read_float(vm, ins, 1);
				}
				break;
				case AST_LITERAL_TYPE_ANIMATION:
				{
					v->type = VAR_ANIMATION;
					v->u.sval = string(vm, read_string_index(vm, ins, 1));
				}
				break;
                case AST_LITERAL_TYPE_STRING:
				{
					v->type = VAR_STRING;
					v->u.sval = string(vm, read_string_index(vm, ins, 1));
				}
				break;
                case AST_LITERAL_TYPE_LOCALIZED_STRING:
				{
					v->type = VAR_LOCALIZED_STRING;
					v->u.sval = string(vm, read_string_index(vm, ins, 1));
				}
				break;
				case AST_LITERAL_TYPE_BOOLEAN:
				{
					v->type = VAR_INTEGER;
					v->u.ival = read_int(vm, ins, 1);
				}
				break;
				case AST_LITERAL_TYPE_INTEGER:
				{
					v->type = VAR_INTEGER;
					v->u.ival = read_int(vm, ins, 1);
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
		
        case OP_CALL:
		{
            const char *function = read_string(vm, ins, 0);
            const char *file = NULL;
            if(check_operand(ins, 1, OPERAND_TYPE_INDEXED_STRING))
                file = read_string(vm, ins, 1);
            // int nargs = pop_int(vm);
            int nargs = cast_int(vm, top(vm, 0));
            info(vm, "CALLING -> %s::%s %d\n", file, function, nargs);
            vm_call_function(vm, file ? file : sf->file, function, nargs);
			ASSERT_STACK(-nargs);
		}
		break;

		case OP_BINOP:
		{
			int op = read_int(vm, ins, 0);
			Variable *a = pop(vm);
			Variable *b = pop(vm);
			Variable *result = variable();
			*result = binop(vm, a, b, op);
			info(vm, "binop result: %d\n", result->u.ival);
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
		getchar();
	}
}

static Object *create_object()
{
	Object *o = calloc(1, sizeof(Object));
	hash_table_init(&o->fields, 10);
	return o;
}

VM *vm_create()
{
	VM *vm = calloc(1, sizeof(VM));
    hash_table_init(&vm->c_functions, 10);

	vm->level.refcount = 0xdeadbeef;
	vm->level.type = VAR_OBJECT;
	vm->level.u.oval = create_object();
	
	vm->game.refcount = 0xdeadbeef;
	vm->game.type = VAR_OBJECT;
	vm->game.u.oval = create_object();

	size_t n = 64 * 10000 * 10000;
	char *buf = malloc(n); // TODO: free memory
	arena_init(&vm->arena, buf, n);
	return vm;
}

void vm_register_c_function(VM *vm, const char *name, vm_CFunction callback)
{
    char lower[256];
	snprintf(lower, sizeof(lower), "%s", name);
	strtolower(lower);
	hash_table_insert(&vm->c_functions, lower)->value = callback;
}

vm_CFunction vm_get_c_function(VM *vm, const char *name)
{
    char lower[256];
	snprintf(lower, sizeof(lower), "%s", name);
	strtolower(lower);
	HashTableEntry *entry = hash_table_find(&vm->c_functions, lower);
	if(!entry)
		return NULL;
	return entry->value;
}

void vm_run(VM *vm)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
	for(;;)
    {
        if(sf->ip >= buf_size(sf->instructions))
            vm_error(vm, "ip oob");
	    Instruction *current = &sf->instructions[sf->ip++];
		vm_execute(vm, current);
    }
}

Variable *get_arg(VM *vm, size_t idx)
{
	Thread *thr = vm->thread;
	int nargs = cast_int(vm, top(vm, 0));
	return thr->stack[thr->sp - nargs - 1 + idx];
}

size_t vm_argc(VM *vm)
{
	return cast_int(vm, top(vm, 0));
}

const char *vm_checkstring(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = thr->frame;
	Variable *arg = get_arg(vm, idx);
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

float vm_checkfloat(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = thr->frame;
	Variable *arg = get_arg(vm, idx);
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
	StackFrame *sf = thr->frame;
	Variable *arg = get_arg(vm, idx);
	if(arg->type != VAR_INTEGER)
		vm_error(vm, "Not a integer");
	return arg->u.ival;
}

void vm_pushstring(VM *vm, const char *str)
{
    Variable *v = variable();
    v->type = VAR_STRING;
	v->u.sval = strdup(str);
	push(vm, v);
}

void vm_pushinteger(VM *vm, int val)
{
    Variable *v = variable();
    v->type = VAR_INTEGER;
	v->u.ival = val;
    push(vm, v);
}

void vm_call_function(VM *vm, const char *file, const char *function, size_t nargs)
{
	Instruction *ins = vm->func_lookup(vm->ctx, file, function);
    if(!ins)
    {
		vm->c_function_arena = vm->arena;

        vm_CFunction cfunc = vm_get_c_function(vm, function);
        if(!cfunc)
        {
            vm_error(vm, "No builtin function '%s'", function);
        }
        int nret = cfunc(vm);
        if(nret == 0)
        {
            push(vm, &undef);
        }
        Variable *ret = pop(vm);

        // Pop all args
        for(size_t i = 0; i < nargs + 1; ++i)
        {
			Variable *arg = pop(vm);
			decref(vm, &arg);
        }

        // Put ret back on stack
        push(vm, ret);
        return;
	}
    Thread *thr = vm->thread;
    if(!thr)
    {
        thr = calloc(1, sizeof(Thread));
        vm->thread = thr;        
    }
	thr->frame = &thr->frames[thr->bp++];
    StackFrame *sf = thr->frame;
	memset(sf->locals, 0, sizeof(sf->locals));
	for(size_t i = 0; i < COUNT_OF(sf->locals); ++i)
	{
		sf->locals[i].refcount = 0xdeadbeef;
	}
	sf->file = file;
    sf->function = function;
    sf->instructions = ins;
	for(size_t i = 0; i < buf_size(ins); ++i)
		print_instruction(vm, &ins[i]);
	getchar();
}