#include "visitor.h"
#include "lexer.h"
#include <setjmp.h>
#include <core/ds/hash_table.h>
#include "compiler.h"

static Node *node(Compiler *c, Node **list)
{
	Node *n = new(&c->arena, Node, 1);
	n->next = *list; // Prepending, so it's actually previous
	*list = n;
	return n;
}

static Scope *current_scope(Compiler *c)
{
	return &c->scopes[c->current_scope];
}

static int lineno(Compiler *c)
{
	if(!c->source || !c->node)
		return -1;
	size_t n = 1;
	for(size_t i = 0; i < c->node->offset && c->source[i]; ++i)
	{
		if(c->source[i] == '\n')
			++n;
	}
	return n;
}

// TODO: FIXME
// Scans for newlines for almost every Node now
// cache it and return more accurate line info
// with more debug info I guess

static void debug_info_node(Compiler *c, ASTNode *n)
{
	c->node = n;
	c->line_number = lineno(c);
}

static void error(Compiler *c, const char *fmt, ...)
{
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[COMPILER] ERROR: %s at line %d\n", message, lineno(c));
	// abort();
	if(c->jmp)
		longjmp(*c->jmp, 1);
}

static size_t intern(Compiler *c, const char *str)
{
	HashTableEntry *entry = hash_table_insert(&c->strings, str);
	if(entry->value)
	{
		return *(size_t*)entry->value;
	}
	entry->value = malloc(sizeof(size_t));
	*(size_t*)entry->value = c->string_index++;
	return c->string_index - 1;
}

static Operand string(Compiler *c, const char *str)
{
	return (Operand) { .type = OPERAND_TYPE_INDEXED_STRING, .value.string_index = intern(c, str) };
}

static Operand integer(int i)
{
	return (Operand) { .type = OPERAND_TYPE_INT, .value.integer = i };
}

static Operand number(float f)
{
	return (Operand) { .type = OPERAND_TYPE_FLOAT, .value.number = f };
}

static Operand NONE = { .type = OPERAND_TYPE_NONE };

static size_t emit(Compiler *c, Opcode op)
{
	Instruction instr = { .opcode = op, .offset = buf_size(c->instructions), .line = c->line_number };
	buf_push(c->instructions, instr);
	return buf_size(c->instructions) - 1;
}

static size_t emit1(Compiler *c, Opcode opcode, Operand operand1)
{
	size_t idx = emit(c, opcode);
	c->instructions[idx].operands[0] = operand1;
	return idx;
}

static size_t emit2(Compiler *c, Opcode opcode, Operand operand1, Operand operand2)
{
	size_t idx = emit(c, opcode);
	c->instructions[idx].operands[0] = operand1;
	c->instructions[idx].operands[1] = operand2;
	return idx;
}

static size_t emit4(Compiler *c, Opcode opcode, Operand operand1, Operand operand2, Operand operand3, Operand operand4)
{
	size_t idx = emit(c, opcode);
	c->instructions[idx].operands[0] = operand1;
	c->instructions[idx].operands[1] = operand2;
	c->instructions[idx].operands[2] = operand3;
	c->instructions[idx].operands[3] = operand4;
	return idx;
}

// Program counter
static int ip(Compiler *c)
{
	return buf_size(c->instructions);
}

static size_t reljmp_(Compiler *c, Opcode opcode, int current, int destination)
{
	return emit1(c, opcode, integer(destination - current - 1));
}

static size_t reljmp_current(Compiler *c, Opcode opcode, int destination)
{
	return reljmp_(c, opcode, ip(c), destination);
}

static void patch_reljmp(Compiler *c, size_t ins)
{
	int destination = ip(c);
	int current = c->instructions[ins].offset;
	c->instructions[ins].operands[0] = integer(destination - current - 1);
}

static void increment_scope(Compiler *c)
{
	if(c->current_scope >= COMPILER_MAX_SCOPES)
	{
		error(c, "Max scope reached");
	}
	c->current_scope++;
	Scope *scope = current_scope(c);
	scope->arena = c->arena;
	scope->break_list = NULL;
	scope->continue_list = NULL;
}

static void decrement_scope(Compiler *c, int continue_offset, int break_offset)
{
	Scope *scope = current_scope(c);

	// Patch all breaks and continue statement jumps

	LIST_FOREACH(Node, scope->break_list, it)
	{
		size_t *offset = it->data;
		Instruction *ins = &c->instructions[*offset];

		ins->operands[0] = integer(break_offset - *offset - 1);
	}

	LIST_FOREACH(Node, scope->continue_list, it)
	{
		size_t *offset = it->data;
		Instruction *ins = &c->instructions[*offset];

		ins->operands[0] = integer(continue_offset - *offset - 1);
	}

	if(c->current_scope-- <= 0)
	{
		error(c, "Scope error");
	}
}

