#include "ast.h"

// #ifndef AST_NODES
// #define AST_NODES(X) \
// 	X(AST_ARRAY_EXPR, ASTArrayExpr, ast_array_expr) \
// 	X(AST_ASSIGNMENT_EXPR, ASTAssignmentExpr, ast_assignment_expr) \
// 	X(AST_BINARY_EXPR, ASTBinaryExpr, ast_binary_expr) \
// 	X(AST_CALL_EXPR, ASTCallExpr, ast_call_expr) \
// 	X(AST_CONDITIONAL_EXPR, ASTConditionalExpr, ast_conditional_expr) \
// 	X(AST_FUNCTION_PTR, ASTFunctionPtr, ast_function_ptr) \
// 	X(AST_IDENTIFIER, ASTIdentifier, ast_identifier) \
// 	X(AST_LITERAL, ASTLiteral, ast_literal) \
// 	X(AST_LOCALIZED_STRING, ASTLocalizedString, ast_localized_string) \
// 	X(AST_MEMBER_EXPR, ASTMemberExpr, ast_member_expr) \
// 	X(AST_UNARY_EXPR, ASTUnaryExpr, ast_unary_expr) \
// 	X(AST_VECTOR_EXPR, ASTVectorExpr, ast_vector_expr) \
// 	X(AST_BLOCK_STMT, ASTBlockStmt, ast_block_stmt) \
// 	X(AST_BREAK_STMT, ASTBreakStmt, ast_break_stmt) \
// 	X(AST_CONTINUE_STMT, ASTContinueStmt, ast_continue_stmt) \
// 	X(AST_DO_WHILE_STMT, ASTDoWhileStmt, ast_do_while_stmt) \
// 	X(AST_EMPTY_STMT, ASTEmptyStmt, ast_empty_stmt) \
// 	X(AST_EXPR_STMT, ASTExprStmt, ast_expr_stmt) \
// 	X(AST_FOR_STMT, ASTForStmt, ast_for_stmt) \
// 	X(AST_IF_STMT, ASTIfStmt, ast_if_stmt) \
// 	X(AST_RETURN_STMT, ASTReturnStmt, ast_return_stmt) \
// 	X(AST_SWITCH_CASE, ASTSwitchCase, ast_switch_case) \
// 	X(AST_SWITCH_STMT, ASTSwitchStmt, ast_switch_stmt) \
// 	X(AST_WAIT_STMT, ASTWaitStmt, ast_wait_stmt) \
// 	X(AST_WAIT_TILL_FRAME_END_STMT, ASTWaitTillFrameEndStmt, ast_wait_till_frame_end_stmt) \
// 	X(AST_WHILE_STMT, ASTWhileStmt, ast_while_stmt)
// #endif

typedef struct ASTVisitor ASTVisitor;

#define TYPEDEF_VISIT_ENTRY(UPPER, TYPE, LOWER) typedef void (*Visit##TYPE##Fn)(ASTVisitor*, ASTNode*, TYPE*);
AST_X_MACRO(TYPEDEF_VISIT_ENTRY)

#define VISIT_ENTRY(UPPER, TYPE, LOWER) Visit##TYPE##Fn visit_ ## LOWER;
struct ASTVisitor
{
    void *ctx;
    AST_X_MACRO(VISIT_ENTRY)

	void (*visit_fallback)(ASTVisitor *, ASTNode *, const char *type);
};

#define visit_node(v, n) visit_node_(v, n, FILE_BASENAME, __LINE__, #n)
void visit_node_(ASTVisitor *v, ASTNode *n, const char *file, int line, const char *extra);