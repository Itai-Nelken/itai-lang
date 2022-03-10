#ifndef AST_H
#define AST_H

#include "types.h"

typedef enum ast_type {
    ND_ADD, ND_SUB,
    ND_MUL, ND_DIV,
    ND_NEG,
    ND_NUM
} ASTNodeType;

typedef struct ast_node {
    ASTNodeType type;
    struct ast_node *left, *right;
    union {
        i32 int32;
    } literal;
} ASTNode;

ASTNode *newNode(ASTNodeType type, ASTNode *left, ASTNode *right);

void freeAST(ASTNode *root);

ASTNode *newNumberNode(int value);
ASTNode *newUnaryNode(ASTNodeType type, ASTNode *left);

#endif // AST_H
