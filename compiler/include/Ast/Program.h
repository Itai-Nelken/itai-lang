#ifndef AST_PROGRAM_H
#define AST_PROGRAM_H

#include <stdio.h>
#include "Array.h"
#include "Ast/StringTable.h"
#include "Ast/Module.h"

/**
 * A Program represents a complete AST tree for a program including modules, scopes, objects etc.
 * Effectively, it stores an Array of Modules, and owns all of them.
 * It also owns the StringTable for the entire program.
 **/
typedef struct ast_program {
    StringTable *strings;
    Array modules; // Array<ASTModule *>;
} ASTProgram;

// A ModuleID is an index into the modules array.
// It is used since it can represent an equivalent module in two different programs
// (such as a parsed program and a checked program).
typedef usize ModuleID;


/**
 * Pretty print an ASTProgram.
 *
 * @param to The stream to print to.
 * @param prog The ASTProgram to print.
 **/
void astProgramPrint(FILE *to, ASTProgram *prog);

/**
 * Initializes an ASTProgram.
 *
 * @param prog The ASTProgram to initialize.
 * @param st The string table to use in this program.
 **/
void astProgramInit(ASTProgram *prog, StringTable *st);

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
 * @return The ModuleID for the newly created module.
 **/
ModuleID astProgramNewModule(ASTProgram *prog, ASTString name);

/**
 * Get an ASTModule using its ModuleID.
 *
 * @param prog The ASTProgram to get the module from.
 * @param id The ModuleID of the module to get (C.R.E for id to be invalid).
 * @return The ASTModule the id refers to (guaranteed to not be NULL).
 **/
ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id);

#endif // AST_PROGRAM_H
