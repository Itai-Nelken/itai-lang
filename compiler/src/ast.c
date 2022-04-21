#include <stdlib.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Token.h"
#include "ast.h"

void initASTProg(ASTProg *astp) {
    initArray(&astp->functions);
    initArray(&astp->globals);
}

void freeASTProg(ASTProg *astp) {
    freeArray(&astp->functions);
    freeArray(&astp->globals);
}

ASTFunction *newFunction(const char *name, ASTNode *body) {
    ASTFunction *fn = CALLOC(1, sizeof(*fn));
    fn->name = stringCopy(name);
    fn->body = body;
    initArray(&fn->locals);
    return fn;
}

static void free_ast_obj_callback(void *obj, void *cl) {
    UNUSED(cl);
    ASTObj *o = (ASTObj *)obj;
    freeString(o->name);
}

void freeFunction(ASTFunction *fn) {
    freeString(fn->name);
    freeAST(fn->body);
    arrayMap(&fn->locals, free_ast_obj_callback, NULL);
    freeArray(&fn->locals);
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
