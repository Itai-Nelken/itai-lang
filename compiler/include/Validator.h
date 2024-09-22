#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "common.h"
#include "Compiler.h"
#include "memory.h"
#include "Ast/ParsedAst.h"
#include "Ast/CheckedAst.h"

typedef struct validator {
    Compiler *compiler;
    struct {
        Allocator *allocator;
        ModuleID module;
        ScopeID scope;
    } current;
    bool had_error;
    ASTParsedProgram *parsed_prog;
    ASTCheckedProgram *checked_prog;
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
 * @param checked_prog The ASTCheckedProgram to store the validated AST in.
 * @param parsed_prog The ASTParsedProgram to validate.
 * @return true on success, false on failure.
 ***/
bool validatorValidate(Validator *v, ASTCheckedProgram *checked_prog, ASTParsedProgram *parsed_prog);

#endif // VALIDATOR_H
