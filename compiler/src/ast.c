#include <stdlib.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Symbols.h"
#include "Token.h"
#include "ast.h"

void initASTProg(ASTProg *astp) {
    // NOTE: add free callback if values are added (not only keys)
    initSymTable(&astp->globals, SYM_GLOBAL, NULL, NULL);
    initArray(&astp->statements);
}

static void free_ast_callback(void *node, void *cl) {
    UNUSED(cl);
    freeAST((ASTNode *)node);
}

void freeASTProg(ASTProg *astp) {
    arrayMap(&astp->statements, free_ast_callback, NULL);
    freeArray(&astp->statements);
    freeSymTable(&astp->globals);
}

ASTNode newNode(ASTNodeType type, Location loc) {
    ASTNode n = {type, loc};
    return n;
} 

// TODO: single newNode(type, loc, ...) function for
//        new nodes that decides the node's type according
//        to the node type.
ASTNode *newNumberNode(i32 value, Location loc) {
    ASTLiteralNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(ND_NUM, loc);
    n->as.int32 = value;
    return AS_NODE(n);
}

ASTNode *newUnaryNode(ASTNodeType type, Location loc, ASTNode *child) {
    ASTUnaryNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(type, loc);
    n->child = child;
    return AS_NODE(n);
}

ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right) {
    ASTBinaryNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(type, loc);
    n->left = left;
    n->right = right;
    return AS_NODE(n);
}

ASTNode *newVarNode(Location loc, int id) {
    ASTVarNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(ND_VAR, loc);
    n->id = id;
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
        case ND_VAR:
        case ND_NUM:
            // nothing
            break;
        default:
            break;
    }
    FREE(root);
}
