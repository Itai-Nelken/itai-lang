#ifndef AST_STMTNODE_H
#define AST_STMTNODE_H

/**
 * An ASTStmtNode represents a statement (i.e. a variable declaration, an if statement, loops etc.)
 * Statements use the same inheritance "trick" that we use for expressions.
 **/
typedef enum ast_statement_types {A} ASTStmtType;
typedef struct ast_statement_node {} ASTStmtNode;

#endif // AST_STMTNODE_H
