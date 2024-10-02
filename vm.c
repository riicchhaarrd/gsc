#include "vm.h"

static void strtolower(char *str)
{
	for(char *p = str; *p; ++p)
		*p = tolower(*p);
}

static void vm_error(VM *vm, const char *fmt, ...)
{
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[VM] ERROR: %s\n", message);
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    printf("========= STACK =========\n");
	printf("sp: %d\n", thr->sp);
	for(int i = thr->sp; i > 0; i--)
    {
        printf("%d: type:%d, refcount:%d\n", i, thr->stack[i]->type, thr->stack[i]->refcount);
    }
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
		vm_error(vm, "%d is not a integer", v->type);
	return v->u.ival;
}

static Variable *top(VM *vm, int offset)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    Variable *v = thr->stack[thr->sp - 1 - offset];
	return v;
}

static int pop_int(VM *vm)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    Variable *top = thr->stack[--thr->sp];
    if(top->type != VAR_INTEGER)
		vm_error(vm, "%d is not a integer", top->type);
    int i = top->u.ival;
    decref(vm, &top);
    return i;
}

static int read_int(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_INT)
		vm_error(vm, "%d is not a integer", op->type);
    return op->value.integer;
}

static int read_string_index(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_INDEXED_STRING)
		vm_error(vm, "%d is not a string", op->type);
	return op->value.string_index;
}

static const char* read_string(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_INDEXED_STRING)
		vm_error(vm, "%d is not a string", op->type);
	return vm->string_table[op->value.string_index];
}

static float read_float(VM *vm, Instruction *ins, size_t idx)
{
    Operand *op = &ins->operands[idx];
    if(op->type != OPERAND_TYPE_FLOAT)
		vm_error(vm, "%d is not a float", op->type);
    return op->value.number;
}

static bool check_operand(Instruction *ins, size_t idx, OperandType type)
{
    return ins->operands[idx].type == type;
}

static void print_instruction(VM *vm, Instruction *instr)
{
	printf("%d: %s ", instr->offset, opcode_names[instr->opcode]);
	for(size_t i = 0; i < MAX_OPERANDS; ++i)
	{
		if(instr->operands[i].type != OPERAND_TYPE_NONE)
		{
			switch(instr->operands[i].type)
			{
				case OPERAND_TYPE_INDEXED_STRING:
					printf("%s ", vm->string_table[instr->operands[i].value.string_index]);
					break;
				case OPERAND_TYPE_INT:
					printf("%d ", instr->operands[i].value.integer);
				break;
				case OPERAND_TYPE_FLOAT:
					printf("%f ", instr->operands[i].value.number);
				break;
			}
			// printf("%s ", operand_type_names[instr->operands[i].type]);
		}
	}
	printf("\n");
}

void vm_call_function(VM *vm, const char *file, const char *function, size_t nargs);

static void vm_execute(VM *vm, Instruction *ins)
{
    Thread *thr = vm->thread;
    StackFrame *sf = thr->frame;
    print_instruction(vm, ins);
    switch(ins->opcode)
    {
		case OP_POP:
		{
            Variable *v = pop(vm);
            decref(vm, &v);
		}
		break;

		case OP_PUSH:
		{
            int var_type = read_int(vm, ins, 0);
            Variable *v = variable();
            v->type = var_type;
			switch(var_type)
			{
				case VAR_FLOAT:
				{
					v->u.fval = read_float(vm, ins, 1);
				}
				break;
				case VAR_ANIMATION:
                case VAR_STRING:
                case VAR_LOCALIZED_STRING:
				{
					v->u.sval = string(vm, read_string_index(vm, ins, 1));
				}
				break;
				case VAR_INTEGER:
				{
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
            printf("CALLING -> %s::%s %d\n", file, function, nargs);
            vm_call_function(vm, file ? file : sf->file, function, nargs);
		}
		break;

		default:
		{
            vm_error(vm, "Opcode %s unhandled", opcode_names[ins->opcode]);
		}
		break;
	}
}
VM *vm_create()
{
	VM *vm = calloc(1, sizeof(VM));
    hash_table_init(&vm->c_functions, 10);
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

const char *vm_checkstring(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = thr->frame;
	Variable *top = thr->stack[thr->sp - idx - 1];
	switch(top->type)
	{
		case VAR_STRING: return top->u.sval;
		default: vm_error(vm, "Not a string");
	}
	return "";
}

float vm_checkfloat(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = thr->frame;
	Variable *top = thr->stack[thr->sp - idx - 1];
	switch(top->type)
	{
		case VAR_INTEGER: return (float)top->u.ival;
		case VAR_FLOAT: return top->u.fval;
		default: vm_error(vm, "Not a number");
	}
	return 0;
}

int vm_checkinteger(VM *vm, int idx)
{
	Thread *thr = vm->thread;
	StackFrame *sf = thr->frame;
	Variable *top = thr->stack[thr->sp - idx - 1];
	if(top->type != VAR_INTEGER)
		vm_error(vm, "Not a integer");
	return top->u.ival;
}

void vm_pushinteger(VM *vm, int val)
{
    Variable *v = variable();
    v->type = VAR_INTEGER;
	v->u.ival = val;
    push(vm, v);
}

static Variable undef = { .type = VAR_UNDEFINED, .refcount = 0xdeadbeef };

void vm_call_function(VM *vm, const char *file, const char *function, size_t nargs)
{
	Instruction *ins = vm->func_lookup(vm->ctx, file, function);
    if(!ins)
    {
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
    sf->file = file;
    sf->function = function;
    sf->instructions = ins;
}