typedef void (*VisitorFn)(Compiler *, void *);
#define IMPL_VISIT(TYPE) static void visit_ ## TYPE ## _(Compiler *c, TYPE *n)
#define AST_VISITORS(UPPER, TYPE, LOWER) [UPPER] = (VisitorFn)visit_ ## TYPE ## _,
#define FWD(UPPER, TYPE, LOWER) static void visit_##TYPE##_(Compiler *c, TYPE *n);
AST_X_MACRO(FWD)

static const VisitorFn ast_visitors[] = {
    AST_X_MACRO(AST_VISITORS) NULL
};
static void visit_(Compiler* c, ASTNode *n)
{
	if(!n)
	{
		error(c, "Node is null");
	}
	ast_visitors[n->type](c, n);
}
#define visit(x) visit_(c, x)

static void callee(Compiler *c, ASTNode *n, int call_flags, int numarguments)
{
	switch(n->type)
	{
		case AST_IDENTIFIER:
		{
			emit4(c, OP_CALL, string(c, n->ast_identifier_data.name), NONE, integer(numarguments), integer(call_flags));
		}
		break;
		case AST_FUNCTION_POINTER_EXPR:
		{
			visit(n->ast_function_pointer_expr_data.expression);
			emit4(c, OP_CALL_PTR, NONE, NONE, integer(numarguments), integer(call_flags));
		}
		break;
		case AST_LITERAL:
		{
			ASTLiteral *lit = &n->ast_literal_data;
			if(lit->type != AST_LITERAL_TYPE_FUNCTION)
				error(c, "Not a function");
			if(lit->value.function.file->type != AST_FILE_REFERENCE)
				error(c, "Not a file reference");
			if(lit->value.function.function->type != AST_IDENTIFIER)
				error(c, "Not a identifier");
			emit4(c,
				  OP_CALL,
				  string(c, lit->value.function.function->ast_identifier_data.name),
				  string(c, lit->value.function.file->ast_file_reference_data.file),
				  integer(numarguments),
				  integer(call_flags));
		}
		break;

		default:
		{
			error(c, "Unsupported callee %s", ast_node_names[n->type]);
		}
		break;
	}
}

static void visit_ASTConditionalExpr_(Compiler *c, ASTConditionalExpr *n)
{
	error(c,
		  "ASTConditionalExpr"
		  " unimplemented");
}
static void visit_ASTFileReference_(Compiler *c, ASTFileReference *n)
{
	error(c,
		  "ASTFileReference"
		  " unimplemented");
}
static void visit_ASTVectorExpr_(Compiler *c, ASTVectorExpr *n)
{
	// error(c,
	// 	  "ASTVectorExpr"
	// 	  " unimplemented");
}
static void visit_ASTBreakStmt_(Compiler *c, ASTBreakStmt *n)
{
	Scope *scope = current_scope(c);
	Node *break_entry = node(c, &scope->break_list);
	size_t *jmp = new(&c->arena, size_t, 1);
	*jmp = emit(c, OP_JMP);
	break_entry->data = jmp;
}
static void visit_ASTContinueStmt_(Compiler *c, ASTContinueStmt *n)
{
	Scope *scope = current_scope(c);
	Node *continue_entry = node(c, &scope->continue_list);
	size_t *jmp = new(&c->arena, size_t, 1);
	*jmp = emit(c, OP_JMP);
	continue_entry->data = jmp;
}
static void visit_ASTDoWhileStmt_(Compiler *c, ASTDoWhileStmt *n)
{
	error(c,
		  "ASTDoWhileStmt"
		  " unimplemented");
}
static void visit_ASTSwitchCase_(Compiler *c, ASTSwitchCase *n)
{
	error(c,
		  "ASTSwitchCase"
		  " unimplemented");
}
static void visit_ASTSwitchStmt_(Compiler *c, ASTSwitchStmt *n)
{
	// error(c,
	// 	  "ASTSwitchStmt"
	// 	  " unimplemented");
}
static void visit_ASTWaitTillFrameEndStmt_(Compiler *c, ASTWaitTillFrameEndStmt *n)
{
	error(c,
		  "ASTWaitTillFrameEndStmt"
		  " unimplemented");
}

