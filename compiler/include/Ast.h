#ifndef AST_H
#define AST_H

// TODO: Think through and draw a diagram of how I want my AST to look.
//       Things to keep in mind:
//         * Scope: separation between "namespace" and block scope? - No. they are similar enough that separating doesn't make sense (only difference is that block scope can't have structs.)
//            - In any case, holds all identifiers and their linked objects (functions, vars etc).
//         * Is separation between parsed and checked AST necessary? Future validator might do direct translation to bytecode. - No. this just complicates things.
//               Solution - have checkedAST methods (in validator?) that compute information that a checkedAST node would contain.
//            - Speaking of, consider converting AST to bytecode in validator. simplifying AST instead of complicating it makes sense.
//         * Try to keep as simple as possible!! (see bytecode idea).


/**
 * A Scope instance represents a namespace and possibly also a block scope.
 * It's a namespace because it contains all the variables, functions, and structs in a certain namespace (e.g. a module).
 * Since a block scope is basically a namespace where structs are not allowed, a Scope can also represent a block scope.
 * Each scope has an array of all its children. An example of a possible scope tree is as following:
 * Example code:
 * =================================
 * fn main() {
 *     if(Strings::compare(Args::args[0], "ten")) {
 *         return add(5 + 5);
 *     } else {
 *         return add(1 + 2);
 *     }
 * }
 * 
 * fn add(a: int, b: int) {
 *     return a + b;
 * }
 * =================================
 * Scope tree for example code:
 *                                               (root module scope)
 *                                                /               \
 *                            (fn main block scope)                (fn add block scope)
 *                             /            \
 *         (if stmt block scope)            (else stmt block scope)
 **/
typedef struct scope {} Scope;

/**
 * An ASTObj represents an object - i.e a variable, function, struct etc. (see ASTObjType).
 * An object stores the name, data type, and any additional information such as parameters or function body.
 **/
typedef enum ast_object_types {A} ASTObjType;
typedef struct ast_object {} ASTObj;

/**
 * An ASTExprNode represents an expression (i.e. addition, negation, call etc.)
 * The type ASTExprNode only stores common information for all types of expressions (such as location).
 * Any additional data required for a specific expression is stored in a struct with an ASTExprNode as the first field.
 * This effectively allows us to represent all expression nodes as an ASTExprNode, and then using the expression typ
 * to know what node we actually have, we can cast it to the correct expression node to access the rest of the stored data.
 **/
typedef enum ast_expression_types {A} ASTExprType;
typedef struct ast_expression_node {} ASTExprNode;

/**
 * An ASTStmtNode represents a statement (i.e. a variable declaration, an if statement, loops etc.)
 * Statements use the same inheritance "trick" that we use for expressions.
 **/
typedef enum ast_statement_types {A} ASTStmtType;
typedef struct ast_statement_node {} ASTStmtNode;

#endif // AST_H
