#include "common.h"
#include "memory.h"
#include "Arena.h"
#include "Table.h"
#include "Ast/Type.h"
#include "Ast/Module.h"

/* Helper functions */

static void free_type_callback(void *type, void *cl) {
    UNUSED(cl);
    typeFree((Type *)type);
}


/* Module functions */

ASTModule *astModuleNew(void) {
    ASTModule *m;
    NEW0(m);
    arenaInit(&m->ast_allocator.storage);
    m->ast_allocator.alloc = arenaMakeAllocator(&m->ast_allocator.storage);
    m->moduleScope = scopeNew(NULL);
    tableInit(&m->types, NULL, NULL);
}

void astModuleFree(ASTModule *module) {
    scopeFree(module->moduleScope);
    arenaFree(&module->ast_allocator.storage);
    // Set allocator to NULL to prevent accidental use of the freed arena.
    // (although if someone is using the allocator in a freed module there are bigger problems to solve).
    module->ast_allocator.alloc = allocatorNew(NULL, NULL, NULL, NULL);
    tableMap(&module->types, free_type_callback, NULL);
    tableFree(&module->types);
    FREE(module);
}

bool astModuleHasType(ASTModule *module, Type *ty) {
    return tableGet(&module->types, (void *)ty->name) != NULL;
}

void astModuleAddType(ASTModule *module, Type *ty) {
    VERIFY(!astModuleHasType(module, ty));
    TableItem *item = NULL;
    tableSet(&module->types, (void *)ty->name, (void *)ty);
}
