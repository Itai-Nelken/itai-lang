#include <stdbool.h>
#include "common.h"
#include "ast.h"
#include "Validator.h"

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

static void _validate(void *node, void *cl) {
    bool *had_error = (bool *)cl;
    ASTNode *n = (ASTNode *)node;
    switch(n->type) {
        // check that unary nodes have a valid operand
        case ND_EXPR_STMT:
        case ND_NEG:
        case ND_PRINT:
        case ND_RETURN:
            *had_error = validate_unary(n);
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
            *had_error = validate_binary(n);
            break;
        // everything else isn't handled yet
        default:
            break;
    }
}

// TODO: finish
bool validate(ASTProg *prog) {
    bool had_error = false;
    arrayMap(&prog->declarations, _validate, (void *)&had_error);
    return !had_error;
}
