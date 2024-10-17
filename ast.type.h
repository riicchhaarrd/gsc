#ifndef AST_X_MACRO
#define AST_X_MACRO(X) \
	X(AST_FUNCTION, ASTFunction, ast_function) \
	X(AST_FUNCTION_POINTER_EXPR, ASTFunctionPointerExpr, ast_function_pointer_expr) \
	X(AST_ARRAY_EXPR, ASTArrayExpr, ast_array_expr) \
	X(AST_GROUP_EXPR, ASTGroupExpr, ast_group_expr) \
	X(AST_ASSIGNMENT_EXPR, ASTAssignmentExpr, ast_assignment_expr) \
	X(AST_BINARY_EXPR, ASTBinaryExpr, ast_binary_expr) \
	X(AST_CALL_EXPR, ASTCallExpr, ast_call_expr) \
	X(AST_CONDITIONAL_EXPR, ASTConditionalExpr, ast_conditional_expr) \
	X(AST_FILE_REFERENCE, ASTFileReference, ast_file_reference) \
	X(AST_IDENTIFIER, ASTIdentifier, ast_identifier) \
	X(AST_LITERAL, ASTLiteral, ast_literal) \
	X(AST_MEMBER_EXPR, ASTMemberExpr, ast_member_expr) \
	X(AST_UNARY_EXPR, ASTUnaryExpr, ast_unary_expr) \
	X(AST_VECTOR_EXPR, ASTVectorExpr, ast_vector_expr) \
	X(AST_BLOCK_STMT, ASTBlockStmt, ast_block_stmt) \
	X(AST_BREAK_STMT, ASTBreakStmt, ast_break_stmt) \
	X(AST_CONTINUE_STMT, ASTContinueStmt, ast_continue_stmt) \
	X(AST_DO_WHILE_STMT, ASTDoWhileStmt, ast_do_while_stmt) \
	X(AST_EMPTY_STMT, ASTEmptyStmt, ast_empty_stmt) \
	X(AST_EXPR_STMT, ASTExprStmt, ast_expr_stmt) \
	X(AST_FOR_STMT, ASTForStmt, ast_for_stmt) \
	X(AST_IF_STMT, ASTIfStmt, ast_if_stmt) \
	X(AST_RETURN_STMT, ASTReturnStmt, ast_return_stmt) \
	X(AST_SWITCH_CASE, ASTSwitchCase, ast_switch_case) \
	X(AST_SWITCH_STMT, ASTSwitchStmt, ast_switch_stmt) \
	X(AST_WAIT_STMT, ASTWaitStmt, ast_wait_stmt) \
	X(AST_WAIT_TILL_FRAME_END_STMT, ASTWaitTillFrameEndStmt, ast_wait_till_frame_end_stmt) \
	X(AST_WHILE_STMT, ASTWhileStmt, ast_while_stmt)
#endif
#ifndef AST_TYPE_HEADER_INCLUDE_GUARD
#define AST_TYPE_HEADER_INCLUDE_GUARD

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef struct
{
	char name[256];
	ASTNodePtr body;
	ASTNodePtr parameters;
	int parameter_count;
} ASTFunction;

typedef struct
{
	ASTNodePtr expression;
} ASTFunctionPointerExpr;

typedef struct
{
	size_t numelements;
	ASTNodePtr *elements;
} ASTArrayExpr;

typedef struct
{
	ASTNodePtr expression;
} ASTGroupExpr;

typedef struct
{
	ASTNodePtr lhs;
	ASTNodePtr rhs;
	uint32 op;
} ASTAssignmentExpr;

typedef struct
{
	ASTNodePtr lhs;
	ASTNodePtr rhs;
	uint32 op;
} ASTBinaryExpr;

typedef struct
{
	bool threaded;
	bool pointer;
	ASTNodePtr object;
	ASTNodePtr callee;
	size_t numarguments;
	ASTNodePtr *arguments;
} ASTCallExpr;

typedef struct
{
	ASTNodePtr condition;
	ASTNodePtr consequent;
	ASTNodePtr alternative;
} ASTConditionalExpr;

typedef struct
{
	char file[256];
} ASTFileReference;

