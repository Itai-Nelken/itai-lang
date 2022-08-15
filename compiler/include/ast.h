#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdbool.h>
#include "Token.h"
#include "Strings.h"
#include "Array.h"

// Update astPrint(), astFree(), node_type_name(), node_name() in ast.c
// and token_to_node_type() in Parser.c when adding new types.
typedef enum ast_node_type {
    ND_ADD, ND_SUB,
    ND_MUL, ND_DIV,
    ND_NEG,
    ND_NUMBER
} ASTNodeType;

typedef struct ast_node {
    ASTNodeType type;
    Location location;
} ASTNode;

typedef struct ast_number_node {
    ASTNode header;
    NumberConstant value;
} ASTNumberNode;

typedef struct ast_unary_node {
    ASTNode header;
    ASTNode *operand;
} ASTUnaryNode;

typedef struct ast_binary_node {
    ASTNode header;
    ASTNode *left, *right;
} ASTBinaryNode;

typedef struct ast_list_node {
    ASTNode header;
    Array body; // Array<ASTNode *>
} ASTListNode;

typedef struct ast_conditional_node {
    ASTNode header;
    ASTNode *condition;
    ASTListNode *body;
    ASTNode *else_;
} ASTConditionalNode;

typedef struct ast_loop_node {
    ASTNode header;
    ASTNode *initializer;
    ASTNode *condition;
    ASTNode *increment;
    ASTListNode *body;
} ASTLoopNode;

#define AS_NODE(node) ((ASTNode *)(node))
#define AS_NUMBER_NODE(node) ((ASTNumberNode *)(node))
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)(node))
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)(node))
#define AS_LIST_NODE(node) ((ASTListNode *)(node))
#define AS_CONDITIONAL_NODE(node) ((ASTConditionalNode *)(node))
#define AS_LOOP_NODE(node) ((ASTLoopNode *)(node))

/*
typedef enum ast_obj_type {
    OBJ_FUNCTION,
    OBJ_GLOBAL, OBJ_LOCAL, OBJ_PARAMETER,
    OBJ_STRUCT, OBJ_ENUM,
    OBJ_TYPEDEF
} ASTObjType;

typedef struct ast_obj {
    ASTObjType type;
    Location location;
    String name;
    // Type data_type;
    // ScopeID scope;
    // bool is_public;
} ASTObj;

typedef struct ast_record_obj {
    ASTObj header;
    Array fields; // Array<ASTObj *>
    Array functions; // Array<ASTFunctionObj *>
} ASTRecordObj;

typedef struct ast_function_obj {
    ASTObj header;
    // Type return_type;
    Array parameters; // Array<ASTObj *> (OBJ_PARAMETER)
    Array generic_parameters; // Array<ASTObj *> (OBJ_TYPEDEF)
    Array locals; // Array<ASTVariableNode *> (OBJ_LOCAL)
    ASTListNode body;
} ASTFunctionObj;

typedef struct ast_variable_obj {
    ASTObj header;
    ASTNode *initializer;
    bool is_const;
} ASTVariableObj;

typedef struct ast_program {
    ASTFunctionObj entry_point;
    Array functions; // Array<ASTFunctionObj *>
    Array records; // Array<ASTRecordObj *>
    Array types; // Array<ASTObj *> (OBJ_TYPEDEF)
} ASTProgram;

#define AS_OBJ(obj) ((ASTObj *)(obj))
#define AS_RECORD_OBJ(obj) ((ASTRecordObj *)(obj))
#define AS_FUNCTION_OBJ(obj) ((ASTFunctionObj *)(obj))
#define AS_VARIABLE_OBJ(obj) ((ASTVariableObj *)(obj))

// constructors...
*/

/***
 * Create a new AST number node.
 *
 * @param loc The Location of the number.
 * @param value The value of the number.
 ***/
ASTNode *astNewNumberNode(Location loc, NumberConstant value);

/***
 * Create a new AST unary node.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param operand The operand.
 ***/
ASTNode *astNewUnaryNode(ASTNodeType type, Location loc, ASTNode *operand);

/***
 * Create a new AST binary node.
 *
 * @param type The type of the node.
 * @param loc A Location.
 * @param left The left side.
 * @param right The right side.
 ***/
ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right);

/***
 * Create a new AST list node.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param body An initialized Array containing the list of nodes.
 ***/
ASTNode *astNewListNode(ASTNodeType type, Location loc, Array body);

/***
 * Create a new AST conditional node.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param condition The condition.
 * @param body The body (to be executed if the condition is true).
 * @param else The else clause (to be executed if the condition is false).
 ***/
ASTNode *astNewConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTListNode *body, ASTNode *else_);

/***
 * Create a new AST loop node.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param initializer The initializer.
 * @param condition The condition.
 * @param increment The increment clause (to be executed after every iteration).
 * @param body The body of the loop.
 ***/
ASTNode *astNewLoopNode(ASTNodeType type, Location loc, ASTNode *initializer, ASTNode *condition, ASTNode *increment, ASTListNode *body);

/***
 * Print an AST.
 *
 * @param to The stream to print to.
 * @param node The root of the AST to print.
 ***/
void astPrint(FILE *to, ASTNode *node);

/***
 * Free an AST.
 *
 * @param node The root of the AST to free.
 ***/
void astFree(ASTNode *node);

#endif // AST_H
