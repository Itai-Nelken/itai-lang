#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Table.h"
#include "Ast/Object.h"
#include "Ast/Scope.h"

/* Helper functions */

static void free_scope_callback(void *scope, void *cl) {
    UNUSED(cl);
    scopeFree((Scope *)scope);
}

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astObjectFree((ASTObj *)object);
}


/* Scope functions */

void scopePrint(FILE *to, Scope *sc, bool recursive) {
    if(!sc) {
        fputs("(null)", to);
        return;
    }

    fputs("Scope{\x1b[1mobjects:\x1b[0m [", to);
    ARRAY_FOR(i, sc->objects) {
        astObjectPrint(to, ARRAY_GET_AS(ASTObj *, &sc->objects, i), true);
        if(i + 1 < arrayLength(&sc->objects)) {
            fputs(", ", to);
        }
    }
    String depthStr = stringNew(33); // 33 is the length of SCOPE_DEPTH_MODULE_NAMESPACE
    switch(sc->depth) {
        case SCOPE_DEPTH_MODULE_NAMESPACE: stringAppend(&depthStr, "\x1b[1;33mSCOPE_DEPTH_MODULE_NAMESPACE\x1b[0m"); break;
        case SCOPE_DEPTH_STRUCT: stringAppend(&depthStr, "\x1b[1;33mSCOPE_DEPTH_STRUCT\x1b[0m"); break;
        case SCOPE_DEPTH_BLOCK: stringAppend(&depthStr, "\x1b[1;33mSCOPE_DEPTH_BLOCK\x1b[0m"); break;
        default:
            stringAppend(&depthStr, "\x1b[1;33mSCOPE_DEPTH_MODULE\x1b[0;31m+%u\x1b[0m", sc->depth - SCOPE_DEPTH_BLOCK);
            break;
    }
    fprintf(to, "], \x1b[1mdepth: \x1b[31m%s\x1b[0m, \x1b[1mchildren:\x1b[0m [", depthStr);
    stringFree(depthStr);
    if(recursive) {
        ARRAY_FOR(i, sc->children) {
            scopePrint(to, ARRAY_GET_AS(Scope *, &sc->children, i), true);
            if(i + 1 < arrayLength(&sc->children)) {
                fputs(", ", to);
            }
        }
        fputc(']', to);
        // Note: no need to print the parent of [sc], since its the scope we are printing in the first place.
    } else {
        fprintf(to, "...@%lu]", arrayLength(&sc->children));
    }
    fprintf(to, ", \x1b[1mparent:\x1b[0m %s", sc->parent ? "Scope{...}" : "(null)");
    fputc('}', to);
}

Scope *scopeNew(Scope *parent, ScopeDepth depth) {
    Scope *sc;
    NEW0(sc);
    sc->parent = parent;
    sc->depth = depth;
    arrayInit(&sc->children);
    arrayInit(&sc->objects);
    tableInit(&sc->variables, NULL, NULL);
    tableInit(&sc->functions, NULL, NULL);

    return sc;
}

void scopeFree(Scope *scope) {
    // First, free all children.
    arrayMap(&scope->children, free_scope_callback, NULL);
    arrayFree(&scope->children);

    // Then, free all objects and also the tables.
    arrayMap(&scope->objects, free_object_callback, NULL);
    arrayFree(&scope->objects);
    // Note: the objects themselves are owned by the array
    //       and are freed above.
    tableFree(&scope->variables);
    tableFree(&scope->functions);

    // Finally, free the scope itself.
    FREE(scope);
}

void scopeAddChild(Scope *parent, Scope *child) {
    VERIFY(child != parent);
    VERIFY(child->parent == parent);
    arrayPush(&parent->children, child);
}

bool scopeHasObject(Scope *scope, ASTObj* obj) {
    VERIFY(obj != NULL);
    // Use a result variable for ease of debugging and to make it clear all return paths are handled.
    Table *tbl;
    switch(obj->type) {
        case OBJ_VAR:
            tbl = &scope->variables;
            break;
        case OBJ_FN:
            tbl = &scope->functions;
            break;
        default:
            UNREACHABLE();
    }
    return tableGet(tbl, (void *)obj->name) != NULL;
}

void scopeAddObject(Scope *scope, ASTObj *obj) {
    VERIFY(!scopeHasObject(scope, obj));
    arrayPush(&scope->objects, (void *)obj);
    Table *tbl;
    switch(obj->type) {
        case OBJ_VAR:
            tbl = &scope->variables;
            break;
        case OBJ_FN:
            tbl = &scope->functions;
            break;
        default:
            UNREACHABLE();
    }
    tableSet(tbl, (void *)obj->name, (void *)obj);
}