typedef struct
{
	char name[256];
} ASTIdentifier;

typedef enum
{
	AST_LITERAL_TYPE_STRING,
	AST_LITERAL_TYPE_INTEGER,
	AST_LITERAL_TYPE_BOOLEAN,
	AST_LITERAL_TYPE_FLOAT,
	// AST_LITERAL_TYPE_VECTOR,
	// AST_LITERAL_TYPE_ANIMATION,
	AST_LITERAL_TYPE_FUNCTION,
	AST_LITERAL_TYPE_LOCALIZED_STRING,
	AST_LITERAL_TYPE_UNDEFINED
} ASTLiteralType;

typedef struct
{
	uint32 type;
	ASTLiteralValue value;
} ASTLiteral;

typedef struct
{
	ASTNodePtr object;
	ASTNodePtr prop;
	uint32 op;
} ASTMemberExpr;

typedef struct
{
	ASTNodePtr argument;
	uint32 op;
	bool prefix;
} ASTUnaryExpr;

typedef struct
{
	size_t numelements;
	ASTNodePtr *elements;
} ASTVectorExpr;

typedef struct
{
	size_t numbody;
	ASTNodePtr *body;
} ASTBlockStmt;

typedef struct
{
	int unused;
} ASTBreakStmt;

typedef struct
{
	int unused;
} ASTContinueStmt;

typedef struct
{
	ASTNodePtr test;
	ASTNodePtr body;
} ASTDoWhileStmt;

typedef struct
{
	int unused;
} ASTEmptyStmt;

typedef struct
{
	ASTNodePtr expression;
} ASTExprStmt;

typedef struct
{
	ASTNodePtr init;
	ASTNodePtr test;
	ASTNodePtr update;
	ASTNodePtr body;
} ASTForStmt;

typedef struct
{
	ASTNodePtr test;
	ASTNodePtr consequent;
	ASTNodePtr alternative;
} ASTIfStmt;

typedef struct
{
	ASTNodePtr argument;
} ASTReturnStmt;

typedef struct
{
	ASTNodePtr test;
	size_t numconsequent;
	ASTNodePtr *consequent;
} ASTSwitchCase;

typedef struct
{
	ASTNodePtr discriminant;
	size_t numcases;
	ASTSwitchCase *cases;
} ASTSwitchStmt;

typedef struct
{
	ASTNodePtr duration;
} ASTWaitStmt;

typedef struct
{
	int unused;
} ASTWaitTillFrameEndStmt;

typedef struct
{
	ASTNodePtr test;
	ASTNodePtr body;
} ASTWhileStmt;

#ifdef AST_VISITOR_IMPLEMENTATION

#ifndef STRUCT_TYPE_INFO_DEFINED
#define STRUCT_TYPE_INFO_DEFINED

typedef struct
{
	const char *name;
	uint32_t hash;
	size_t size;
	size_t alignment;
	void (*initialize)(void*);
	void* (*clone)(void*);
	size_t (*visitor)(void *visitor, const char *key, void *value, size_t nmemb, size_t size);
} StructTypeInfo;

#endif

// #include <stdalign.h>

#if __STDC_VERSION__ >= 202311L // C23
    // Already supported
#elif __STDC_VERSION__ >= 201112L // C11
    #define alignof _Alignof
#else
    #define alignof(x) __alignof__(x) // Try GCC extension
#endif

typedef struct AstVisitor_s AstVisitor;
typedef size_t (*AstVisitorFn)(AstVisitor *visitor, const char *key, void *value, size_t nmemb, size_t size);

struct AstVisitor_s
{
	void *ctx;

	AstVisitorFn visit_char;
	AstVisitorFn visit_uint32;
	AstVisitorFn visit_ASTLiteralValue;
	AstVisitorFn visit_ASTNodePtr;
	AstVisitorFn visit_int;
	AstVisitorFn visit_bool;
	AstVisitorFn visit_ASTSwitchCase;
	bool (*pre_visit)(AstVisitor *visitor, const char *key, uint32_t hashed_key, void **value, size_t *nmemb, size_t size);
};

// ============              VISITORS            ==================

