#include "visitor.h"
#include "lexer.h"
#include <setjmp.h>
#include "compiler.h"
#include "variable.h"
#include <inttypes.h>
#include <stdarg.h>
#include "include/gsc.h"

static void property(Compiler *c, ASTNode *n, int op);

static Node *node(Compiler *c, Node **list)
{
	Node *n = new(c->arena, Node, 1);
	n->next = *list; // Prepending, so it's actually previous
	*list = n;
	return n;
}

static Scope *current_scope(Compiler *c)
{
	return &c->scopes[c->current_scope];
}

static Scope *previous_scope(Compiler *c)
{
	if(c->current_scope == 0 || c->current_scope >= COMPILER_MAX_SCOPES)
		return NULL;
	return &c->scopes[c->current_scope - 1];
}

#include "stream.h"
#include "stream_buffer.h"

static void print_source(Compiler *c, int offset, int range_min, int range_max)
{
	if(!c->source || !c->node)
		return;
	Stream s = { 0 };
	StreamBuffer sb = { 0 };
	init_stream_from_buffer(&s, &sb, c->source, strlen(c->source) + 1);
	Stream *ls = &s;
	ls->seek(ls, offset + range_min, SEEK_SET);
	size_t n = range_max - range_min;
	for(int i = 0; i < n; ++i)
	{
		char ch;
		if(0 == ls->read(ls, &ch, 1, 1) || !ch)
			break;
		if(ls->tell(ls) == offset)
			putc('*', stdout);
		putc(ch, stdout);
	}
}

// static int lineno(Compiler *c)
// {
// 	if(!c->source || !c->node)
// 		return -1;
// 	size_t n = 1;
// 	for(size_t i = 0; i < c->node->offset && c->source[i]; ++i)
// 	{
// 		if(c->source[i] == '\n')
// 			++n;
// 	}
// 	return n;
// }

// TODO: FIXME
// Scans for newlines for almost every Node now
// cache it and return more accurate line info
// with more debug info I guess

// static void debug_info_node(Compiler *c, ASTNode *n)
// {
// 	c->node = n;
// 	c->line_number = lineno(c);
// }

static void strtolower(char *str)
{
	for(char *p = str; *p; ++p)
		*p = tolower(*p);
}

static const char *lowercase(Compiler *c, const char *s)
{
	snprintf(c->string, sizeof(c->string), "%s", s);
	strtolower(c->string);
	return c->string;
}

static void error(Compiler *c, const char *fmt, ...)
{
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	if(c->node)
		print_source(c, c->node->offset, -100, 100);
	printf("\n[COMPILER] ERROR: %s at line %d '%s'\n", message, c->node->line, c->path);
	if(c->jmp)
		longjmp(*c->jmp, 1);
	else
		abort();
}

static Operand string(Compiler *c, const char *str)
{
	return (Operand) { .type = OPERAND_TYPE_INDEXED_STRING, .value.string_index = string_table_intern(c->strings, str) };
}

static Operand integer(int64_t i)
{
	return (Operand) { .type = OPERAND_TYPE_INT, .value.integer = i };
}

static Operand number(float f)
{
	return (Operand) { .type = OPERAND_TYPE_FLOAT, .value.number = f };
}

static Operand NONE = { .type = OPERAND_TYPE_NONE };

static size_t emit_base(Compiler *c, Opcode op)
{
	if(c->instruction_count >= c->max_instruction_count)
		error(c, "Max instructions reached");
	Instruction *instr = &c->instructions[c->instruction_count];
	instr->opcode = op;
	instr->offset = c->instruction_count;
	instr->line = c->node->line;
	c->instruction_count++;
	return instr->offset;
}

static size_t emit_operands(Compiler *c, Opcode opcode, int operand_count, ...)
{
	size_t idx = emit_base(c, opcode);
	va_list args;
	va_start(args, operand_count);
	for(int i = 0; i < operand_count; i++) {
		c->instructions[idx].operands[i] = va_arg(args, Operand);
	}
	va_end(args);
	return idx;
}

#define emit(c, op) emit_base(c, op)
#define emit1(c, op, o1) emit_operands(c, op, 1, o1)
#define emit2(c, op, o1, o2) emit_operands(c, op, 2, o1, o2)
#define emit4(c, op, o1, o2, o3, o4) emit_operands(c, op, 4, o1, o2, o3, o4)

