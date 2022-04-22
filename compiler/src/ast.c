#include <stdlib.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Token.h"
#include "ast.h"

// callbacks for arrayMap()
static void free_ast_node_callback(void *node, void *cl) {
    UNUSED(cl);
    ASTNode *n = (ASTNode *)node;
    freeAST(n);
}

static void free_ast_obj_callback(void *obj, void *cl) {
    UNUSED(cl);
    ASTObj *o = (ASTObj *)obj;
    //freeString(o->name); // an ASTNode most likely has free'd this
    FREE(o);
}

static void free_ast_fn_callback(void *func, void *cl) {
    UNUSED(cl);
    ASTFunction *fn = (ASTFunction *)func;
    freeFunction(fn);
}

void initASTProg(ASTProg *astp) {
    initArray(&astp->functions);
    initArray(&astp->globals);
}

void freeASTProg(ASTProg *astp) {
    arrayMap(&astp->functions, free_ast_fn_callback, NULL);
    arrayMap(&astp->globals, free_ast_node_callback, NULL);
    freeArray(&astp->functions);
    freeArray(&astp->globals);
}

ASTFunction *newFunction(const char *name, Location loc, ASTNode *body) {
    ASTFunction *fn = CALLOC(1, sizeof(*fn));
    fn->name = stringCopy(name);
    fn->location = loc;
    fn->body = body;
    initArray(&fn->locals);
    return fn;
}

void freeFunction(ASTFunction *fn) {
    freeString(fn->name);
    freeAST(fn->body);
    arrayMap(&fn->locals, free_ast_obj_callback, NULL);
    freeArray(&fn->locals);
    FREE(fn);
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
        case ND_FN_CALL:
            freeString(root->as.name);
            break;
        case ND_VAR:
            freeString(root->as.var.name);
            break;
        case ND_BLOCK:
            for(int i = 0; i < (int)root->as.body.used; ++i) {
                freeAST(ARRAY_GET_AS(ASTNode *, &root->as.body, i));
            }
            freeArray(&root->as.body);
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
