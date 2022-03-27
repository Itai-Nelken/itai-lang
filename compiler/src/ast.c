#include <stdlib.h>
#include "memory.h"
#include "ast.h"

void initASTProg(ASTProg *astp) {
    astp->statements = NULL;
    astp->capacity = astp->used = 0;
}

void freeASTProg(ASTProg *astp) {
    for(size_t i = 0; i < astp->used; ++i) {
        freeAST(astp->statements[i]);
    }
    FREE(astp->statements);
    astp->statements = NULL;
}

void ASTProgPush(ASTProg *astp, ASTNode *node) {
    if(astp->used >= astp->capacity) {
        astp->capacity = astp->capacity == 0 ? 8 : astp->capacity;
        astp->capacity *= 2;
        astp->statements = REALLOC(astp->statements, astp->capacity);
    }
    astp->statements[astp->used++] = node;
}

ASTNode *ASTProgAt(ASTProg *astp, int index) {
    if((int)index > astp->used) {
        return NULL;
    }
    return astp->statements[index];
}

ASTNode *ASTProgPop(ASTProg *astp) {
    if(astp->used == 0) {
        return NULL;
    }
    return astp->statements[astp->used--];
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
    n->literal.int32 = value;
    return n;
}

ASTNode *newUnaryNode(ASTNodeType type, ASTNode *left) {
    return newNode(type, left, NULL);
}
