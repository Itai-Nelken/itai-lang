#include <stdlib.h>
#include "memory.h"
#include "Array.h"
#include "ast.h"

void initASTProg(ASTProg *astp) {
    initArray(&astp->statements);
}

void freeASTProg(ASTProg *astp) {
    for(size_t i = 0; i < astp->statements.used; ++i) {
        freeAST(astp->statements.data[i]);
    }
    freeArray(&astp->statements);
}

ASTNode *newNode(ASTNodeType type, ASTNode *left, ASTNode *right) {
    ASTNode *n = CALLOC(1, sizeof(*n));
    n->type = type;
    n->left = left;
    n->right = right;

    return n;
}

void freeAST(ASTNode *root) {
    if(root == NULL) {
        return;
    }
    freeAST(root->left);
    freeAST(root->right);
    FREE(root);
}

ASTNode *newNumberNode(int value) {
    ASTNode *n = newNode(ND_NUM, NULL, NULL);
    n->as.literal.int32 = value;
    return n;
}

ASTNode *newUnaryNode(ASTNodeType type, ASTNode *left) {
    return newNode(type, left, NULL);
}
