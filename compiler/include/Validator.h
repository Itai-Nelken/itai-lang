#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdbool.h>
#include "memory.h" // Allocator
#include "Table.h"
#include "Compiler.h"
#include "Ast.h"

typedef struct validator {
    Compiler *compiler;
    ASTProgram *program;
    ModuleID current_module;
    Allocator *current_allocator;
    ASTObj *current_function;
    BlockScope *current_scope;
    Table global_ids_in_current_module; // Table<ASTString, void>
    Table visible_locals_in_current_function; // Table<ASTString, ASTObj *>
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
 * @param v The Validator to use.
 * @param prog The ASTProgram to validate.
 * @return true on success, false on failure.
 ***/
bool validatorValidate(Validator *v, ASTProgram *prog);

#endif // VALIDATOR_H
