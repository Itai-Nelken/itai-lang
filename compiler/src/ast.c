#include <stdlib.h>
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Token.h"
#include "ast.h"

void initASTProg(ASTProg *astp) {
    initArray(&astp->declarations);
}

void freeASTProg(ASTProg *astp) {
    for(size_t i = 0; i < astp->declarations.used; ++i) {
        freeAST(astp->declarations.data[i]);
    }
    freeArray(&astp->declarations);
}

ASTNode *newNode(ASTNodeType type, ASTNode *left, ASTNode *right, Location loc) {
    ASTNode *n = CALLOC(1, sizeof(*n));
    n->type = type;
    n->left = left;
    n->right = right;
    n->loc = loc;

    return n;
}

void freeAST(ASTNode *root) {
    if(root == NULL) {
        return;
    }
    freeAST(root->left);
    freeAST(root->right);
    switch(root->type) {
        case ND_FN:
            freeString(root->as.fn.name);
            freeArray(&root->as.fn.locals);
            freeAST(root->as.fn.body);
            break;
        case ND_VAR:
            freeString(root->as.var.name);
            break;
        case ND_BLOCK:
            for(int i = 0; i < (int)root->as.body.used; ++i) {
                freeAST(ARRAY_GET_AS(ASTNode *, &root->as.body, i));
            }
            break;
        case ND_IF:
        case ND_LOOP:
            freeAST(root->as.conditional.initializer);
            freeAST(root->as.conditional.increment);
            freeAST(root->as.conditional.condition);
            freeAST(root->as.conditional.then);
            freeAST(root->as.conditional.els);
            break;
        default:
            break;
    }
    FREE(root);
}

ASTNode *newNumberNode(int value, Location loc) {
    ASTNode *n = newNode(ND_NUM, NULL, NULL, loc);
    n->as.literal.int32 = value;
    return n;
}

ASTNode *newUnaryNode(ASTNodeType type, ASTNode *left, Location loc) {
    return newNode(type, left, NULL, loc);
}
