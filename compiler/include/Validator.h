#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdbool.h>
#include "Compiler.h"
#include "Ast.h"

typedef struct validator {
    Compiler *compiler;
    ASTProgram *program;
    ModuleID current_module;
    bool had_error;
} Validator;

void validatorInit(Validator *v, Compiler *c);
void validatorFree(Validator *v);
bool validatorValidate(Validator *v, ASTProgram *prog);

#endif // VALIDATOR_H
