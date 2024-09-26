#pragma once
#include <stdbool.h>

typedef struct ASTNode ASTNode;
typedef ASTNode* ASTNodePtr;

typedef union
{
	char *string;
	int integer;
	float number;
	float vector[4];
	bool boolean;
	union
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
	struct ASTNode *next;
};