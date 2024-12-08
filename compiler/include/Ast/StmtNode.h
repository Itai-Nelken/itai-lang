#ifndef AST_STMTNODE_H
#define AST_STMTNODE_H

#include "memory.h"
#include "Array.h"
#include "Token.h"
#include "Object.h"
#include "ExprNode.h"

/**
 * An ASTStmtNode represents a statement (i.e. a variable declaration, an if statement, loops etc.)
 * Statements use the same inheritance "trick" that we use for expressions.
 **/
typedef enum ast_statement_types {
    // VarDecl nodes
    STMT_VAR_DECL,

    // Block nodes
    STMT_BLOCK,

    // Conditional nodes
    STMT_IF,

    // Loop nodes
    STMT_LOOP,

    // Expr nodes
    STMT_RETURN,
    STMT_EXPR,

    // Defer nodes
    STMT_DEFER,

    STMT_TYPE_COUNT // not an stmt type.
} ASTStmtType;

typedef struct ast_statement_node {
    ASTStmtType type;
    Location location;
} ASTStmtNode;

typedef struct ast_var_decl_statement {
    ASTStmtNode header;
    ASTObj *variable;
    ASTExprNode *initializer; // optional
} ASTVarDeclStmt;

typedef struct ast_block_statement {
    ASTStmtNode header;
    // TODO: scope? control flow.
    Array nodes; // Array<ASTStmtNode *>
} ASTBlockStmt;

typedef struct ast_conditional_statement {
    ASTStmtNode header;
    ASTExprNode *condition;
    ASTStmtNode *then;
    ASTExprNode *else_; // optional
} ASTConditionalStmt;

typedef struct ast_loop_statement {
    ASTStmtNode header;
    ASTExprNode *initializer; // optional
    ASTExprNode *condition;
    ASTExprNode *increment; // optional
    ASTBlockStmt *body;
} ASTLoopStmt;

typedef struct ast_expression_statement {
    ASTStmtNode header;
    ASTExprNode *expression;
} ASTExprStmt;

typedef struct ast_defer_statement {
    ASTStmtNode header;
    ASTStmtNode *body;
} ASTDeferStmt;


/**
 * Create a new ASTVarDeclStmt node.
 *
 * @param a The allocator to use to allocate the node.
 * @param loc The location of the node.
 * @param var The variable being declared.
 * @param init The initial value (if exists, otherwise NULL).
 * @return A new node initialized with the above data.
 */
ASTVarDeclStmt *astVarDeclStmtNew(Allocator *a, Location loc, ASTObj *var, ASTExprNode *init);

/**
 * Create a new ASTBlockStmt node.
 *
 * @param a The allocator to use to allocate the node.
 * @param loc The location of the node.
 * @param nodes A pointer to an Array<ASTStmtNode *> containing the body of the block.
 * @return A new node initialized with the above data.
 */
ASTBlockStmt *astBlockStmtNew(Allocator *a, Location loc, Array *nodes);

/**
 * Create a new ASTConditionalStmt node.
 *
 * @param a The allocator to use to allocate the node.
 * @param loc The location of the node.
 * @param cond The condition expression.
 * @param then The statement that represents what will be executed if the condition is true.
 * @param else_ The statement that represents what will be executed if the condition is false (optional).
 * @return A new node initialized with the above data.
 */
ASTConditionalStmt *astConditionalStmtNew(Allocator *a, Location loc, ASTExprNode *cond, ASTStmtNode *then, ASTStmtNode *else_);

/**
 * Create a new ASTLoopStmt node.
 *
 * @param a The allocator to use to allocate the node.
 * @param loc The location of the node.
 * @param init The initializer expression (optional).
 * @param cond The condition of the loop.
 * @param inc The increment expression (optional).
 * @param body The body of the loop.
 * @return A new node initialized with the above data.
 */
ASTLoopStmt *astLoopStmtNew(Allocator *a, Location loc, ASTExprNode *init, ASTExprNode *cond, ASTExprNode *inc, ASTBlockStmt *body);

/**
 * Create a new ASTExprStmt node.
 *
 * @param a The allocator to use to allocate the node.
 * @param type The type of the node (e.g. STMT_EXPR).
 * @param loc The location of the node.
 * @param expr The expression this node represents.
 * @return A new node initialized with the above data.
 */
ASTExprStmt *astExprStmtNew(Allocator *a, ASTStmtType type, Location loc, ASTExprNode *expr);

/**
 * Create a new ASTDeferStmt node.
 *
 * @param a The allocator to use to allocate the node.
 * @param loc The location of the node.
 * @param body The body of the defer statement.
 * @return A new node initialized with the above data.
 */
ASTDeferStmt *astDeferStmtNew(Allocator *a, Location loc, ASTStmtNode *body);

#endif // AST_STMTNODE_H
