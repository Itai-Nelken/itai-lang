#include <stdio.h>
#include "common.h"
#include "Array.h"
#include "Ast/StringTable.h"
#include "Ast/Module.h"
#include "Ast/Program.h"

/* Helper functions */

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astModuleFree((ASTModule *)module);
}


/* ASTProgram functions */

void astProgramPrint(FILE *to, ASTProgram *prog) {
    if(!prog) {
        fputs("(null)", to);
        return;
    }

    // Print string table
    fputs("ASTProgram{\x1b[1mstrings:\x1b[0m ", to);
    stringTablePrint(to, &prog->strings);
    fputs(", \x1b[1mmodules:\x1b[0m [", to);
    ARRAY_FOR(i, prog->modules) {
        astModulePrint(to, ARRAY_GET_AS(ASTModule *, &prog->modules, i), false);
        if(i + 1 < arrayLength(&prog->modules)) {
            fputs(", ", to);
        }
    }
    fputs("]}", to);
}

void astProgramInit(ASTProgram *prog) {
    arrayInit(&prog->modules);
    stringTableInit(&prog->strings);
}

void astProgramFree(ASTProgram *prog) {
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
    stringTableFree(&prog->strings);
}

ModuleID astProgramNewModule(ASTProgram *prog, ASTString name) {
    ASTModule *m = astModuleNew(name);
    // Note: cast is uneccesary, but is included to make code more readable and understandable.
    return (ModuleID)arrayPush(&prog->modules, (void *)m);
}

ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id) {
    VERIFY(id < arrayLength(&prog->modules));
    ASTModule *m = ARRAY_GET_AS(ASTModule *, &prog->modules, id);
    VERIFY(m);
    return m;
}
