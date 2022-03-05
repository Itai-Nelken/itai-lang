#include <stdlib.h>
#include "memory.h"
#include "ast.h"

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
