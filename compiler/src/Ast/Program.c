#include <stdio.h>
#include "common.h"
#include "Array.h"
#include "Table.h"
#include "Ast/StringTable.h"
#include "Ast/Module.h"
#include "Ast/Program.h"

/* Helper functions */

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astModuleFree((ASTModule *)module);
}

static unsigned int hashModuleId(void *mID) {
    return (usize)mID;
}

static bool cmpModuleId(void *a, void *b) {
    ModuleID aID = (ModuleID)a;
    ModuleID bID = (ModuleID)b;
    return aID == bID;
}


/* ASTProgram functions */

void astProgramPrint(FILE *to, ASTProgram *prog) {
    if(!prog) {
        fputs("(null)", to);
        return;
    }

    // Print string table
    fputs("ASTProgram{\x1b[1mstrings:\x1b[0m ", to);
    stringTablePrint(to, prog->strings);
    fputs(", \x1b[1mmodules:\x1b[0m [", to);
    ARRAY_FOR(i, prog->modules) {
        astModulePrint(to, ARRAY_GET_AS(ASTModule *, &prog->modules, i), false);
        if(i + 1 < arrayLength(&prog->modules)) {
            fputs(", ", to);
        }
    }
    fputs("]}", to);
}

void astProgramInit(ASTProgram *prog, StringTable *st) {
    arrayInit(&prog->modules);
    tableInit(&prog->moduleIDToIdx, hashModuleId, cmpModuleId);
    prog->strings = st;
}

void astProgramFree(ASTProgram *prog) {
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
    // nothing to free here
    tableFree(&prog->moduleIDToIdx);
}

ModuleID astProgramNewModule(ASTProgram *prog, ASTString name) {
    ASTModule *m = astModuleNew(name);
    usize actualIdx = arrayPush(&prog->modules, (void *)m);
    tableSet(&prog->moduleIDToIdx, (void *)actualIdx, (void *)actualIdx);
    m->id = actualIdx;
    // Note: the cast is uneccesary because ModuleID is actually usize,
    // but it is included to make the code more readable and understandable.
    return (ModuleID)actualIdx;
}

void astProgramNewModuleWithID(ASTProgram *prog, ModuleID id, ASTString name) {
    ASTModule *m = astModuleNew(name);
    m->id = id;
    usize actualIdx = arrayPush(&prog->modules, (void *)m);
    tableSet(&prog->moduleIDToIdx, (void *)id, (void *)actualIdx);
}

ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id) {
    TableItem *item = tableGet(&prog->moduleIDToIdx, (void *)id);
    VERIFY(item);
    usize actualIdx = (usize)item->value;
    ASTModule *m = ARRAY_GET_AS(ASTModule *, &prog->modules, actualIdx);
    VERIFY(m);
    return m;
}
