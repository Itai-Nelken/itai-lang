#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "types.h"
#include "Token.h"
#include "Array.h"

typedef enum ast_type {
    ND_PRINT, // temporary print function
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
    ASTNode *body;
    Array locals; // Array<ASTObj *>
    int stack_size;
} ASTFunction;

typedef struct ast_node {
    ASTNodeType type;
    struct ast_node *left, *right;
    Location loc; // for error reporting
    union {
        union {
            i32 int32;
        } literal; // ND_NUM
        ASTObj var; // ND_VAR
        Array body; // ND_BLOCK
        struct {
            struct ast_node *condition, *then, *els;
            struct ast_node *initializer, *increment;
        } conditional; // ND_IF, ND_LOOP
    } as;
} ASTNode;

typedef struct ast_program {
    Array functions; // Array<ASTFunction *>
    Array globals; // Array<ASTNode *>
} ASTProg;

void initASTProg(ASTProg *astp);
void freeASTProg(ASTProg *astp);

ASTFunction *newFunction(const char *name, ASTNode *body);
void freeFunction(ASTFunction *fn);

ASTNode *newNode(ASTNodeType type, ASTNode *left, ASTNode *right, Location loc);

void freeAST(ASTNode *root);

ASTNode *newNumberNode(int value, Location loc);
ASTNode *newUnaryNode(ASTNodeType type, ASTNode *left, Location loc);

#endif // AST_H