static size_t ASTFunction_visit(AstVisitor *visitor, const char *key, ASTFunction *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "name", 0x8e631b86, (void**)&inst->name, NULL, sizeof(inst->name[0])))
	{
		changed_count += visitor->visit_char(visitor, "name", (char*)&inst->name[0], 256, sizeof(inst->name[0]));
	}
	if(visitor->pre_visit(visitor, "body", 0xc6c93295, NULL, NULL, sizeof(inst->body)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "body", (ASTNodePtr*)&inst->body, 1, sizeof(inst->body));
	}
	if(visitor->pre_visit(visitor, "parameters", 0x3234cd99, NULL, NULL, sizeof(inst->parameters)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "parameters", (ASTNodePtr*)&inst->parameters, 1, sizeof(inst->parameters));
	}
	if(visitor->pre_visit(visitor, "parameter_count", 0xbe61e9fc, NULL, NULL, sizeof(inst->parameter_count)))
	{
		changed_count += visitor->visit_int(visitor, "parameter_count", (int*)&inst->parameter_count, 1, sizeof(inst->parameter_count));
	}
	return changed_count;
}

static size_t ASTFunctionPointerExpr_visit(AstVisitor *visitor, const char *key, ASTFunctionPointerExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "expression", 0x8a3083cb, NULL, NULL, sizeof(inst->expression)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "expression", (ASTNodePtr*)&inst->expression, 1, sizeof(inst->expression));
	}
	return changed_count;
}

static size_t ASTArrayExpr_visit(AstVisitor *visitor, const char *key, ASTArrayExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "elements", 0xf6da3a6c, (void**)&inst->elements, &inst->numelements, sizeof(inst->elements[0])) && inst->elements && inst->numelements > 0)
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "elements", (ASTNodePtr*)&inst->elements[0], inst->numelements, sizeof(inst->elements[0]));
	}
	return changed_count;
}

static size_t ASTGroupExpr_visit(AstVisitor *visitor, const char *key, ASTGroupExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "expression", 0x8a3083cb, NULL, NULL, sizeof(inst->expression)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "expression", (ASTNodePtr*)&inst->expression, 1, sizeof(inst->expression));
	}
	return changed_count;
}

static size_t ASTAssignmentExpr_visit(AstVisitor *visitor, const char *key, ASTAssignmentExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "lhs", 0x1dad32be, NULL, NULL, sizeof(inst->lhs)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "lhs", (ASTNodePtr*)&inst->lhs, 1, sizeof(inst->lhs));
	}
	if(visitor->pre_visit(visitor, "rhs", 0x6119edbc, NULL, NULL, sizeof(inst->rhs)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "rhs", (ASTNodePtr*)&inst->rhs, 1, sizeof(inst->rhs));
	}
	if(visitor->pre_visit(visitor, "op", 0xb556600a, NULL, NULL, sizeof(inst->op)))
	{
		changed_count += visitor->visit_uint32(visitor, "op", (uint32*)&inst->op, 1, sizeof(inst->op));
	}
	return changed_count;
}

static size_t ASTBinaryExpr_visit(AstVisitor *visitor, const char *key, ASTBinaryExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "lhs", 0x1dad32be, NULL, NULL, sizeof(inst->lhs)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "lhs", (ASTNodePtr*)&inst->lhs, 1, sizeof(inst->lhs));
	}
	if(visitor->pre_visit(visitor, "rhs", 0x6119edbc, NULL, NULL, sizeof(inst->rhs)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "rhs", (ASTNodePtr*)&inst->rhs, 1, sizeof(inst->rhs));
	}
	if(visitor->pre_visit(visitor, "op", 0xb556600a, NULL, NULL, sizeof(inst->op)))
	{
		changed_count += visitor->visit_uint32(visitor, "op", (uint32*)&inst->op, 1, sizeof(inst->op));
	}
	return changed_count;
}