// Program counter
static int ip(Compiler *c)
{
	return c->instruction_count;
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

static void increment_scope(Compiler *c, ASTNode *node, Node *prev_continue_list)
{
	if(c->current_scope >= COMPILER_MAX_SCOPES)
	{
		error(c, "Max scope reached");
	}
	c->current_scope++;
	Scope *scope = current_scope(c);
	scope->node = node;
	// scope->arena = c->arena;
	scope->break_list = NULL;
	scope->continue_list = prev_continue_list ? prev_continue_list : NULL;
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
	if(continue_offset != -1)
	{
		LIST_FOREACH(Node, scope->continue_list, it)
		{
			size_t *offset = it->data;
			Instruction *ins = &c->instructions[*offset];

			ins->operands[0] = integer(continue_offset - *offset - 1);
		}
	} else
	{
		Scope *prev_scope = previous_scope(c);
		if(prev_scope)
		{
			prev_scope->continue_list = scope->continue_list;
		}
		else
		{
			error(c, "continue cannot be used in this context");
		}
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
		// Enables object.callback() instead of [[ object.callback ]]()
		// Force to call as method, since object object.callback() isn't valid
		case AST_MEMBER_EXPR:
		{
			// visit(n->ast_member_expr_data.object); // Moved to ASTCallExpr
			// call_flags |= VM_CALL_FLAG_METHOD;
			
			property(c, n->ast_member_expr_data.prop, n->ast_member_expr_data.op);
			visit(n->ast_member_expr_data.object);
			emit(c, OP_LOAD_FIELD);
			emit4(c, OP_CALL_PTR, NONE, NONE, integer(numarguments), integer(call_flags));
		}
		break;
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
static void visit_ASTBreakStmt_(Compiler *c, ASTBreakStmt *n)
{
	Scope *scope = current_scope(c);
	Node *break_entry = node(c, &scope->break_list);
	size_t *jmp = new(c->arena, size_t, 1);
	*jmp = emit(c, OP_JMP);
	break_entry->data = jmp;
}
static void visit_ASTContinueStmt_(Compiler *c, ASTContinueStmt *n)
{
	Scope *scope = current_scope(c);
	Node *continue_entry = node(c, &scope->continue_list);
	size_t *jmp = new(c->arena, size_t, 1);
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
	if(!n->duration)
	{
		emit(c, OP_UNDEF);
	} else
	{
		visit(n->duration);	
	}
	emit(c, OP_WAIT);
}

IMPL_VISIT(ASTArrayExpr)
{
	emit(c, OP_TABLE);
}

IMPL_VISIT(ASTStructExpr)
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
	increment_scope(c, (ASTNode*)n, NULL);
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
	size_t instr_idx = emit(c, OP_PUSH);
	Instruction *instr = &c->instructions[instr_idx];
	instr->operands[0] = integer(n->type);
	switch(n->type)
	{
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
			} else
			{
				instr->operands[2] = string(c, c->path);
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
IMPL_VISIT(ASTVectorExpr)
{
	for(size_t i = 0; i < n->numelements; ++i)
	{
		visit(n->elements[n->numelements - i - 1]);
	}
	emit1(c, OP_VECTOR, integer(n->numelements));
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

static bool is_expr(ASTNode *n)
{
	switch(n->type)
	{
		case AST_FUNCTION_POINTER_EXPR:
		case AST_ARRAY_EXPR:
		case AST_GROUP_EXPR:
		case AST_ASSIGNMENT_EXPR:
		case AST_BINARY_EXPR:
		case AST_CALL_EXPR:
		case AST_CONDITIONAL_EXPR:
		case AST_IDENTIFIER:
		case AST_LITERAL:
		case AST_MEMBER_EXPR:
		case AST_UNARY_EXPR:
		// case AST_SELF:
		case AST_VECTOR_EXPR: return true;
	}
	return false;
}

static void property(Compiler *c, ASTNode *n, int op)
{
	if(op == '[')
	{
		if(!is_expr(n))
			error(c, "Invalid node %s for property", ast_node_names[n->type]);
		visit(n);
		return;
	}
	switch(n->type)
	{
		case AST_IDENTIFIER:
		{
			emit4(c, OP_PUSH, integer(AST_LITERAL_TYPE_STRING), string(c, n->ast_identifier_data.name), NONE, NONE);
		}
		break;
		case AST_MEMBER_EXPR:
		{
			// lvalue(c, n->ast_member_expr_data.object);
			property(c, n->ast_member_expr_data.prop, n->ast_member_expr_data.op);
			visit(n->ast_member_expr_data.object);
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
				// case AST_LITERAL_TYPE_INTEGER:
				// {
				// 	emit4(c, OP_PUSH, integer(lit->type), integer(lit->value.integer), NONE, NONE);
				// }
				// break;
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

static int define_local_variable(Compiler *c, const char *name, bool is_parm)
{
	Allocator allocator = arena_allocator(c->arena);
	HashTrieNode *entry =
		hash_trie_upsert(&c->variables, lowercase(c, name), &allocator, false); // This is OK, Hash Trie doesn't store the allocator
	if(entry->value)
	{
		if(is_parm)
			error(c, "Parameter '%s' already defined", name);
		return *(int *)entry->value;
	}
	entry->value = new(c->arena, int, 1);
	*(int *)entry->value = c->variable_index++;
	return c->variable_index - 1;
}

static void identifier(Compiler *c, ASTNode *n)
{
	HashTrieNode *entry = NULL;
	if(c->globals)
		entry = hash_trie_upsert(c->globals, n->ast_identifier_data.name, NULL, false);
	if(!entry)
	{
		int idx = define_local_variable(c, n->ast_identifier_data.name, false);
		emit4(c, OP_REF, integer(idx), NONE, NONE, NONE);
	}
	else
	{
		// emit2(c, OP_GLOBAL, string(c, n->ast_identifier_data.name), integer(1));
		property(c, n, '.');
		emit1(c, OP_GLOBAL, integer(1));
		emit(c, OP_FIELD_REF);
	}
}

static void lvalue(Compiler *c, ASTNode *n)
{
	switch(n->type)
	{
		case AST_SELF:
		{
			emit4(c, OP_REF, integer(0), NONE, NONE, NONE);
		}
		break;

		case AST_IDENTIFIER:
		{
			identifier(c, n);
		}
		break;
		case AST_MEMBER_EXPR:
		{
			property(c, n->ast_member_expr_data.prop, n->ast_member_expr_data.op);
			lvalue(c, n->ast_member_expr_data.object);
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

IMPL_VISIT(ASTSwitchStmt)
{
	Scope *prev_scope = previous_scope(c);
	increment_scope(c, (ASTNode*)n, prev_scope ? prev_scope->continue_list : NULL);

	ASTSwitchCase *default_case = NULL;
	size_t case_jnz[256]; // TODO: increase limit
	size_t numcases = 0;

	for(ASTSwitchCase *it = n->cases; it; it = (ASTSwitchCase *)((ASTNode *)it)->next)
	{
		if(((ASTNode *)it)->type != AST_SWITCH_CASE)
			error(c, "Expected case got '%s'", ast_node_names[((ASTNode *)it)->type]);

		if(!it->test)
		{
			default_case = it;
			continue;
		}
		
		visit(n->discriminant);
		visit(it->test);
		emit4(c, OP_BINOP, integer(TK_EQUAL), NONE, NONE, NONE);
		emit(c, OP_TEST);
		size_t jnz = emit(c, OP_JNZ);
		if(numcases >= 256)
			error(c, "Maximum amount of cases is 256");
		case_jnz[numcases++] = jnz;
	}
	int jmp_default = -1;
	if(default_case)
	{
		jmp_default = emit(c, OP_JMP);
	}
	
	numcases = 0;

	for(ASTSwitchCase *it = n->cases; it; it = (ASTSwitchCase *)((ASTNode *)it)->next)
	{
		if(!it->test)
		{
			continue;
		}
		patch_reljmp(c, case_jnz[numcases++]);
		for(ASTNode *consequent = it->consequent; consequent; consequent = consequent->next)
		{
			visit(consequent);
		}
	}
	if(default_case)
	{
		patch_reljmp(c, jmp_default);
		for(ASTNode *consequent = default_case->consequent; consequent; consequent = consequent->next)
		{
			visit(consequent);
		}
	}
	decrement_scope(c, -1, ip(c));
}

IMPL_VISIT(ASTForStmt)
{
	if(n->init)
	{
		visit(n->init);
		emit(c, OP_POP);
	}
	increment_scope(c, (ASTNode*)n, NULL);
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
			} else
			{
				visit(n->argument);
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
			if(!n->prefix)
			{
				emit(c, OP_POP);
			}
			// visit(n->argument);
		}
		break;
		default: error(c, "Unsupported operator %d for unary expression", n->op); break;
	}
}
IMPL_VISIT(ASTMemberExpr)
{
	property(c, n->prop, n->op);
	visit(n->object);
	emit(c, OP_LOAD_FIELD);
}

IMPL_VISIT(ASTSelf)
{
	emit4(c, OP_LOAD, integer(0), NONE, NONE, NONE);
}

IMPL_VISIT(ASTIdentifier)
{
	HashTrieNode *entry = NULL;
	if(c->globals)
		entry = hash_trie_upsert(c->globals, n->name, NULL, false);
	if(!entry)
	{
		entry = hash_trie_upsert(&c->variables, lowercase(c, n->name), NULL, false);
		if(!entry)
			error(c, "No variable '%s'", n->name);
		emit4(c, OP_LOAD, integer(*(int *)entry->value), NONE, NONE, NONE);
	}
	else
	{
		// emit2(c, OP_GLOBAL, string(c, n->name), integer(0));
		property(c, n, '.');
		emit1(c, OP_GLOBAL, integer(0));
		emit(c, OP_LOAD_FIELD);
	}
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
		visit(n->lhs);
		visit(n->rhs);
		assert(n->op != TK_LOGICAL_AND && n->op != TK_LOGICAL_OR);
		emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
		lvalue(c, n->lhs);
		emit(c, OP_STORE);
		// visit(n->lhs);
	}
}
IMPL_VISIT(ASTBinaryExpr)
{
	switch(n->op)
	{
		case TK_LOGICAL_AND:
		{
			visit(n->lhs);
			emit(c, OP_TEST);
			int jz = emit(c, OP_JZ);

			emit(c, OP_CONST_1);
			visit(n->rhs);

			emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
			int jmp = emit(c, OP_JMP);
			
			patch_reljmp(c, jz);
			emit(c, OP_CONST_0);
			patch_reljmp(c, jmp);
		}
		break;
		case TK_LOGICAL_OR:
		{
			visit(n->lhs);
			emit(c, OP_TEST);
			int jnz = emit(c, OP_JNZ);

			emit(c, OP_CONST_0);
			visit(n->rhs);

			emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
			int jmp = emit(c, OP_JMP);
			
			patch_reljmp(c, jnz);
			emit(c, OP_CONST_1);
			patch_reljmp(c, jmp);
		}
		break;
		default:
		{
			visit(n->lhs);
			visit(n->rhs);
			emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
		}
		break;
	}
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
		if(pass_args_as_ref && n->numarguments - 1 != i)
		{
			identifier(c, n->arguments[n->numarguments - i - 1]);
			// lvalue(c, n->arguments[n->numarguments - i - 1]);
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
	if(n->object)
	{
		visit(n->object);
		// lvalue(c, n->object);
	}
	else
	{
		if(n->callee->type == AST_MEMBER_EXPR)
		{
			visit(n->callee->ast_member_expr_data.object);
			call_flags |= VM_CALL_FLAG_METHOD;
		}
		else
		{
			emit4(c, OP_LOAD, integer(0), NONE, NONE, NONE); // put "previous" / current local variable self on stack
		}
	}
	emit2(c, OP_PUSH, integer(AST_LITERAL_TYPE_INTEGER), integer(n->numarguments));
	callee(c, n->callee, call_flags, n->numarguments);
}
IMPL_VISIT(ASTExprStmt)
{
	c->node = (ASTNode*)n;
	// debug_info_node(c, (ASTNode*)n);
	visit(n->expression);
	if(c->flags & GSC_COMPILE_FLAG_PRINT_EXPRESSION)
	{
		emit(c, OP_PRINT_EXPR);
	} else
	{
		emit(c, OP_POP);
	}
}
IMPL_VISIT(ASTBlockStmt)
{
	ASTNode **it = (ASTNode**)&n->body;
	while(*it)
	{
		c->node = (ASTNode*)*it;
		// debug_info_node(c, *it);
		visit(*it);
		it = &((*it)->next);
	}
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
					printf("%s ", string_table_get(c->strings, instr->operands[i].value.string_index));
					break;
				case OPERAND_TYPE_INT:
					printf("%" PRId64" ", instr->operands[i].value.integer);
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

// void dump_instructions(Compiler *c, Instruction *instructions)
// {
// 	for(size_t i = 0; i < buf_size(instructions); ++i)
// 	{
// 		Instruction *instr = &instructions[i];
// 		print_instruction(c, instr);
// 	}
// }
IMPL_VISIT(ASTFunction)
{
	error(c, "Nested functions are not supported");
}

int compile_node(Instruction *instructions,
				 int max_instruction_count,
				 Compiler *c,
				 Arena temp,
				 ASTNode *n,
				 jmp_buf *jmp,
				 StringTable *strtab, HashTrie *globals)
{
	hash_trie_init(&c->variables);
	c->globals = globals;
	c->arena = &temp;
	c->strings = strtab;
	c->flags = 0;
	c->jmp = jmp;
	c->source = NULL;
	c->path = NULL;
	c->variable_index = 0;
	c->instructions = instructions;
	c->instruction_count = 0;
	c->max_instruction_count = max_instruction_count;
	c->current_scope = 0;
	c->node = (ASTNode*)n;
	visit(n);
	c->arena = NULL;
	return c->instruction_count;
}


int compile_function(Compiler *c, Arena *perm, Arena temp, ASTFunction *n, int *local_count, CompiledFunction *cf)
{
	hash_trie_init(&c->variables);
	c->arena = &temp;
	c->variable_index = 0;
	c->current_scope = 0;
	c->instruction_count = 0;
	c->max_instruction_count = MAX_INSTRUCTIONS;

	// debug_info_node(c, (ASTNode*)n);
	c->node = (ASTNode*)n;
	cf->line = c->node->line;
	define_local_variable(c, "self", true);
	for(ASTNode *it = n->parameters; it; it = it->next)
    {
		if(it->type != AST_IDENTIFIER)
			error(c, "Expected identifier");
		define_local_variable(c, it->ast_identifier_data.name, true);
    }
	visit(n->body);
	emit(c, OP_UNDEF);
	emit(c, OP_RET);
	*local_count = c->variable_index;
	cf->variable_names = new(perm, char*, c->variable_index);
	size_t idx = 0;
	for(HashTrieNode *it = c->variables.head; it; it = it->next)
	{
		size_t len = strlen(it->key) + 1;
		char *str = new(perm, char, len);
		memcpy(str, it->key, len);
		cf->variable_names[idx++] = str;
	}
	c->arena = NULL;
	return c->instruction_count;
}

// #include "vm.h"

// VMFunction *compile_function(Compiler *c, ASTFunction *n, Arena arena)
// {
// 	VMFunction *vmf = malloc(sizeof(VMFunction));
// 	jmp_buf *prev = c->jmp;
// 	jmp_buf jmp;
// 	if(setjmp(jmp))
// 	{
// 		// buf_free(c->instructions);
// 		// free(vmf);
// 		if(prev)
// 			longjmp(*prev, 1);
// 		return NULL;
// 	}
// 	c->arena = &arena;
// 	c->variable_index = 0;
// 	hash_trie_init(&c->variables);
// 	c->instructions = NULL;
// 	c->jmp = &jmp;
// 	c->current_scope = 0;
// 	vmf->parameter_count = n->parameter_count;
// 	debug_info_node(c, (ASTNode*)n);
// 	define_local_variable(c, "self", true);
// 	for(ASTNode *it = n->parameters; it; it = it->next)
//     {
// 		if(it->type != AST_IDENTIFIER)
// 			error(c, "Expected identifier");
// 		define_local_variable(c, it->ast_identifier_data.name, true);
//     }
// 	visit(n->body);
// 	vmf->local_count = c->variable_index;
// 	emit(c, OP_UNDEF);
// 	emit(c, OP_RET);

// 	vmf->instructions = c->instructions;
// 	c->instructions = NULL;
// 	c->arena = NULL;
// 	return vmf;
// }
