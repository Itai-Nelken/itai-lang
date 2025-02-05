#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include <stdbool.h>
#include "Ast/Program.h"
#include "Compiler.h"

typedef struct typechecker {
    Compiler *compiler;
    ASTProgram *program;
    bool hadError;

    struct {
        Scope *scope;
        ASTObj *function; // TODO: maybe change to current.object?
        ASTModule *module;
    } current;
} Typechecker;

/**
 * Initialize a Typechecker.
 *
 * @param typechecker The Typechecker to initialize.
 * @param c A Compiler instance to use to report errors.
 **/
void typecheckerInit(Typechecker *typechecker, Compiler *c);

/**
 * Free a Typechecker.
 *
 * @param typechecker The Typechecker to free.
 **/
void typecheckerFree(Typechecker *typechecker);

/**
 * Typecheck an ASTProgram.
 * C.R.E for 'prog' to be NULL.
 *
 * @param typechecker A Typechecker instance to use.
 * @param prog The ASTProgram to typecheck (note: MUST be validated with validatorValidate() first.)
 * @return true on success, false on failure.
 **/
bool typecheckerTypecheck(Typechecker *typechecker, ASTProgram *prog);


#endif // TYPECHECKER_H