static size_t ASTCallExpr_visit(AstVisitor *visitor, const char *key, ASTCallExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "threaded", 0x3cd9f388, NULL, NULL, sizeof(inst->threaded)))
	{
		changed_count += visitor->visit_bool(visitor, "threaded", (bool*)&inst->threaded, 1, sizeof(inst->threaded));
	}
	if(visitor->pre_visit(visitor, "pointer", 0xb5d9498a, NULL, NULL, sizeof(inst->pointer)))
	{
		changed_count += visitor->visit_bool(visitor, "pointer", (bool*)&inst->pointer, 1, sizeof(inst->pointer));
	}
	if(visitor->pre_visit(visitor, "object", 0x50df0ffa, NULL, NULL, sizeof(inst->object)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "object", (ASTNodePtr*)&inst->object, 1, sizeof(inst->object));
	}
	if(visitor->pre_visit(visitor, "callee", 0x5e4679f3, NULL, NULL, sizeof(inst->callee)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "callee", (ASTNodePtr*)&inst->callee, 1, sizeof(inst->callee));
	}
	if(visitor->pre_visit(visitor, "arguments", 0x63a2219f, (void**)&inst->arguments, &inst->numarguments, sizeof(inst->arguments[0])) && inst->arguments && inst->numarguments > 0)
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "arguments", (ASTNodePtr*)&inst->arguments[0], inst->numarguments, sizeof(inst->arguments[0]));
	}
	return changed_count;
}

static size_t ASTConditionalExpr_visit(AstVisitor *visitor, const char *key, ASTConditionalExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "condition", 0x1750745e, NULL, NULL, sizeof(inst->condition)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "condition", (ASTNodePtr*)&inst->condition, 1, sizeof(inst->condition));
	}
	if(visitor->pre_visit(visitor, "consequent", 0x125347fa, NULL, NULL, sizeof(inst->consequent)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "consequent", (ASTNodePtr*)&inst->consequent, 1, sizeof(inst->consequent));
	}
	if(visitor->pre_visit(visitor, "alternative", 0x7ce8b368, NULL, NULL, sizeof(inst->alternative)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "alternative", (ASTNodePtr*)&inst->alternative, 1, sizeof(inst->alternative));
	}
	return changed_count;
}

static size_t ASTFileReference_visit(AstVisitor *visitor, const char *key, ASTFileReference *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "file", 0xf02a6a23, (void**)&inst->file, NULL, sizeof(inst->file[0])))
	{
		changed_count += visitor->visit_char(visitor, "file", (char*)&inst->file[0], 256, sizeof(inst->file[0]));
	}
	return changed_count;
}

static size_t ASTIdentifier_visit(AstVisitor *visitor, const char *key, ASTIdentifier *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "name", 0x8e631b86, (void**)&inst->name, NULL, sizeof(inst->name[0])))
	{
		changed_count += visitor->visit_char(visitor, "name", (char*)&inst->name[0], 256, sizeof(inst->name[0]));
	}
	return changed_count;
}

static size_t ASTLiteral_visit(AstVisitor *visitor, const char *key, ASTLiteral *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "type", 0x7bfa9c2d, NULL, NULL, sizeof(inst->type)))
	{
		changed_count += visitor->visit_uint32(visitor, "type", (uint32*)&inst->type, 1, sizeof(inst->type));
	}
	if(visitor->pre_visit(visitor, "value", 0x30e80cea, NULL, NULL, sizeof(inst->value)))
	{
		changed_count += visitor->visit_ASTLiteralValue(visitor, "value", (ASTLiteralValue*)&inst->value, 1, sizeof(inst->value));
	}
	return changed_count;
}

static size_t ASTMemberExpr_visit(AstVisitor *visitor, const char *key, ASTMemberExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "object", 0x50df0ffa, NULL, NULL, sizeof(inst->object)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "object", (ASTNodePtr*)&inst->object, 1, sizeof(inst->object));
	}
	if(visitor->pre_visit(visitor, "prop", 0x35379c18, NULL, NULL, sizeof(inst->prop)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "prop", (ASTNodePtr*)&inst->prop, 1, sizeof(inst->prop));
	}
	if(visitor->pre_visit(visitor, "op", 0xb556600a, NULL, NULL, sizeof(inst->op)))
	{
		changed_count += visitor->visit_uint32(visitor, "op", (uint32*)&inst->op, 1, sizeof(inst->op));
	}
	return changed_count;
}

