#ifndef AST_MODULE_H
#define AST_MODULE_H

#include "Arena.h"
#include "memory.h"
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
    Scope *module_scope; // owned by this struct.
} ASTModule;


ASTModule *astModuleNew(void);

void astModuleFree(ASTModule *module);

#endif // AST_MODULE_H
