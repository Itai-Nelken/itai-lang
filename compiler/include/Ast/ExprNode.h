#ifndef AST_EXPRNODE_H
#define AST_EXPRNODE_H

/**
 * An ASTExprNode represents an expression (i.e. addition, negation, call etc.)
 * The type ASTExprNode only stores common information for all types of expressions (such as location).
 * Any additional data required for a specific expression is stored in a struct with an ASTExprNode as the first field.
 * This effectively allows us to represent all expression nodes as an ASTExprNode, and then using the expression typ
 * to know what node we actually have, we can cast it to the correct expression node to access the rest of the stored data.
 **/
typedef enum ast_expression_types {A} ASTExprType;
typedef struct ast_expression_node {} ASTExprNode;

#endif // AST_EXPRNODE_H