static size_t ASTUnaryExpr_visit(AstVisitor *visitor, const char *key, ASTUnaryExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "argument", 0x3c41b216, NULL, NULL, sizeof(inst->argument)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "argument", (ASTNodePtr*)&inst->argument, 1, sizeof(inst->argument));
	}
	if(visitor->pre_visit(visitor, "op", 0xb556600a, NULL, NULL, sizeof(inst->op)))
	{
		changed_count += visitor->visit_uint32(visitor, "op", (uint32*)&inst->op, 1, sizeof(inst->op));
	}
	if(visitor->pre_visit(visitor, "prefix", 0x72531e29, NULL, NULL, sizeof(inst->prefix)))
	{
		changed_count += visitor->visit_bool(visitor, "prefix", (bool*)&inst->prefix, 1, sizeof(inst->prefix));
	}
	return changed_count;
}

static size_t ASTVectorExpr_visit(AstVisitor *visitor, const char *key, ASTVectorExpr *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "elements", 0xf6da3a6c, (void**)&inst->elements, &inst->numelements, sizeof(inst->elements[0])) && inst->elements && inst->numelements > 0)
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "elements", (ASTNodePtr*)&inst->elements[0], inst->numelements, sizeof(inst->elements[0]));
	}
	return changed_count;
}

static size_t ASTBlockStmt_visit(AstVisitor *visitor, const char *key, ASTBlockStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "body", 0xc6c93295, (void**)&inst->body, &inst->numbody, sizeof(inst->body[0])) && inst->body && inst->numbody > 0)
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "body", (ASTNodePtr*)&inst->body[0], inst->numbody, sizeof(inst->body[0]));
	}
	return changed_count;
}

static size_t ASTBreakStmt_visit(AstVisitor *visitor, const char *key, ASTBreakStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "unused", 0xd46e39db, NULL, NULL, sizeof(inst->unused)))
	{
		changed_count += visitor->visit_int(visitor, "unused", (int*)&inst->unused, 1, sizeof(inst->unused));
	}
	return changed_count;
}

static size_t ASTContinueStmt_visit(AstVisitor *visitor, const char *key, ASTContinueStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "unused", 0xd46e39db, NULL, NULL, sizeof(inst->unused)))
	{
		changed_count += visitor->visit_int(visitor, "unused", (int*)&inst->unused, 1, sizeof(inst->unused));
	}
	return changed_count;
}

static size_t ASTDoWhileStmt_visit(AstVisitor *visitor, const char *key, ASTDoWhileStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "test", 0x197c2b25, NULL, NULL, sizeof(inst->test)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "test", (ASTNodePtr*)&inst->test, 1, sizeof(inst->test));
	}
	if(visitor->pre_visit(visitor, "body", 0xc6c93295, NULL, NULL, sizeof(inst->body)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "body", (ASTNodePtr*)&inst->body, 1, sizeof(inst->body));
	}
	return changed_count;
}

static size_t ASTEmptyStmt_visit(AstVisitor *visitor, const char *key, ASTEmptyStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "unused", 0xd46e39db, NULL, NULL, sizeof(inst->unused)))
	{
		changed_count += visitor->visit_int(visitor, "unused", (int*)&inst->unused, 1, sizeof(inst->unused));
	}
	return changed_count;
}

static size_t ASTExprStmt_visit(AstVisitor *visitor, const char *key, ASTExprStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "expression", 0x8a3083cb, NULL, NULL, sizeof(inst->expression)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "expression", (ASTNodePtr*)&inst->expression, 1, sizeof(inst->expression));
	}
	return changed_count;
}

