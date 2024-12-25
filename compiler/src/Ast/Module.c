#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Arena.h"
#include "Table.h"
#include "Ast/Type.h"
#include "Ast/StmtNode.h"
#include "Ast/Module.h"

/* Helper functions */

static void free_type_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    typeFree((Type *)item->value);
}

static void print_type_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    typePrint(to, (Type *)item->value, true);
    if(!is_last) {
        fputs(", ", to);
    }
}


/* Module functions */

void astModulePrint(FILE *to, ASTModule *m, bool compact) {
    if(m == NULL) {
        fputs("(null)", to);
        return;
    }
    if(compact) {
        fprintf(to, "ASTModule{'%s'}", m->name);
        return;
    }

    fprintf(to, "ASTModule{\x1b[1mname:\x1b[0m '%s', \x1b[1mtypes:\x1b[0m [", m->name);
    tableMap(&m->types, print_type_table_callback, (void *)to);
    fputs("], \x1b[1mmoduleScope:\x1b[0m ", to);
    scopePrint(to, m->moduleScope, false);
    fputs(", \x1b[1mvariableDecls:\x1b[0m [", to);
    ARRAY_FOR(i, m->variableDecls) {
        astStmtPrint(to, ARRAY_GET_AS(ASTStmtNode *, &m->variableDecls, i));
        if(i + 1 < arrayLength(&m->variableDecls)) {
            fputs(", ", to);
        }
    }
    fputs("]}", to);
}

ASTModule *astModuleNew(ASTString name) {
    ASTModule *m;
    NEW0(m);
    arenaInit(&m->ast_allocator.storage);
    m->ast_allocator.alloc = arenaMakeAllocator(&m->ast_allocator.storage);
    m->name = name;
    m->moduleScope = scopeNew(NULL);
    tableInit(&m->types, NULL, NULL);
    arrayInit(&m->variableDecls);
    return m;
}

void astModuleFree(ASTModule *module) {
    scopeFree(module->moduleScope);
    arenaFree(&module->ast_allocator.storage);
    // Set allocator to NULL to prevent accidental use of the freed arena.
    // (although if someone is using the allocator in a freed module there are bigger problems to solve).
    module->ast_allocator.alloc = allocatorNew(NULL, NULL, NULL, NULL);
    tableMap(&module->types, free_type_callback, NULL);
    tableFree(&module->types);
    arrayFree(&module->variableDecls); // ASTNodes are owned by the arena in the module.
    FREE(module);
}

bool astModuleHasType(ASTModule *module, Type *ty) {
    return tableGet(&module->types, (void *)ty->name) != NULL;
}

void astModuleAddType(ASTModule *module, Type *ty) {
    VERIFY(!astModuleHasType(module, ty));
    tableSet(&module->types, (void *)ty->name, (void *)ty);
}

void astModuleAddVarDecl(ASTModule *module, ASTVarDeclStmt *decl) {
    VERIFY(decl != NULL);
    arrayPush(&module->variableDecls, (void *)decl);
}
