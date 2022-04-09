#ifndef AST_H
#define AST_H

#include "types.h"

typedef enum ast_type {
    ND_EXPR_STMT, // expression statement
    ND_ADD, ND_SUB, // infix +, -
    ND_MUL, ND_DIV, // infix *, /
    ND_REM, // infix % (remainder, modulo)
    ND_EQ, ND_NE, // ==, !=
    ND_GT, ND_GE, // >, >=
    ND_LT, ND_LE, // <, <=
    ND_BIT_OR, // infix |
    ND_XOR, // infix ^
    ND_BIT_AND, // infix &
    ND_BIT_RSHIFT, ND_BIT_LSHIFT, // infix >> <<
    ND_NEG, // unary -
    ND_NUM  // numbers
} ASTNodeType;

typedef struct ast_obj {
    char *name;
} ASTObj;

typedef struct ast_node {
    ASTNodeType type;
    struct ast_node *left, *right;
    union {
        union {
            i32 int32;
        } literal;
        ASTObj obj;
    } as;
} ASTNode;

typedef struct ast_program {
    Array statements;
} ASTProg;

void initASTProg(ASTProg *astp);
void freeASTProg(ASTProg *astp);

ASTNode *newNode(ASTNodeType type, ASTNode *left, ASTNode *right);

void freeAST(ASTNode *root);

ASTNode *newNumberNode(int value);
ASTNode *newUnaryNode(ASTNodeType type, ASTNode *left);

#endif // AST_H