static size_t ASTForStmt_visit(AstVisitor *visitor, const char *key, ASTForStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "init", 0x7ab57213, NULL, NULL, sizeof(inst->init)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "init", (ASTNodePtr*)&inst->init, 1, sizeof(inst->init));
	}
	if(visitor->pre_visit(visitor, "test", 0x197c2b25, NULL, NULL, sizeof(inst->test)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "test", (ASTNodePtr*)&inst->test, 1, sizeof(inst->test));
	}
	if(visitor->pre_visit(visitor, "update", 0x2a3d0454, NULL, NULL, sizeof(inst->update)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "update", (ASTNodePtr*)&inst->update, 1, sizeof(inst->update));
	}
	if(visitor->pre_visit(visitor, "body", 0xc6c93295, NULL, NULL, sizeof(inst->body)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "body", (ASTNodePtr*)&inst->body, 1, sizeof(inst->body));
	}
	return changed_count;
}

static size_t ASTIfStmt_visit(AstVisitor *visitor, const char *key, ASTIfStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "test", 0x197c2b25, NULL, NULL, sizeof(inst->test)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "test", (ASTNodePtr*)&inst->test, 1, sizeof(inst->test));
	}
	if(visitor->pre_visit(visitor, "consequent", 0x125347fa, NULL, NULL, sizeof(inst->consequent)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "consequent", (ASTNodePtr*)&inst->consequent, 1, sizeof(inst->consequent));
	}
	if(visitor->pre_visit(visitor, "alternative", 0x7ce8b368, NULL, NULL, sizeof(inst->alternative)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "alternative", (ASTNodePtr*)&inst->alternative, 1, sizeof(inst->alternative));
	}
	return changed_count;
}

static size_t ASTReturnStmt_visit(AstVisitor *visitor, const char *key, ASTReturnStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "argument", 0x3c41b216, NULL, NULL, sizeof(inst->argument)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "argument", (ASTNodePtr*)&inst->argument, 1, sizeof(inst->argument));
	}
	return changed_count;
}

static size_t ASTSwitchCase_visit(AstVisitor *visitor, const char *key, ASTSwitchCase *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "test", 0x197c2b25, NULL, NULL, sizeof(inst->test)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "test", (ASTNodePtr*)&inst->test, 1, sizeof(inst->test));
	}
	if(visitor->pre_visit(visitor, "consequent", 0x125347fa, (void**)&inst->consequent, &inst->numconsequent, sizeof(inst->consequent[0])) && inst->consequent && inst->numconsequent > 0)
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "consequent", (ASTNodePtr*)&inst->consequent[0], inst->numconsequent, sizeof(inst->consequent[0]));
	}
	return changed_count;
}

static size_t ASTSwitchStmt_visit(AstVisitor *visitor, const char *key, ASTSwitchStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "discriminant", 0xdbdd9d44, NULL, NULL, sizeof(inst->discriminant)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "discriminant", (ASTNodePtr*)&inst->discriminant, 1, sizeof(inst->discriminant));
	}
	if(visitor->pre_visit(visitor, "cases", 0x5ebf046, (void**)&inst->cases, &inst->numcases, sizeof(inst->cases[0])) && inst->cases && inst->numcases > 0)
	{
		changed_count += visitor->visit_ASTSwitchCase(visitor, "cases", (ASTSwitchCase*)&inst->cases[0], inst->numcases, sizeof(inst->cases[0]));
	}
	return changed_count;
}

static size_t ASTWaitStmt_visit(AstVisitor *visitor, const char *key, ASTWaitStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "duration", 0x9dd3208d, NULL, NULL, sizeof(inst->duration)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "duration", (ASTNodePtr*)&inst->duration, 1, sizeof(inst->duration));
	}
	return changed_count;
}

static size_t ASTWaitTillFrameEndStmt_visit(AstVisitor *visitor, const char *key, ASTWaitTillFrameEndStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "unused", 0xd46e39db, NULL, NULL, sizeof(inst->unused)))
	{
		changed_count += visitor->visit_int(visitor, "unused", (int*)&inst->unused, 1, sizeof(inst->unused));
	}
	return changed_count;
}

static size_t ASTWhileStmt_visit(AstVisitor *visitor, const char *key, ASTWhileStmt *inst, size_t nmemb, size_t size)
{
	size_t changed_count = 0;
	size_t n = 0;
	if(visitor->pre_visit(visitor, "test", 0x197c2b25, NULL, NULL, sizeof(inst->test)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "test", (ASTNodePtr*)&inst->test, 1, sizeof(inst->test));
	}
	if(visitor->pre_visit(visitor, "body", 0xc6c93295, NULL, NULL, sizeof(inst->body)))
	{
		changed_count += visitor->visit_ASTNodePtr(visitor, "body", (ASTNodePtr*)&inst->body, 1, sizeof(inst->body));
	}
	return changed_count;
}

