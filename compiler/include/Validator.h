#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdbool.h>
#include "Compiler.h"
#include "Ast/Scope.h"
#include "Ast/Program.h"

typedef struct validator {
    ASTProgram *parsedProgram;
    ASTProgram *checkedProgram;
    Compiler *compiler;
    bool hadError;
    struct {
        // TODO: add comments specifiyng if each field may be NULL or invalid and when.
        Scope *parsedScope, *checkedScope;
        ASTObj *function;
        ASTObj *objParent; // May be NULL.
        ModuleID module;
    } current;
} Validator;

/**
 * Initialize a Validator.
 *
 * @param v The validator to initalize.
 * @param c A Compiler to use.
 **/
void validatorInit(Validator *v, Compiler *c);

/**
 * Free a Validator.
 *
 * @param v The Validator to free.
 **/
void validatorFree(Validator *v);

/**
 * Validate an ASTProg.
 *
 * @param v A Validator to use.
 * @param parsedProg The ASTProgram to validate.
 * @param checkedProg The ASTProgram to store the validated AST in.
 * @return true on success, false on failure.
 **/
bool validatorValidate(Validator *v, ASTProgram *parsedProg, ASTProgram *checkedProg);

#endif // VALIDATOR_H
