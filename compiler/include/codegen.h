#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "ast.h"

typedef struct code_generator {
    ASTProg *program;
    FILE *out;

    // internal state
    bool stack_has_space;
} CodeGenerator;

void initCodegen(CodeGenerator *cg, ASTProg *program, FILE *file);
void freeCodegen(CodeGenerator *cg);

void codegen(CodeGenerator *cg);

#endif // CODEGEN_H
