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

ASTNode newNode(ASTNodeType type, Location loc) {
    ASTNode n = {type, loc};
    return n;
} 

void freeAST(ASTNode *root) {
    if(root == NULL) {
        return;
    }
    switch(root->type) {
        // all unary nodes
        case ND_NEG:
        case ND_PRINT:
        case ND_RETURN:
        case ND_EXPR_STMT:
            freeAST(AS_UNARY_NODE(root)->child);
            break;
        // all binary nodes
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
        case ND_NUM:
            break;
        case ND_FN_CALL:
        case ND_VAR:
            freeString(AS_OBJ_NODE(root)->obj.name);
            break;
        case ND_BLOCK:
            for(int i = 0; i < (int)AS_BLOCK_NODE(root)->body.used; ++i) {
                freeAST(ARRAY_GET_AS(ASTNode *, &AS_BLOCK_NODE(root)->body, i));
            }
            freeArray(&AS_BLOCK_NODE(root)->body);
            break;
        case ND_IF:
            freeAST(AS_CONDITIONAL_NODE(root)->condition);
            freeAST(AS_CONDITIONAL_NODE(root)->then);
            freeAST(AS_CONDITIONAL_NODE(root)->else_);
            break;
        case ND_LOOP:
            freeAST(AS_LOOP_NODE(root)->initializer);
            freeAST(AS_LOOP_NODE(root)->condition);
            freeAST(AS_LOOP_NODE(root)->increment);
            freeAST(AS_LOOP_NODE(root)->body);
            break;
        default:
            break;
    }
    FREE(root);
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

ASTNode *newObjNode(ASTNodeType type, Location loc, ASTObj obj) {
    ASTObjNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(type, loc);
    n->obj = obj;
    return AS_NODE(n);
}

ASTNode *newBlockNode(Location loc) {
    ASTBlockNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(ND_BLOCK, loc);
    initArray(&n->body);
    return AS_NODE(n);
}

ASTNode *newConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTNode *then, ASTNode *else_) {
    ASTConditionalNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(type, loc);
    n->condition = condition;
    n->then = then;
    n->else_ = else_;
    return AS_NODE(n);
}

ASTNode *newLoopNode(Location loc, ASTNode *initializer, ASTNode *condition, ASTNode *increment, ASTNode *body) {
    ASTLoopNode *n = CALLOC(1, sizeof(*n));
    n->header = newNode(ND_LOOP, loc);
    n->initializer = initializer;
    n->condition = condition;
    n->increment = increment;
    n->body = body;
    return AS_NODE(n);
}
