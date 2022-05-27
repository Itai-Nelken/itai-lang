#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "types.h"
#include "Token.h"
#include "Array.h"
#include "Symbols.h"
#include "Arena.h"

typedef enum ast_type {
    ND_BLOCK, // block ({ ... })
    ND_IDENTIFIER, // identifier
    ND_ASSIGN, // assignment (infix =)
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

typedef struct ast_identifier_node {
    ASTNode header;
    int id;
} ASTIdentifierNode;

typedef struct ast_block_node {
    ASTNode header;
    Array body; // Array<ASTNode *>
} ASTBlockNode;

#define AS_NODE(node) ((ASTNode *)node)
#define AS_LITERAL_NODE(node) ((ASTLiteralNode *)node)
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)node)
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)node)
#define AS_IDENTIFIER_NODE(node) ((ASTIdentifierNode *)node)
#define AS_BLOCK_NODE(node) ((ASTBlockNode *)node)

typedef struct ast_identifier {
    char *text; // owned by the instance.
    int length;
} ASTIdentifier;

ASTIdentifier *newIdentifier(char *str, int length);
void freeIdentifier(ASTIdentifier *identifier);

typedef struct ast_function {
    ASTIdentifierNode *name;
    SymTable identifiers;
    Array locals; // Array<ASTNode *>
    ASTBlockNode *body;
} ASTFunction;

// initializes the body to NULL
ASTFunction *newFunction(ASTIdentifierNode *name);
void freeFunction(ASTFunction *fn);

typedef struct ast_program {
    SymTable identifiers; // global identifiers
    Array globals; // Array<ASTNode *>
    Array functions; // Array<ASTFunction *>
} ASTProg;

void initASTProg(ASTProg *astp);
void freeASTProg(ASTProg *astp);

ASTNode newNode(ASTNodeType type, Location loc);

ASTNode *newNumberNode(int value, Location loc);
ASTNode *newUnaryNode(ASTNodeType type, Location loc, ASTNode *child);
ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right);
ASTNode *newIdentifierNode(Location loc, int id);
// initializes with empty body
ASTNode *newBlockNode(Location loc);

void freeAST(ASTNode *root);

#endif // AST_H
