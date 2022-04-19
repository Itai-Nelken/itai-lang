#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stddef.h> // size_t
#include <stdbool.h>
#include "Array.h"
#include "ast.h"

typedef enum registers {
    R0, R1, R2, R3, R4,
    _REG_COUNT, NOREG
} Register;

typedef struct code_generator {
    ASTProg *program;
    char *buffer;
    size_t buffer_size;
    FILE *buff, *out; // buff is actually the buffer, out is the real file

    bool print_stmt_used;
    bool had_error;
    Array globals; // Array<ASTObj *>
    int counter;
    bool free_regs[_REG_COUNT];
    int spilled_regs;
} CodeGenerator;

void initCodegen(CodeGenerator *cg, ASTProg *program, FILE *file);
void freeCodegen(CodeGenerator *cg);

void codegen(CodeGenerator *cg);

#endif // CODEGEN_H
