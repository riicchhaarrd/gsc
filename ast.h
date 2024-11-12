#pragma once
#include <stdbool.h>
#include "allocator.h"
#include "hash_trie.h"

typedef struct ASTNode ASTNode;
typedef ASTNode* ASTNodePtr;

typedef union
{
	char *string;
	int64_t integer;
	float number;
	float vector[4];
	bool boolean;
	struct
	{
		ASTNode *file; // ASTFileReference
		ASTNode *function; // ASTIdentifier
	} function;
} ASTLiteralValue;

// typedef struct
// {
// 	ASTNode *node;
// } ASTExpr;

// typedef struct
// {
// 	ASTNode *node;
// } ASTStmt;

#include "ast.type.h"

#define AST_ENUM(UPPER, TYPE, LOWER) UPPER,
enum
{
	AST_INVALID,
	AST_X_MACRO(AST_ENUM) AST_MAX
};

#define AST_STRINGS(UPPER, TYPE, LOWER) #TYPE,
static const char *ast_node_names[] = { "invalid", AST_X_MACRO(AST_STRINGS) NULL };

#define AST_FIELD_DATA(UPPER, TYPE, LOWER) TYPE LOWER##_data;
struct ASTNode
{
	union
	{
		AST_X_MACRO(AST_FIELD_DATA)
	};
	uint32_t type;
	size_t offset;
	int line;
	struct ASTNode *next;
};

// typedef struct ASTFile ASTFile;
// struct ASTFile
// {
// 	HashTrie functions; // ASTFunction
// 	char path[256];
// 	// char *source;
// 	// Node *includes; // TODO: FIXME reverse order
// };

// typedef struct ASTProgram ASTProgram;
// struct ASTProgram
// {
// 	Allocator *allocator;
// 	HashTrie files; // ASTFile
// 	char base_path[256];
// 	// const char *source;
// };
