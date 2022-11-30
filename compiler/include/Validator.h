#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdbool.h>
#include "Table.h"
#include "Compiler.h"
#include "Ast.h"

typedef struct validator {
    Compiler *compiler;
    ASTProgram *program;
    ModuleID current_module;
    ASTObj *current_function;
    BlockScope *current_scope;
    Table global_ids_in_current_module; // Table<ASTString, void>
    Table locals_already_declared_in_current_function; // Table<ASTString, void>
    bool found_main;
    bool had_error;
} Validator;

/***
 * Initialize A Validator.
 *
 * @param v The Validator to initialize.
 * @param c A Compiler.
 ***/
void validatorInit(Validator *v, Compiler *c);

/***
 * Free a Validator.
 *
 * @param v The Validator to free.
 ***/
void validatorFree(Validator *v);

/***
 * Validate an ASTProgram.
 *
 * @param v The Validatpr to use.
 * @param prog The ASTProgram to validate.
 ***/
bool validatorValidate(Validator *v, ASTProgram *prog);

#endif // VALIDATOR_H
