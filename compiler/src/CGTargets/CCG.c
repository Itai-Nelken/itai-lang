#include <stdio.h>
#include <stdarg.h>
#include "Ast/Ast.h"
#include "Codegen.h"
#include "CGTargets/CCG.h"

typedef enum ccg_fn_state {
    FN_PREDECL,
    FN_DEFINE,
    FN_DECLARED
} CCGFNState;

static void print(TargetCCG *ccg, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(ccg->output, format, ap);
    va_end(ap);
}

static void beginModule(ASTModule *m, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    // Note: Eventually I might want to change files here I think.
    print(ccg, "// Begin module '%s'\n", m->name);
}

static void endModule(ASTModule *m, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    // See note in beginModule().
    print(ccg, "// End module '%s'\n", m->name);
}

static void declType(Type *ty, void *cl) {
    UNUSED(ty);
    UNUSED(cl);
    // nothing (for now.)
}

static void declVar(ASTObj *var, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    print(ccg, "%s %s", var->dataType->name, var->name);
}

static void beginStruct(ASTObj *st, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    print(ccg, "struct %s {\n", st->name);
}

static void endStruct(ASTObj *st, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    print(ccg, "} // struct %s\n", st->name);
}

static void beginFn(ASTObj *fn, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    print(ccg, "%s %s(", fn->as.fn.returnType->name, fn->name);
    ARRAY_FOR(i, fn->as.fn.parameters) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        print(ccg, "%s %s", param->dataType->name, param->name);
        if(i + 1 < arrayLength(&fn->as.fn.parameters)) {
            print(ccg, ", ");
        }
    }
    if(tableGet(&ccg->declaredFunctions, (void *)fn->name) == NULL) {
        tableSet(&ccg->declaredFunctions, (void *)fn->name, (void *)FN_PREDECL);
        return;
    }
    print(ccg, "{\n");
}

static void endFn(ASTObj *fn, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    TableItem *item = tableGet(&ccg->declaredFunctions, (void *)fn->name);
    VERIFY(item);
    CCGFNState newState;
    switch((CCGFNState)item->value) {
        case FN_PREDECL:
            newState = FN_DEFINE;
            return;
        case FN_DEFINE:
            newState = FN_DECLARED;
            break;
        case FN_DECLARED:
        default:
            UNREACHABLE();
    }
    tableSet(&ccg->declaredFunctions, (void *)fn->name, (void *)newState);
    print(ccg, "}\n");
}

static void expr(ASTExprNode *expr, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    UNUSED(expr);
    print(ccg, "// expr\n");
}

static void stmt(ASTStmtNode *stmt, void *cl) {
    TargetCCG *ccg = (TargetCCG *)cl;
    UNUSED(stmt);
    print(ccg, "// stmt\n");
}

void targetCCGInit(TargetCCG *ccg, FILE *output) {
    ccg->output = output;
    tableInit(&ccg->declaredFunctions, NULL, NULL);
}

void targetCCGFree(TargetCCG *ccg) {
    tableFree(&ccg->declaredFunctions);
    ccg->output = NULL;
}

CGInterface makeTargetCCGInterface(TargetCCG *ccg) {
    CGInterface cg = {
        .beginModule = beginModule,
        .endModule = endModule,
        .declType = declType,
        .declVar = declVar,
        .beginStruct = beginStruct,
        .endStruct = endStruct,
        .beginFn = beginFn,
        .endFn = endFn,
        .expr = expr,
        .stmt = stmt,
        .cl = (void *)ccg,
    };
    return cg;
}

void targetCCGGenerate(TargetCCG *ccg, ASTProgram *prog) {
    CGInterface cg = makeTargetCCGInterface(ccg);
    CGRequest requests[] = {
        //CGREQ_TYPES,
        CGREQ_STRUCTS,
        CGREQ_VARIABLES,
        CGREQ_FUNCTIONS, // predecl
        CGREQ_FUNCTIONS, // decl
        CGREQ_END_OF_LIST
    };
    cgGenerate(&cg, requests, prog);
}
