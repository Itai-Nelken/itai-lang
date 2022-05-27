#include <stdlib.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Symbols.h"
#include "Token.h"
#include "ast.h"

/*** callbacks ***/
static void free_ast_callback(void *node, void *cl) {
    UNUSED(cl);
    freeAST((ASTNode *)node);
}

static void free_function_callback(void *fn, void *cl) {
    UNUSED(cl);
    freeFunction((ASTFunction *)fn);
}

static void free_identifier_callback(void *identifier, void *cl) {
    UNUSED(cl);
    freeIdentifier((ASTIdentifier *)identifier);
}

ASTIdentifier *newIdentifier(char *str, int length) {
    ASTIdentifier *identifier;
    NEW0(identifier);
    identifier->text = stringNCopy(str, length);
    identifier->length = length;
    return identifier;
}

void freeIdentifier(ASTIdentifier *identifier) {
    freeString(identifier->text);
    identifier->text = NULL;
    identifier->length = 0;
    FREE(identifier);
}

ASTFunction *newFunction(ASTIdentifierNode *name) {
    assert(name);
    ASTFunction *fn;
    NEW0(fn);
    fn->name = name;
    initSymTable(&fn->identifiers, SYM_LOCAL, free_identifier_callback, NULL);
    initArray(&fn->locals);
    fn->body = NULL;
    return fn;
}

void freeFunction(ASTFunction *fn) {
    freeAST(AS_NODE(fn->name));
    freeSymTable(&fn->identifiers);
    arrayMap(&fn->locals, free_ast_callback, NULL);
    freeArray(&fn->locals);
    if(fn->body) {
        freeAST(AS_NODE(fn->body));
    }
    FREE(fn);
}

void initASTProg(ASTProg *astp) {
    initSymTable(&astp->identifiers, SYM_GLOBAL, free_identifier_callback, NULL);
    initArray(&astp->globals);
    initArray(&astp->functions);
}

void freeASTProg(ASTProg *astp) {
    freeSymTable(&astp->identifiers);
    arrayMap(&astp->globals, free_ast_callback, NULL);
    freeArray(&astp->globals);
    arrayMap(&astp->functions, free_function_callback, NULL);
    freeArray(&astp->functions);
}

ASTNode newNode(ASTNodeType type, Location loc) {
    ASTNode n = {type, loc};
    return n;
} 

// TODO: single newNode(type, loc, ...) function for
//        new nodes that decides the node's type according
//        to the node type.
ASTNode *newNumberNode(i32 value, Location loc) {
    ASTLiteralNode *n;
    NEW0(n);
    n->header = newNode(ND_NUM, loc);
    n->as.int32 = value;
    return AS_NODE(n);
}

ASTNode *newUnaryNode(ASTNodeType type, Location loc, ASTNode *child) {
    ASTUnaryNode *n;
    NEW0(n);
    n->header = newNode(type, loc);
    n->child = child;
    return AS_NODE(n);
}

ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right) {
    ASTBinaryNode *n;
    NEW0(n);
    n->header = newNode(type, loc);
    n->left = left;
    n->right = right;
    return AS_NODE(n);
}

ASTNode *newIdentifierNode(Location loc, int id) {
    ASTIdentifierNode *n;
    NEW0(n);
    n->header = newNode(ND_IDENTIFIER, loc);
    n->id = id;
    return AS_NODE(n);
}

ASTNode *newBlockNode(Location loc) {
    ASTBlockNode *n;
    NEW0(n);
    n->header = newNode(ND_BLOCK, loc);
    initArray(&n->body);
    return AS_NODE(n);
}

void freeAST(ASTNode *root) {
    if(root == NULL) {
        return;
    }
    switch(root->type) {
        // unary nodes
        case ND_EXPR_STMT:
            freeAST(AS_UNARY_NODE(root)->child);
            break;
        case ND_NEG:
            freeAST(AS_UNARY_NODE(root)->child);
            break;
        // binary nodes
        case ND_ASSIGN:
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_REM:
        case ND_EQ:
        case ND_NE:
        case ND_GT:
        case ND_GE:
        case ND_LT:
        case ND_LE:
        case ND_BIT_OR:
        case ND_XOR:
        case ND_BIT_AND:
        case ND_BIT_RSHIFT:
        case ND_BIT_LSHIFT:
            freeAST(AS_BINARY_NODE(root)->left);
            freeAST(AS_BINARY_NODE(root)->right);
            break;
        // everything else
        case ND_BLOCK:
            arrayMap(&AS_BLOCK_NODE(root)->body, free_ast_callback, NULL);
            freeArray(&AS_BLOCK_NODE(root)->body);
            break;
        case ND_IDENTIFIER:
        case ND_NUM:
            // nothing
            break;
        default:
            break;
    }
    FREE(root);
}
