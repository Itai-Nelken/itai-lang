#include <stdbool.h>
#include "common.h"
#include "ast.h"
#include "Validator.h"

void _validate(void *node, void *cl) {
    bool *had_error = (bool *)cl;
    ASTNode *n = (ASTNode *)node;
    UNUSED(n);
    UNUSED(had_error);
}

// TODO: finish
bool validate(ASTProg *prog) {
    bool had_error = true;
    arrayMap(&prog->statements, _validate, (void *)&had_error);
    return !had_error;
}