IMPL_VISIT(ASTFunctionPointerExpr)
{
	error(c, "Must be used within calling context");
}

IMPL_VISIT(ASTWaitStmt)
{
	visit(n->duration);
	emit(c, OP_WAIT);
}

IMPL_VISIT(ASTArrayExpr)
{
	emit(c, OP_TABLE);
}

IMPL_VISIT(ASTEmptyStmt)
{
}

IMPL_VISIT(ASTReturnStmt)
{
	if(n->argument)
	{
		visit(n->argument);
	} else
	{
		emit(c, OP_UNDEF);
	}
	emit(c, OP_RET);
}

IMPL_VISIT(ASTWhileStmt)
{
	increment_scope(c);
	int loop_begin = ip(c);
	if(n->test)
	{
		visit(n->test);
	}
	else
	{
		emit(c, OP_CONST_1);
	}
	
	emit(c, OP_TEST);
	size_t jz = emit(c, OP_JZ);

	visit(n->body);

	int continue_offset = ip(c);

	reljmp_current(c, OP_JMP, loop_begin);
	patch_reljmp(c, jz);
	int break_offset = ip(c);

	decrement_scope(c, continue_offset, break_offset);
}

IMPL_VISIT(ASTLiteral)
{
	// static const char *_[] = {

	// 	"STRING", "INTEGER", "BOOLEAN", "FLOAT", "VECTOR", "ANIMATION", "FUNCTION", "LOCALIZED_STRING", "UNDEFINED"
	// };

	size_t instr_idx = emit(c, OP_PUSH);
	Instruction *instr = &c->instructions[instr_idx];
	instr->operands[0] = integer(n->type);
	switch(n->type)
	{
		case AST_LITERAL_TYPE_ANIMATION:
		case AST_LITERAL_TYPE_LOCALIZED_STRING:
		case AST_LITERAL_TYPE_STRING:
		{
			instr->operands[1] = string(c, n->value.string);
		}
		break;
		case AST_LITERAL_TYPE_BOOLEAN:
		{
			instr->operands[1] = integer(n->value.boolean);
		}
		break;
		case AST_LITERAL_TYPE_INTEGER:
		{
			instr->operands[1] = integer(n->value.integer);
		}
		break;
		case AST_LITERAL_TYPE_UNDEFINED:
		{
			instr->operands[1] = NONE;
		}
		break;
		case AST_LITERAL_TYPE_FLOAT:
		{
			instr->operands[1] = number(n->value.number);
		}
		break;
		case AST_LITERAL_TYPE_FUNCTION:
		{
			if(n->value.function.file)
			{
				if(n->value.function.file->type != AST_FILE_REFERENCE)
					error(c, "Not a file reference");
				instr->operands[2] = string(c, n->value.function.file->ast_file_reference_data.file);
			}
			if(n->value.function.function->type != AST_IDENTIFIER)
				error(c, "Not a function identifier");
			instr->operands[1] = string(c, n->value.function.function->ast_identifier_data.name);
		}
		break;

		default:
		{
			error(c, "Unhandled literal %d", n->type);
		}
		break;
	}
}

IMPL_VISIT(ASTIfStmt)
{
	visit(n->test);
	emit(c, OP_TEST);
	size_t jz = emit(c, OP_JZ);
	visit(n->consequent);
	if (n->alternative)
	{
		size_t jmp = emit(c, OP_JMP);
		patch_reljmp(c, jz);
		visit(n->alternative);
		patch_reljmp(c, jmp);
	}
	else
	{
		patch_reljmp(c, jz);
	}
}

static void property(Compiler *c, ASTNode *n)
{
	switch(n->type)
	{
		case AST_IDENTIFIER:
		{
			emit4(c, OP_PUSH, integer(AST_LITERAL_TYPE_STRING), string(c, n->ast_identifier_data.name), NONE, NONE);
		}
		break;
		case AST_MEMBER_EXPR:
		{
			visit(n->ast_member_expr_data.object);
			// lvalue(c, n->ast_member_expr_data.object);
			property(c, n->ast_member_expr_data.prop);
			emit(c, OP_LOAD_FIELD);
		}
		break;
		case AST_LITERAL:
		{
			ASTLiteral *lit = &n->ast_literal_data;
			switch(lit->type)
			{
				case AST_LITERAL_TYPE_STRING:
				{
					emit4(c, OP_PUSH, integer(lit->type), string(c, lit->value.string), NONE, NONE);
				}
				break;
				case AST_LITERAL_TYPE_INTEGER:
				{
					emit4(c, OP_PUSH, integer(lit->type), integer(lit->value.integer), NONE, NONE);
				}
				break;
				default:
				{
					error(c, "Invalid literal %d for for property", lit->type);
				}
				break;
			}
		}
		break;
		default:
		{
			error(c, "Invalid node %s for property", ast_node_names[n->type]);
		}
		break;
	}
}

