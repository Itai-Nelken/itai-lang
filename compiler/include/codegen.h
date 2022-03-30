#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "ast.h"

typedef enum registers {
    R0, R1, R2, R3,
    _REG_COUNT
} Register;

typedef struct code_generator {
    ASTProg *program;
    FILE *out;

    bool free_regs[_REG_COUNT];
} CodeGenerator;

void initCodegen(CodeGenerator *cg, ASTProg *program, FILE *file);
void freeCodegen(CodeGenerator *cg);

void codegen(CodeGenerator *cg);

#endif // CODEGEN_H
