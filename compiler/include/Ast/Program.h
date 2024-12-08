#ifndef AST_PROGRAM_H
#define AST_PROGRAM_H

#include "Array.h"
#include "Ast/Module.h"

/**
 * A Program represents a complete AST tree for a program including modules, scopes, objects etc.
 * Effectively, it stores an Array of Modules, and owns all of them.
 **/
typedef struct ast_program {
    Array modules; // Array<ASTModule *>;
} ASTProgram;


/**
 * Initializes an ASTProgram.
 *
 * @param prog The ASTProgram to initialize.
 **/
void astProgramInit(ASTProgram *prog);

/**
 * Frees all resources stored in an ASTProgram.
 *
 * @param prog The ASTProgram to free.
 **/
void astProgramFree(ASTProgram *prog);

/**
 * Creates a new ASTModule and adds it to the internal list.
 *
 * @param prog The ASTProgram to add a module in.
 * @return A pointer to the newly created module. // TODO: or null on failure??
 **/
ASTModule *astProgramNewModule(ASTProgram *prog);

#endif // AST_PROGRAM_H
