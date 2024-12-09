#ifndef AST_PROGRAM_H
#define AST_PROGRAM_H

#include "Array.h"
#include "Ast/StringTable.h"
#include "Ast/Module.h"

/**
 * A Program represents a complete AST tree for a program including modules, scopes, objects etc.
 * Effectively, it stores an Array of Modules, and owns all of them.
 * It also owns the StringTable for the entire program.
 **/
typedef struct ast_program {
    StringTable strings;
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
 * @param name The name of the module to create.
 * @return A pointer to the newly created module. // TODO: or null on failure??
 **/
ASTModule *astProgramNewModule(ASTProgram *prog, ASTString name);

#endif // AST_PROGRAM_H
