#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "types.h"
#include "Token.h"
#include "Array.h"

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

typedef struct ast_node {
    ASTNodeType type;
    Location loc;
} ASTNode;

typedef struct ast_literal_node {
    ASTNode header;
    union {
        i32 int32;
    } as;
} ASTLiteralNode;

typedef struct ast_unary_node {
    ASTNode header;
    ASTNode *child;
} ASTUnaryNode;

typedef struct ast_binary_node {
    ASTNode header;
    ASTNode *left, *right;
} ASTBinaryNode;

#define AS_NODE(node) ((ASTNode *)node)
#define AS_LITERAL_NODE(node) ((ASTLiteralNode *)node)
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)node)
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)node)

typedef struct ast_program {
    Array statements;
} ASTProg;

void initASTProg(ASTProg *astp);
void freeASTProg(ASTProg *astp);

ASTNode newNode(ASTNodeType type, Location loc);

ASTNode *newNumberNode(int value, Location loc);
ASTNode *newUnaryNode(ASTNodeType type, Location loc, ASTNode *child);
ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right);

void freeAST(ASTNode *root);

#endif // AST_H
