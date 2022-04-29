#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "types.h"
#include "Token.h"
#include "Array.h"

typedef enum ast_type {
    ND_PRINT, // temporary print function
    ND_FN_CALL, // function call
    ND_IF, // if statement
    ND_LOOP, // for statement
    ND_RETURN, // return statement
    ND_BLOCK, // { ... }
    ND_VAR, ND_ASSIGN, // variable, assignment
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

typedef enum ast_obj_type {
    OBJ_GLOBAL,
    OBJ_LOCAL
} ASTObjType;

typedef struct ast_obj {
    ASTObjType type;
    char *name;
    int offset; // for OBJ_LOCAL
} ASTObj;

typedef struct ast_node ASTNode;
typedef struct ast_function {
    char *name;
    Location location;
    ASTNode *body;
    Array locals; // Array<ASTObj *>
} ASTFunction;

//typedef struct ast_node {
//    ASTNodeType type;
//    struct ast_node *left, *right;
//    Location loc; // for error reporting
//    union {
//        union {
//            i32 int32;
//        } literal; // ND_NUM
//        ASTObj var; // ND_VAR
//        Array body; // ND_BLOCK
//        char *name; // ND_FN_CALL
//        struct {
//            struct ast_node *condition, *then, *els;
//            struct ast_node *initializer, *increment;
//        } conditional; // ND_IF, ND_LOOP
//    } as;
//} ASTNode;

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

typedef struct ast_block_node {
    ASTNode header;
    Array body;
} ASTBlockNode;

typedef struct ast_obj_node {
    ASTNode header;
    ASTObj obj;
} ASTObjNode;

typedef struct ast_conditional_node {
    ASTNode header;
    ASTNode *condition, *then, *else_;
} ASTConditionalNode;

typedef struct ast_loop_node {
    ASTNode header;
    ASTNode *initializer, *condition, *increment, *body;
} ASTLoopNode;

#define AS_NODE(node) ((ASTNode *)node)
#define AS_LITERAL_NODE(node) ((ASTLiteralNode *)node)
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)node)
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)node)
#define AS_BLOCK_NODE(node) ((ASTBlockNode *)node)
#define AS_OBJ_NODE(node) ((ASTObjNode *)node)
#define AS_CONDITIONAL_NODE(node) ((ASTConditionalNode *)node)
#define AS_LOOP_NODE(node) ((ASTLoopNode *)node)

typedef struct ast_program {
    Array functions; // Array<ASTFunction *>
    Array globals; // Array<ASTNode *>
} ASTProg;

void initASTProg(ASTProg *astp);
void freeASTProg(ASTProg *astp);

ASTFunction *newFunction(const char *name, Location loc, ASTNode *body);
void freeFunction(ASTFunction *fn);

//ASTNode *newNode(ASTNodeType type, ASTNode *left, ASTNode *right, Location loc);
ASTNode newNode(ASTNodeType type, Location loc);

void freeAST(ASTNode *root);

ASTNode *newNumberNode(int value, Location loc);
ASTNode *newUnaryNode(ASTNodeType type, Location loc, ASTNode *child);

ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right);
ASTNode *newObjNode(ASTNodeType type, Location loc, ASTObj obj);
ASTNode *newBlockNode(Location loc);
ASTNode *newConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTNode *then, ASTNode *else_);
ASTNode *newLoopNode(Location loc, ASTNode *initializer, ASTNode *condition, ASTNode *increment, ASTNode *body);

#endif // AST_H
