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
 *
 * Modules are a bit weird because they have a ModuleID that allows for a single module to be
 * represented by multiple instances of ASTModule over the compilation process (only one at a time though).
 * However, the modules array itself won't necessarily have the same order, for example the validator
 * sorts the module by their dependency on each other (imports).
 * The solution I'm using is having a table that maps moduleIDs to the actual index of each module
 * in the modules array. That way the order in which the modules are stored doesn't matter
 * and doesn't change the ModuleIDs.
 * The reason I'm using a hashtable even though an array would suffice theoretically
 * is that I need to be able to insert ModuleIDs out of order, and my Table interface/impl
 * provides that ability.
 **/
typedef struct ast_program {
    StringTable *strings;
    Array modules; // Array<ASTModule *>;
    Table moduleIDToIdx; // Table<ModuleID, usize>
} ASTProgram;

// A ModuleID is an index into the moduleIDToIdx array.
// It is used since it can represent an equivalent module in two different programs
// (such as a parsed program and a checked program).
// see explanation above ASTProgram definition for what it actually indexes.
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
 * Creates a new ASTModule associated with a specific ModuleID and adds it to the internal list.
 *
 * @param prog The ASTProgram to add a module in.
 * @param id The ID to associate the module with.
 * @param name The name of the module to create.
 * @return The ModuleID for the newly created module.
 **/
void astProgramNewModuleWithID(ASTProgram *prog, ModuleID id, ASTString name);

/**
 * Get an ASTModule using its ModuleID.
 *
 * @param prog The ASTProgram to get the module from.
 * @param id The ModuleID of the module to get (C.R.E for id to be invalid).
 * @return The ASTModule the id refers to (guaranteed to not be NULL).
 **/
ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id);

#endif // AST_PROGRAM_H