// ================================================================

static size_t ast_visitor_dummy_(AstVisitor *visitor, const char *key, void *value, size_t nmemb, size_t size) { return 0; }
static bool ast_pre_visit_dummy_(AstVisitor *visitor, const char *key, uint32_t hashed_key, void **value, size_t *nmemb, size_t size) { return true; }
static void type_ast_visitor_init(AstVisitor *v, void *ctx)
{
	v->ctx = ctx;
	v->pre_visit = ast_pre_visit_dummy_;
	v->visit_char = ast_visitor_dummy_;
	v->visit_uint32 = ast_visitor_dummy_;
	v->visit_ASTLiteralValue = ast_visitor_dummy_;
	v->visit_ASTNodePtr = ast_visitor_dummy_;
	v->visit_int = ast_visitor_dummy_;
	v->visit_bool = ast_visitor_dummy_;
	v->visit_ASTSwitchCase = (AstVisitorFn)ASTSwitchCase_visit;
}
// ============           INITIALIZATION         ==================

static void ASTFunction_init(ASTFunction *inst)
{
}

static void ASTFunctionPointerExpr_init(ASTFunctionPointerExpr *inst)
{
}

static void ASTArrayExpr_init(ASTArrayExpr *inst)
{
}

static void ASTGroupExpr_init(ASTGroupExpr *inst)
{
}

static void ASTAssignmentExpr_init(ASTAssignmentExpr *inst)
{
}

static void ASTBinaryExpr_init(ASTBinaryExpr *inst)
{
}

static void ASTCallExpr_init(ASTCallExpr *inst)
{
	inst->threaded = false;
	inst->pointer = false;
}

static void ASTConditionalExpr_init(ASTConditionalExpr *inst)
{
}

static void ASTFileReference_init(ASTFileReference *inst)
{
}

static void ASTIdentifier_init(ASTIdentifier *inst)
{
}

static void ASTLiteral_init(ASTLiteral *inst)
{
}

static void ASTMemberExpr_init(ASTMemberExpr *inst)
{
}

static void ASTUnaryExpr_init(ASTUnaryExpr *inst)
{
}

static void ASTVectorExpr_init(ASTVectorExpr *inst)
{
}

static void ASTBlockStmt_init(ASTBlockStmt *inst)
{
}

static void ASTBreakStmt_init(ASTBreakStmt *inst)
{
}

static void ASTContinueStmt_init(ASTContinueStmt *inst)
{
}

static void ASTDoWhileStmt_init(ASTDoWhileStmt *inst)
{
}

static void ASTEmptyStmt_init(ASTEmptyStmt *inst)
{
}

static void ASTExprStmt_init(ASTExprStmt *inst)
{
}

static void ASTForStmt_init(ASTForStmt *inst)
{
}

static void ASTIfStmt_init(ASTIfStmt *inst)
{
}

static void ASTReturnStmt_init(ASTReturnStmt *inst)
{
}

static void ASTSwitchCase_init(ASTSwitchCase *inst)
{
}

static void ASTSwitchStmt_init(ASTSwitchStmt *inst)
{
	inst->numcases = 0;
	inst->cases = NULL;
}

static void ASTWaitStmt_init(ASTWaitStmt *inst)
{
}

static void ASTWaitTillFrameEndStmt_init(ASTWaitTillFrameEndStmt *inst)
{
}

static void ASTWhileStmt_init(ASTWhileStmt *inst)
{
}

// ================================================================


#ifndef type_info
static StructTypeInfo* type_info_(StructTypeInfo **ptr)
{
	return *ptr;
}
#define type_info(ptr) type_info_((StructTypeInfo**)ptr)
#endif

#ifndef type_init
#define type_init(type, name) \
    type name = (type){ 0 }; \
    type##_init(&name)
#endif

#endif
#endif
