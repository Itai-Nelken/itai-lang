#include "memory.h"
#include "Arena.h"
#include "Ast/Module.h"
#include "Ast/Module.h"


ASTModule *astModuleNew(void) {
    ASTModule *m;
    NEW0(m);
    arenaInit(&m->ast_allocator.storage);
    m->ast_allocator.alloc = arenaMakeAllocator(&m->ast_allocator.storage);
    m->moduleScope = scopeNew(NULL);
}

void astModuleFree(ASTModule *module) {
    scopeFree(module->moduleScope);
    arenaFree(&module->ast_allocator.storage);
    // Set allocator to NULL to prevent accidental use of the freed arena.
    // (although if someone is using the allocator in a freed module there are bigger problems to solve).
    module->ast_allocator.alloc = allocatorNew(NULL, NULL, NULL, NULL);
    FREE(module);
}
