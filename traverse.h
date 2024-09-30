#pragma once
#include <stdbool.h>

typedef struct ASTNode ASTNode;
typedef bool (*TraverseNodeFn)(ASTNode *, void *);
void traverse(ASTNode *root, TraverseNodeFn node_fn, void *ctx);