#include <stdbool.h>
#include "common.h"
#include "ast.h"
#include "Validator.h"

static void validate_global(void *global, void *cl) {
    ASTNode *g = (ASTNode *)global;
    bool *had_error = (bool *)cl;

    if(g->type == ND_ASSIGN && (g->left == NULL || g->right == NULL)) {
        *had_error = true;
        return;
    }
    ASTObjType type = g->type == ND_ASSIGN ? g->left->as.var.type : g->as.var.type;
    if(type != OBJ_GLOBAL) {
        *had_error = true;
    }
}

static void validate_local(void *local, void *cl) {
    ASTObj *l = (ASTObj *)local;
    bool *had_error = (bool *)cl;

    if(l->type != OBJ_LOCAL) {
        *had_error = true;
    }
}

static bool validate_unary(ASTNode *n) {
    if(n->left == NULL) {
        return true;
    }
    return false;
}

static bool validate_binary(ASTNode *n) {
    if(n->left == NULL || n->right == NULL) {
        return true;
    }
    return false;
}

static void validate_ast(ASTNode *node, bool *had_error) {
    switch(node->type) {
        // check that unary nodes have a valid operand
        case ND_EXPR_STMT:
        case ND_NEG:
        case ND_PRINT:
        case ND_RETURN:
            *had_error = validate_unary(node);
            break;
        // check that binary nodes have valid operands
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
            *had_error = validate_binary(node);
            break;
        // everything else isn't handled yet
        default:
            break;
    }
}

static void validate_function(void *function, void *cl) {
    ASTFunction *fn = (ASTFunction *)function;
    bool *had_error = (bool *)cl;

    arrayMap(&fn->locals, validate_local, cl);
    validate_ast(fn->body, had_error);
}

//  == TODO ==
// * check that fn main() exists
// * check that no global variable & function names clash
bool validate(ASTProg *prog) {
    bool had_error = false;
    arrayMap(&prog->globals, validate_global, (void *)&had_error);
    arrayMap(&prog->functions, validate_function, (void *)&had_error);
    return !had_error;
}