static void lvalue(Compiler *c, ASTNode *n)
{
	switch(n->type)
	{
		// TODO: handle level, self, game[""] ...
		case AST_IDENTIFIER:
		{
			if(!strcmp(n->ast_identifier_data.name, "level"))
			{
				emit(c, OP_LEVEL);
			}
			else if(!strcmp(n->ast_identifier_data.name, "self"))
			{
				emit(c, OP_SELF);
			}
			else if(!strcmp(n->ast_identifier_data.name, "game"))
			{
				emit(c, OP_GAME);
			}
			else
			{
				HashTableEntry *entry = hash_table_find(&c->variables, n->ast_identifier_data.name);
				if(!entry)
				{
					entry = hash_table_insert(&c->variables, n->ast_identifier_data.name);
					entry->integer = c->variable_index++;
				}
				emit4(c, OP_REF, integer(entry->integer), NONE, NONE, NONE);
			}
		}
		break;
		case AST_MEMBER_EXPR:
		{
			lvalue(c, n->ast_member_expr_data.object);
			property(c, n->ast_member_expr_data.prop);
			emit(c, OP_FIELD_REF);
		}
		break;
		
		default:
		{
			error(c, "Invalid lvalue for %s", ast_node_names[n->type]);
		}
		break;
	}
}

IMPL_VISIT(ASTForStmt)
{
	if(n->init)
	{
		visit(n->init);
		emit(c, OP_POP);
	}
	increment_scope(c);
	int loop_begin = ip(c);
	if(n->test)
	{
		visit(n->test);
	}
	else
	{
		emit(c, OP_CONST_1);
	}
	
	emit(c, OP_TEST);
	size_t jz = emit(c, OP_JZ);

	visit(n->body);

	int continue_offset = ip(c);
	if(n->update)
	{
		visit(n->update);
		emit(c, OP_POP);
	}

	reljmp_current(c, OP_JMP, loop_begin);
	patch_reljmp(c, jz);
	int break_offset = ip(c);

	decrement_scope(c, continue_offset, break_offset);
}
IMPL_VISIT(ASTGroupExpr)
{
	visit(n->expression);
}
IMPL_VISIT(ASTUnaryExpr)
{
	switch (n->op)
	{
		case '-':
		{
			visit(n->argument);
			emit(c, OP_CONST_0);
			emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
		}
		break;
		case '!':
		case '~':
		{
			visit(n->argument);
			emit4(c, OP_UNARY, integer(n->op), NONE, NONE, NONE);
		}
		break;
		case TK_INCREMENT:
		case TK_DECREMENT:
		{
			if(n->prefix)
			{
				error(c, "Unsupported prefix operator -- or ++");
			}
			visit(n->argument);
			emit(c, OP_CONST_1);
			if(n->op == TK_INCREMENT)
			{
				emit4(c, OP_BINOP, integer('+'), NONE, NONE, NONE);
			}
			else
			{
				emit4(c, OP_BINOP, integer('-'), NONE, NONE, NONE);
			}
			
			lvalue(c, n->argument);
			emit(c, OP_STORE);
			// visit(n->argument);
		}
		break;
		default: error(c, "Unsupported operator %d for unary expression", n->op); break;
	}
}
IMPL_VISIT(ASTMemberExpr)
{
	visit(n->object);
	property(c, n->prop);
	emit(c, OP_LOAD_FIELD);
}
IMPL_VISIT(ASTIdentifier)
{
	if(!strcmp(n->name, "level"))
	{
		emit(c, OP_LEVEL);
	}
	else if(!strcmp(n->name, "self"))
	{
		emit(c, OP_SELF);
	}
	else if(!strcmp(n->name, "game"))
	{
		emit(c, OP_GAME);
	}
	else
	{
		HashTableEntry *entry = hash_table_find(&c->variables, n->name);
		if(!entry)
		{
			error(c, "No variable '%s'", n->name);
		}
		emit4(c, OP_LOAD, integer(entry->integer), NONE, NONE, NONE);
	}
	// emit4(c, OP_LOAD, string(c, n->name), NONE, NONE, NONE);
}
IMPL_VISIT(ASTAssignmentExpr)
{
	if(n->op == '=')
	{
		visit(n->rhs);
		lvalue(c, n->lhs);
		emit(c, OP_STORE);
		// visit(n->lhs);
	}
	else
	{
		visit(n->rhs);
		visit(n->lhs);
		
		emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
		lvalue(c, n->lhs);
		emit(c, OP_STORE);
		// visit(n->lhs);
	}
}
IMPL_VISIT(ASTBinaryExpr)
{
	visit(n->rhs);
	visit(n->lhs);
	emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
}

