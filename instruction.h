#pragma once

#include <stdint.h>
#include <core/ds/hash_trie.h>

#define OPCODES(X) \
	X(PUSH)        \
	X(POP)         \
	X(UNDEF)       \
	X(NOP)         \
	X(LOAD)        \
	X(STORE)       \
	X(REF)         \
	X(LOAD_FIELD)  \
	X(FIELD_REF)   \
	X(BINOP)       \
	X(RET)         \
	X(CALL)        \
	X(CALL_PTR)    \
	X(TEST)        \
	X(JMP)         \
	X(JZ)          \
	X(JNZ)         \
	X(CONST_0)     \
	X(CONST_1)     \
	X(TABLE)       \
	X(WAIT)        \
	X(UNARY)       \
	X(VECTOR)      \
	X(PRINT_EXPR)      \
	X(GLOB)
 // X(SELF)

typedef enum
{
	OP_INVALID,
#define OPCODE_ENUM(NAME) OP_##NAME,
	OPCODES(OPCODE_ENUM) OP_MAX
} Opcode;

static const char *opcode_names[] = {
	"invalid",
#define OPCODE_ENUM_STR(NAME) #NAME,
	OPCODES(OPCODE_ENUM_STR) NULL,
};

#define VM_CALL_FLAG_NONE (0)
#define VM_CALL_FLAG_THREADED (1)
#define VM_CALL_FLAG_METHOD (2)

// typedef enum
// {
// 	OP_PUSH,
// 	// OP_PUSHF,
// 	// ...
// 	OP_POP,
// 	OP_NOP,
// 	OP_LOAD,
// 	OP_STORE,
//     OP_REF,
//     OP_LOAD_FIELD,
//     OP_FIELD_REF,
// 	// OP_LOAD_REF,
// 	// OP_STORE_REF,
// 	// OP_LOAD_OBJECT_FIELD_REF,

// 	OP_BINOP,
// 	// OP_ADD,
// 	// OP_SUB,
// 	// OP_MUL,
// 	// OP_DIV,
// 	// OP_MOD,

// 	OP_RET,
// 	OP_CALL,
//     // OP_CALL_EXTERNAL,
// 	OP_TEST,
// 	OP_JMP,
// 	OP_JZ,
// 	OP_JNZ,
// 	OP_CONST_0,
// 	OP_CONST_1,
// 	OP_WAIT,
// 	// OP_LABEL
// } Opcode;

typedef enum
{
	OPERAND_TYPE_NONE,
	OPERAND_TYPE_INT,
	OPERAND_TYPE_FLOAT,
	OPERAND_TYPE_INDEXED_STRING
} OperandType;

static const char *operand_type_names[] = { "NONE", "INT", "FLOAT", "STRING", NULL };

typedef struct
{
	OperandType type;
	union
	{
		int64_t integer;
		float number;
		unsigned int string_index;
	} value;
} Operand;

#define MAX_OPERANDS (4)

typedef struct Instruction Instruction;
struct Instruction
{
	int32_t offset;
	uint8_t opcode;
	Operand operands[MAX_OPERANDS];
	int line;
};

enum
{
	COMPILE_STATE_NOT_STARTED,
	COMPILE_STATE_FAILED,
	COMPILE_STATE_DONE
};

typedef struct
{
	const char *name;
	HashTrie functions;
	HashTrie includes;
	HashTrie file_references;
	int state;
} CompiledFile;

typedef struct
{
	const char *name;
	CompiledFile *file;
	Instruction *instructions;
	size_t parameter_count;
	size_t local_count;
	char **variable_names;
	int line;
} CompiledFunction;