#ifndef AST_EXPRNODE_H
#define AST_EXPRNODE_H

#include <stdio.h> // FILE
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Token.h"
#include "Ast/StringTable.h"
#include "Ast/Type.h"
#include "Ast/Object.h"

/**
 * An ASTExprNode represents an expression (i.e. addition, negation, call etc.)
 * The type ASTExprNode only stores common information for all types of expressions (such as location).
 * Any additional data required for a specific expression is stored in a struct with an ASTExprNode as the first field.
 * This effectively allows us to represent all expression nodes as an ASTExprNode, and then using the expression typ
 * to know what node we actually have, we can cast it to the correct expression node to access the rest of the stored data.
 **/
typedef enum ast_expression_types {
    // Constant value nodes.
    EXPR_NUMBER_CONSTANT,
    EXPR_STRING_CONSTANT,
    EXPR_BOOLEAN_CONSTANT,

    // Obj nodes
    EXPR_VARIABLE,
    EXPR_FUNCTION, // represents a function passed as a function argument for example.

    // Binary nodes
    EXPR_ASSIGN,
    EXPR_PROPERTY_ACCESS, // a.b.c
    EXPR_ADD,
    EXPR_SUBTRACT,
    EXPR_MULTIPLY,
    EXPR_DIVIDE,
    EXPR_EQ, EXPR_NE,
    EXPR_LT, EXPR_LE,
    EXPR_GT, EXPR_GE,
    EXPR_LOGICAL_AND, EXPR_LOGICAL_OR, // &&, ||
    EXPR_SCOPE_RESOLUTION, // ::

    // Unary nodes
    EXPR_NEGATE,
    EXPR_LOGICAL_NOT, // !<expr>
    EXPR_ADDROF, // &<obj>
    EXPR_DEREF, // *<obj>

    // Call node
    EXPR_CALL,

    // Other nodes.
    EXPR_IDENTIFIER,

    EXPR_TYPE_COUNT // not a node type.
} ASTExprType;

typedef struct ast_expression_node {
    ASTExprType type;
    Location location;
    Type *dataType;
} ASTExprNode;


typedef struct ast_constant_value_expression {
    ASTExprNode header;
    union {
        u64 number;
        ASTString string;
        bool boolean;
    } as;
} ASTConstantValueExpr;

typedef struct ast_object_expression {
    ASTExprNode header;
    ASTObj *obj;
} ASTObjExpr;

typedef struct ast_binary_expression {
    ASTExprNode header;
    ASTExprNode *lhs, *rhs;
} ASTBinaryExpr;

typedef struct ast_unary_expression {
    ASTExprNode header;
    ASTExprNode *operand;
} ASTUnaryExpr;

typedef struct ast_call_expression {
    ASTExprNode header;
    ASTExprNode *callee;
    Array arguments; // Array<ASTExprNode *>
} ASTCallExpr;

typedef struct ast_identifier_expression {
    ASTExprNode header;
    ASTString id;
} ASTIdentifierExpr;


/**
 * Pretty print an ASTExprNode.
 *
 * @param to The stream to print to.
 * @param n The node to print.
 **/
void astExprPrint(FILE *to, ASTExprNode *n);

/**
 * Create a new ASTConstanValueExpr.
 * NOTE: Caller must initialize either node.as.number or node.as.string.
 *
 * @param a The allocator to use to allocate the node.
 * @param type The type of the node (e.g. EXPR_NUMBER_CONSTANT)
 * @param loc The location of the node.
 * @param valueTy The type of the value (e.g. the type representing u64 for a number constant)
 * @return A new node initialized with the above data.
 **/
ASTConstantValueExpr *astConstantValueExprNew(Allocator *a, ASTExprType type, Location loc, Type *valueTy);

/**
 * Create a new ASTObjExpr.
 * C.R.E for obj == NULL.
 *
 * @param a The allocator to use to allocate the node.
 * @param type The type of the node.
 * @param loc The location of the node.
 * @param obj The ASTObj that this node will represent.
 * @return A new node initialized with the above data.
 **/
ASTObjExpr *astObjExprNew(Allocator *a, ASTExprType type, Location loc, ASTObj *obj);

/**
 * Create a new ASTBinaryExpr.
 *
 * @param a The allocator to use to allocate the node.
 * @param type The type of the node.
 * @param loc The location of the node.
 * @param exprTy The data type of the expression.
 * @param lhs The left node.
 * @param rhs The right node.
 * @return A new node initialized with the above data.
 **/
ASTBinaryExpr *astBinaryExprNew(Allocator *a, ASTExprType type, Location loc, Type *exprTy, ASTExprNode *lhs, ASTExprNode *rhs);

/**
 * Create a new ASTUnaryExpr.
 *
 * @param a The allocator to use to allocate the node.
 * @param type The type of the node.
 * @param loc The location of the node.
 * @param exprTy The data type of the expression.
 * @param operand The operand node.
 * @return A new node initialized with the above data.
 **/
ASTUnaryExpr *astUnaryExprNew(Allocator *a, ASTExprType type, Location loc, Type *exprTy, ASTExprNode *operand);

/**
 * Create a new ASTCallExpr.
 *
 * @param a The allocator to use to allocate the node.
 * @param type The type of the node.
 * @param loc The location of the node.
 * @param exprTy The data type of the expression (should be return type of callee).
 * @param callee The ASTExprNode representing the callee.
 * @param arguments an Array<ASTExprNode *> with each node representing an argument to the callee.
 * @return A new node initialized with the above data.
 **/
ASTCallExpr *astCallExprNew(Allocator *a, Location loc, Type *exprTy, ASTExprNode *callee, Array *arguments);

/**
 * Create a new ASTIdentifierExpr.
 *
 * @param a The allocator to use to allocate the node.
 * @param loc The location of the node.
 * @param id The identifier as an ASTString.
 * @return A new node initialized with the above data.
 **/
ASTIdentifierExpr *astIdentifierExprNew(Allocator *a, Location loc, ASTString id);

#endif // AST_EXPRNODE_H