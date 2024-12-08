#include "common.h"
#include "Array.h"
#include "Ast/Module.h"
#include "Ast/Program.h"

/* Helper functions */

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astModuleFree((ASTModule *)module);
}


/* ASTProgram functions */

void astProgramInit(ASTProgram *prog) {
    arrayInit(&prog->modules);
}

void astProgramFree(ASTProgram *prog) {
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
}

ASTModule *astProgramNewModule(ASTProgram *prog) {
    ASTModule *m = astModuleNew();
    arrayPush(&prog->modules, (void *)m);
    return m;
}

