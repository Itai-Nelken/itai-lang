#ifndef AST_MODULE_H
#define AST_MODULE_H

#include <stdbool.h>
#include <stdio.h>
#include "memory.h"
#include "StringTable.h"
#include "Arena.h"
#include "Table.h"
#include "StmtNode.h"
#include "Scope.h"

/**
 * A Module is basically a wrapper around a scope to represent a certain namespace that we call a module.
 * It owns all the AST nodes and all the scopes for the module it represents. The module scope/root scope represents the module namespace.
 * A Module also owns and maintains a type table for all the types declared in this module. The primitive types exist in all modules, TODO: but are owned by the ASTProgram???
 **/
typedef struct ast_module {
    struct {
        Arena storage;
        Allocator alloc;
    } ast_allocator;
    ASTString name;
    Table types; // Table<char *, Type *>
    Scope *moduleScope; // owned by this struct.
    // Holds any "global" variable declarations.
    Array variableDecls; // Array<ASTVarDeclStmt *>
} ASTModule;


/**
 * Pretty print an ASTModule.
 *
 * @param to The stream to print to.
 * @param m The module to print.
 * @param compact print compact (only module name)?
 **/
void astModulePrint(FILE *to, ASTModule *m, bool compact);

/**
 * Create a new ASTModule.
 *
 * @param name The name of the module.
 * @return A new ASTModule.
 **/
ASTModule *astModuleNew(ASTString name);

/**
 * Free an ASTModule.
 *
 * @param module The ASTModule to free.
 **/
void astModuleFree(ASTModule *module);

/**
 * Check if a type exists in a module.
 *
 * @param module The module to check in.
 * @param ty The type to check.
 * @return true if [ty] exists in [module], false otherwise.
 **/
bool astModuleHasType(ASTModule *module, Type *ty);

/**
 * Add a type to a module.
 *
 * @param module The module to add the type to.
 * @param ty The type to add (C.R.E for [ty] to already exist in the module).
 **/
void astModuleAddType(ASTModule *module, Type *ty);

/**
 * Add a variable declaration to an ASTModule.
 *
 * @param module The module to add the declaration to.
 * @param decl The declaration to add (C.R.E for decl == NULL).
 **/
void astModuleAddVarDecl(ASTModule *module, ASTVarDeclStmt *decl);

#endif // AST_MODULE_H
