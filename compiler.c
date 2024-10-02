#include "visitor.h"
#include "lexer.h"
#include <setjmp.h>
#include <core/ds/hash_table.h>
#include "compiler.h"

static void error(Compiler *c, const char *fmt, ...)
{
	char message[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);
	printf("[COMPILER] ERROR: %s", message);
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

static Instruction *emit(Compiler *c, Opcode op)
{
	Instruction instr = { .opcode = op, .offset = buf_size(c->instructions) };
	buf_push(c->instructions, instr);
	return &c->instructions[buf_size(c->instructions) - 1];
}

static Instruction *emit1(Compiler *c, Opcode opcode, Operand operand1)
{
	Instruction *instr = emit(c, opcode);
	instr->operands[0] = operand1;
	return instr;
}

static Instruction *emit2(Compiler *c, Opcode opcode, Operand operand1, Operand operand2)
{
	Instruction *instr = emit(c, opcode);
	instr->operands[0] = operand1;
	instr->operands[1] = operand2;
	return instr;
}

static Instruction *emit4(Compiler *c, Opcode opcode, Operand operand1, Operand operand2, Operand operand3, Operand operand4)
{
	Instruction *instr = emit(c, opcode);
	instr->operands[0] = operand1;
	instr->operands[1] = operand2;
	instr->operands[2] = operand3;
	instr->operands[3] = operand4;
	return instr;
}

static void callee(Compiler *c, ASTNode *n)
{
	switch(n->type)
	{
		case AST_IDENTIFIER:
		{
			emit4(c, OP_CALL, string(c, n->ast_identifier_data.name), NONE, NONE, NONE);
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
				  NONE,
				  NONE);
		}
		break;

		default:
		{
			error(c, "Unsupported callee %s", ast_node_names[n->type]);
		}
		break;
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

static void visit_ASTFunctionPointerExpr_(Compiler *c, ASTFunctionPointerExpr *n)
{
	error(c,
		  "ASTFunctionPointerExpr"
		  " unimplemented");
}
static void visit_ASTArrayExpr_(Compiler *c, ASTArrayExpr *n)
{
	// error(c,
	// 	  "ASTArrayExpr"
	// 	  " unimplemented");
}
static void visit_ASTGroupExpr_(Compiler *c, ASTGroupExpr *n)
{
	// error(c, "TODO");
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
static void visit_ASTMemberExpr_(Compiler *c, ASTMemberExpr *n)
{
	// error(c, "TODO");
}
static void visit_ASTVectorExpr_(Compiler *c, ASTVectorExpr *n)
{
	// error(c,
	// 	  "ASTVectorExpr"
	// 	  " unimplemented");
}
static void visit_ASTBreakStmt_(Compiler *c, ASTBreakStmt *n)
{
	error(c,
		  "ASTBreakStmt"
		  " unimplemented");
}
static void visit_ASTContinueStmt_(Compiler *c, ASTContinueStmt *n)
{
	error(c,
		  "ASTContinueStmt"
		  " unimplemented");
}
static void visit_ASTDoWhileStmt_(Compiler *c, ASTDoWhileStmt *n)
{
	error(c,
		  "ASTDoWhileStmt"
		  " unimplemented");
}
static void visit_ASTEmptyStmt_(Compiler *c, ASTEmptyStmt *n)
{
	error(c,
		  "ASTEmptyStmt"
		  " unimplemented");
}
static void visit_ASTForStmt_(Compiler *c, ASTForStmt *n)
{
	// error(c, "TODO");
}
static void visit_ASTReturnStmt_(Compiler *c, ASTReturnStmt *n)
{
	// error(c,
	// 	  "ASTReturnStmt"
	// 	  " unimplemented");
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
static void visit_ASTWaitStmt_(Compiler *c, ASTWaitStmt *n)
{
	visit(n->duration);
	emit(c, OP_WAIT);
}
static void visit_ASTWaitTillFrameEndStmt_(Compiler *c, ASTWaitTillFrameEndStmt *n)
{
	error(c,
		  "ASTWaitTillFrameEndStmt"
		  " unimplemented");
}
static void visit_ASTWhileStmt_(Compiler *c, ASTWhileStmt *n)
{
	// error(c,
	// 	  "ASTWhileStmt"
	// 	  " unimplemented");
}

IMPL_VISIT(ASTLiteral)
{
	// static const char *_[] = {

	// 	"STRING", "INTEGER", "BOOLEAN", "FLOAT", "VECTOR", "ANIMATION", "FUNCTION", "LOCALIZED_STRING", "UNDEFINED"
	// };

	Instruction *instr = emit(c, OP_PUSH);
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

// Program counter
static int ip(Compiler *c)
{
	return buf_size(c->instructions);
}

IMPL_VISIT(ASTIfStmt)
{
	visit(n->test);
	emit(c, OP_TEST);
	Instruction *jz = emit(c, OP_JZ);
	visit(n->consequent);
	Instruction *jmp = NULL;
	if(n->alternative)
	{
		emit(c, OP_CONST_0);
		jmp = emit(c, OP_JMP);
	}
	
	jz->operands[0] = integer(ip(c) - jz->offset);

	if(n->alternative)
	{
		emit(c, OP_CONST_1);
		if(jmp)
		{
			jmp->operands[0] = integer(ip(c) - jmp->offset);
		}
		emit(c, OP_TEST);
		Instruction *jz2 = emit(c, OP_JZ);
		visit(n->alternative);
		jz2->operands[0] = integer(ip(c) - jz2->offset);
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
		case AST_IDENTIFIER:
		{
			HashTableEntry *entry = hash_table_find(&c->variables, n->ast_identifier_data.name);
			if(!entry)
			{
				entry = hash_table_insert(&c->variables, n->ast_identifier_data.name);
				entry->integer = c->variable_index++;
			}
			emit4(c, OP_REF, integer(entry->integer), NONE, NONE, NONE);
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

IMPL_VISIT(ASTUnaryExpr)
{
	// error(c, "TODO");
}
IMPL_VISIT(ASTIdentifier)
{
	// HashTableEntry *entry = hash_table_find(&c->variables, n->name);
	// if(!entry)
	// {
	// 	error(c, "No variable '%s'", n->name);
	// }
	// emit4(c, OP_LOAD, integer(entry->integer), NONE, NONE, NONE);
	emit4(c, OP_LOAD, string(c, n->name), NONE, NONE, NONE);
}
IMPL_VISIT(ASTAssignmentExpr)
{
	if(n->op == '=')
	{
		visit(n->rhs);
		lvalue(c, n->lhs);
		emit(c, OP_STORE);
	}
	else
	{
		visit(n->rhs);
		visit(n->lhs);
		
		emit4(c, OP_BINOP, integer(n->op), NONE, NONE, NONE);
		lvalue(c, n->lhs);
		emit(c, OP_STORE);
		visit(n->lhs);
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
	for(size_t i = 0; i < n->numarguments; ++i)
	{
		visit(n->arguments[i]);
	}
	emit2(c, OP_PUSH, integer(AST_LITERAL_TYPE_INTEGER), integer(n->numarguments));
	callee(c, n->callee);
}
IMPL_VISIT(ASTExprStmt)
{
	visit(n->expression);
	emit(c, OP_POP);
}
IMPL_VISIT(ASTBlockStmt)
{
	ASTNode **it = &n->body;
	while(*it)
	{
		visit(*it);
		it = &((*it)->next);
	}
}
IMPL_VISIT(ASTFunction)
{
	c->variable_index = 0;
	hash_table_clear(&c->variables);
	visit(n->body);
}

void compiler_init(Compiler *c, jmp_buf *jmp)
{
	// c->out = fopen("compiled.gasm", "w");
	c->variable_index = 0;
	hash_table_init(&c->variables, 10);
	
	c->string_index = 0;
	hash_table_init(&c->strings, 16);

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

Instruction *compile_function(Compiler *c, ASTFunction *func)
{
	jmp_buf jmp;
	if(setjmp(jmp))
	{
		buf_free(c->instructions);
		c->instructions = NULL;
		return NULL;
	}
	c->jmp = &jmp;
	c->instructions = NULL;
	visit(func->body);
	Instruction *ins = c->instructions;
	c->instructions = NULL;
	return ins;
}