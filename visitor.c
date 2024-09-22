#include "visitor.h"

void visit_node_(ASTVisitor *v, ASTNode *n, const char *file, int line, const char *extra)
{
	if(!n)
	{
		printf("Node is NULL, %s:%d (%s)\n", file, line, extra);
		return;
	}
	switch(n->type)
	{
#define CASE(UPPER, TYPE, LOWER)                                                \
	case UPPER:                                                                 \
		if(v->visit_##LOWER)                                                    \
			v->visit_##LOWER(v, (ASTNode *)&n->LOWER##_data, &n->LOWER##_data); \
		else if(v->visit_fallback)                                              \
			v->visit_fallback(v, (ASTNode *)&n->LOWER##_data, #TYPE);           \
		break;

		AST_X_MACRO(CASE)
	}
}