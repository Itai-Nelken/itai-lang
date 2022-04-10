#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "Array.h"
#include "ast.h"

typedef enum registers {
    R0, R1, R2, R3, R4,
    _REG_COUNT, NOREG
} Register;

typedef struct code_generator {
    ASTProg *program;
    FILE *out;

    bool had_error;
    Array globals;
    bool free_regs[_REG_COUNT];
    int spilled_regs;
} CodeGenerator;

void initCodegen(CodeGenerator *cg, ASTProg *program, FILE *file);
void freeCodegen(CodeGenerator *cg);

void codegen(CodeGenerator *cg);

#endif // CODEGEN_H
