#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "common.h"
#include "Array.h"
#include "Token.h"
#include "Types.h"
#include "Symbols.h"

// NOTE: when adding a new node type, also add it
//       as a string to 'ast_node_type_str' and nodeName() in ast.c,
//       freeAST(), printAST(), and node_name() (all in ast.c).
typedef enum ast_node_type {
    ND_LOOP, // for, while
    ND_IF, // if statement
    ND_CALL, // call
    ND_RETURN, // return statement
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
    ND_NUM // numbers
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
    Type type; // for variables.
} ASTIdentifierNode;

typedef struct ast_block_node {
    ASTNode header;
    Array body; // Array<ASTNode *>
} ASTBlockNode;

// if-else, ternary
typedef struct ast_conditional_node {
    ASTNode header;
    ASTNode *condition, *body, *else_;
} ASTConditionalNode;

typedef struct ast_loop_node {
    ASTNode header;
    ASTNode *init, *condition, *increment, *body;
} ASTLoopNode;

#define AS_NODE(node) ((ASTNode *)node)
#define AS_LITERAL_NODE(node) ((ASTLiteralNode *)node)
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)node)
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)node)
#define AS_IDENTIFIER_NODE(node) ((ASTIdentifierNode *)node)
#define AS_BLOCK_NODE(node) ((ASTBlockNode *)node)
#define AS_CONDITIONAL_NODE(node) ((ASTConditionalNode *)node)
#define AS_LOOP_NODE(node) ((ASTLoopNode *)node)

typedef struct ast_identifier {
    char *text; // owned by the instance.
    int length;
} ASTIdentifier;

ASTIdentifier *newIdentifier(char *str, int length);
void freeIdentifier(ASTIdentifier *identifier);

typedef struct ast_function {
    ASTIdentifierNode *name;
    Type return_type;
    Array locals; // Array<ASTNode *>
    ASTBlockNode *body;
} ASTFunction;

// initializes the body to NULL
ASTFunction *newFunction(ASTIdentifierNode *name, Type return_type);
void freeFunction(ASTFunction *fn);

typedef struct ast_program {
    SymTable identifiers; // global identifiers
    Array types; // Array<int>
    Array globals; // Array<ASTNode *>
    Array functions; // Array<ASTFunction *>
} ASTProg;

void initASTProg(ASTProg *astp);
void freeASTProg(ASTProg *astp);

ASTNode newNode(ASTNodeType type, Location loc);

ASTNode *newNumberNode(int value, Location loc);
ASTNode *newUnaryNode(ASTNodeType type, Location loc, ASTNode *child);
ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right);
ASTNode *newIdentifierNode(Location loc, int id, Type ty);
// initializes with empty body
ASTNode *newBlockNode(Location loc);
ASTNode *newConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTNode *body, ASTNode *else_);
ASTNode *newLoopNode(Location loc, ASTNode *init, ASTNode *condition, ASTNode *increment, ASTNode *body);
#define newWhileLoopNode(location, condition, body) newLoopNode((location), NULL, (condition), NULL, (body))

void freeAST(ASTNode *root);

void printAST(ASTNode *root);

#endif // AST_H
