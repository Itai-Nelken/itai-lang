#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "Array.h"
#include "Token.h"
#include "Compiler.h"

typedef struct parser {
    Compiler *compiler;
    Array *tokens;
    usize current_token;
} Parser;

// Update astPrint(), astFree(), node_type_name(), node_name(), token_to_node_type(), all the is_*_*() functions, and the rule table all in Parser.c
// when adding new types.
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

#define AS_NODE(node) ((ASTNode *)(node))
#define AS_NUMBER_NODE(node) ((ASTNumberNode *)(node))
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)(node))
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)(node))

/***
 * Initialize a Parser.
 *
 * @param p The Parser to initialize.
 * @param c A pointer to an initialized Compiler.
 ***/
void parserInit(Parser *p, Compiler *c);

/***
 * Free a Parser.
 *
 * @param p The Parser to free.
 ***/
void parserFree(Parser *p);

/***
 * Parse an AST from the token array.
 *
 * @param p An initialized Parser.
 * @param tokens An initialized Array<Token *> containing the tokens from the Scanner.
 ***/
ASTNode *parserParse(Parser *p, Array *tokens);

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

#endif // PARSER_H
