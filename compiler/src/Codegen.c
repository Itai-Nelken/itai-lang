#include "common.h"
#include "Array.h"
#include "Ast/Ast.h"
#include "Codegen.h"


static void req_type_callback(TableItem *item, bool isLast, void *cl) {
    UNUSED(isLast);
    CGInterface *cg = (CGInterface *)cl;
    Type *ty = (Type *)item->value;
    cg->declType(ty, cg->cl);
}

void req_variables(CGInterface *cg) {
    VERIFY(cg->data.current.scope->depth == SCOPE_DEPTH_MODULE_NAMESPACE);
    ARRAY_FOR(i, cg->data.current.module->variableDecls) {
        ASTVarDeclStmt *vdecl = ARRAY_GET_AS(ASTVarDeclStmt *, &cg->data.current.module->variableDecls, i);
        cg->declVar(vdecl->variable, cg->cl);
        if(vdecl->initializer) {
            cg->stmt(NODE_AS(ASTStmtNode, vdecl), cg->cl);
        }
        // TODO: emit empty init otherwise?
    }
}

void req_function(CGInterface *cg, ASTObj *fn) {
    cg->beginFn(fn, cg->cl);
    cg->stmt(NODE_AS(ASTStmtNode, fn->as.fn.body), cg->cl);
    cg->endFn(fn, cg->cl);
}

void req_struct(CGInterface *cg, ASTObj *st) {
    VERIFY(st->as.structure.scope->parent == cg->data.current.scope);
    cg->beginStruct(st, cg->cl);
    cg->data.current.scope = st->as.structure.scope;

    Array objects;
    arrayInitSized(&objects, scopeGetNumObjects(cg->data.current.scope));
    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        if(obj->type == OBJ_VAR) {
            cg->declVar(obj, cg->cl);
        }
    }
    arrayFree(&objects);

    cg->data.current.scope = cg->data.current.scope->parent;
    cg->endStruct(st, cg->cl);
}


void cgGenerate(CGInterface *cg, CGRequest requests[], ASTProgram *prog) {
    cg->data.prog = prog;
    Array objects;
    arrayInit(&objects);
    ARRAY_FOR(moduleIdx, cg->data.prog->modules) {
        arrayClear(&objects);
        ASTModule *m = ARRAY_GET_AS(ASTModule *, &cg->data.prog->modules, moduleIdx);
        cg->data.current.scope = m->moduleScope;
        cg->beginModule(m, cg->cl);
        scopeGetAllObjects(m->moduleScope, &objects);
        CGRequest req = requests[0];
        for(usize i = 1; req != CGREQ_END_OF_LIST; req = requests[i++]) {
            switch(req) {
                case CGREQ_TYPES:
                    tableMap(&cg->data.current.module->types, req_type_callback, (void *)cg);
                    break;
                case CGREQ_VARIABLES:
                    req_variables(cg);
                    break;
                case CGREQ_FUNCTIONS:
                    ARRAY_FOR(fnIdx, objects) {
                        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, fnIdx);
                        if(obj->type == OBJ_FN) {
                            req_function(cg, obj);
                        }
                    }
                    break;
                case CGREQ_STRUCTS:
                    ARRAY_FOR(fnIdx, objects) {
                        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, fnIdx);
                        if(obj->type == OBJ_STRUCT) {
                            req_struct(cg, obj);
                        }
                    }
                    break;
                default:
                    UNREACHABLE();
            }

        }
        cg->endModule(m, cg->cl);
        // Unnecessary, but why not? It can't hurt, and it makes it clearer to me what the meaning is.
        // Anyway, gcc will probably optimize it away because of the assignment at the beginning of the loop.
        cg->data.current.scope = NULL;
    }
    arrayFree(&objects);
    cg->data.prog = NULL; // Always clean after ourselves, even if it is unnecessary.
}
