#include <stdlib.h>
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Token.h"
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
    if(root->type == ND_VAR) {
        freeString(root->as.var.name);
    } else if(root->type == ND_BLOCK) {
        for(int i = 0; i < (int)root->as.body.used; ++i) {
            freeAST(ARRAY_GET_AS(ASTNode *, &root->as.body, i));
        }
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