IMPL_VISIT(ASTCallExpr)
{
	bool pass_args_as_ref = false;
	if(n->callee->type == AST_IDENTIFIER)
	{
		if(!strcmp(n->callee->ast_identifier_data.name, "waittill"))
		{
			pass_args_as_ref = true;
		}
	}
	for(size_t i = 0; i < n->numarguments; ++i)
	{
		if(pass_args_as_ref && i > 0)
		{
			lvalue(c, n->arguments[n->numarguments - i - 1]);
		} else
		{
			visit(n->arguments[n->numarguments - i - 1]);
		}
	}
	int call_flags = 0;
	if(n->threaded)
		call_flags |= VM_CALL_FLAG_THREADED;
	if(n->object)
		call_flags |= VM_CALL_FLAG_METHOD;
	emit2(c, OP_PUSH, integer(AST_LITERAL_TYPE_INTEGER), integer(n->numarguments));
	if(n->object)
	{
		visit(n->object);
	}
	callee(c, n->callee, call_flags, n->numarguments);
}
IMPL_VISIT(ASTExprStmt)
{
	debug_info_node(c, n);
	visit(n->expression);
	emit(c, OP_POP);
}
IMPL_VISIT(ASTBlockStmt)
{
	ASTNode **it = &n->body;
	while(*it)
	{
		// debug_info_node(c, *it);
		visit(*it);
		it = &((*it)->next);
	}
}

void compiler_init(Compiler *c, jmp_buf *jmp, Arena arena)
{
	// c->out = fopen("compiled.gasm", "w");
	c->variable_index = 0;
	hash_table_init(&c->variables, 10);
	
	c->string_index = 0;
	hash_table_init(&c->strings, 16);
	c->arena = arena;

	c->jmp = jmp;
}

static void print_instruction(Compiler *c, Instruction *instr)
{
	printf("%d: %s ", instr->offset, opcode_names[instr->opcode]);
	for(size_t i = 0; i < MAX_OPERANDS; ++i)
	{
		if(instr->operands[i].type != OPERAND_TYPE_NONE)
		{
			switch(instr->operands[i].type)
			{
				case OPERAND_TYPE_INDEXED_STRING:
					printf("%s ", c->string_table[instr->operands[i].value.string_index]);
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

void dump_instructions(Compiler *c, Instruction *instructions)
{
	for(size_t i = 0; i < buf_size(instructions); ++i)
	{
		Instruction *instr = &instructions[i];
		print_instruction(c, instr);
	}
}
IMPL_VISIT(ASTFunction)
{
	error(c, "Nested functions are not supported");
}

#include "vm.h"

VMFunction *compile_function(Compiler *c, ASTFunction *n)
{
	VMFunction *vmf = malloc(sizeof(VMFunction));
	jmp_buf *prev = c->jmp;
	jmp_buf jmp;
	if(setjmp(jmp))
	{
		buf_free(c->instructions);
		free(vmf);
		if(prev)
			longjmp(*prev, 1);
		return NULL;
	}
	c->instructions = NULL;
	c->jmp = &jmp;
	c->variable_index = 0;
	hash_table_clear(&c->variables);
	vmf->parameter_count = n->parameter_count;
	debug_info_node(c, n);
	for(ASTNode *it = n->parameters; it; it = it->next)
    {
		if(it->type != AST_IDENTIFIER)
			error(c, "Expected identifier");
		HashTableEntry *entry = hash_table_find(&c->variables, it->ast_identifier_data.name);
		if(entry)
		{
			error(c, "Parameter '%s' already defined", it->ast_identifier_data.name);
		}
		entry = hash_table_insert(&c->variables, it->ast_identifier_data.name);
		entry->integer = c->variable_index++;
    }
	visit(n->body);
	vmf->local_count = c->variable_index;
	emit(c, OP_UNDEF);
	emit(c, OP_RET);

	vmf->instructions = c->instructions;
	c->instructions = NULL;
	return vmf;
}