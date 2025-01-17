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

struct object_print_data {
    FILE *to;
    bool skip_last_comma;
};
static void print_object_callback(TableItem *item, bool is_last, void *cl) {
    struct object_print_data *data = (struct object_print_data *)cl;
    astObjectPrint(data->to, (ASTObj *)item->value, false);
    if(!is_last || !data->skip_last_comma) {
        fputs(", ", data->to);
    }
}

static void collect_objects_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    Array *a = (Array *)cl;
    arrayPush(a, item->value);
}

/* Scope functions */

void scopePrint(FILE *to, Scope *sc, bool recursive) {
    if(!sc) {
        fputs("(null)", to);
        return;
    }

    fputs("Scope{\x1b[1mobjects:\x1b[0m [", to);
    struct object_print_data objPrintData = {
        .to = to,
        // Note: change table to last table printed when adding more tables.
        .skip_last_comma = tableSize(&sc->functions) == 0 // if true, skip last comma.
    };
    tableMap(&sc->variables, print_object_callback, (void *)&objPrintData);
    objPrintData.skip_last_comma = true;
    tableMap(&sc->functions, print_object_callback, (void *)&objPrintData);

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
    tableInit(&sc->variables, NULL, NULL);
    tableInit(&sc->functions, NULL, NULL);

    return sc;
}

void scopeFree(Scope *scope) {
    // First, free all children.
    arrayMap(&scope->children, free_scope_callback, NULL);
    arrayFree(&scope->children);

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
    return scopeGetObject(scope, obj->type, obj->name) != NULL;
}

ASTObj *scopeGetObject(Scope *scope, ASTObjType objType, ASTString name) {
    // Use a result variable for ease of debugging and to make it clear all return paths are handled.
    Table *tbl;
    switch(objType) {
        case OBJ_VAR:
            tbl = &scope->variables;
            break;
        case OBJ_FN:
            tbl = &scope->functions;
            break;
        default:
            UNREACHABLE();
    }
    TableItem *item = tableGet(tbl, (void *)name);
    return item ? (ASTObj *)item->value : NULL;
}

ASTObj *scopeGetAnyObject(Scope *scope, ASTString name) {
    VERIFY(OBJ_VAR == 0); // TODO: make a compile time assert.
    for(ASTObjType type = OBJ_VAR; type < OBJ_TYPE_COUNT; ++type) {
        ASTObj *obj = scopeGetObject(scope, type, name);
        if(obj) {
            return obj;
        }
    }
    return NULL;
}

bool scopeAddObject(Scope *scope, ASTObj *obj) {
    if(scopeHasObject(scope, obj)) {
        return false;
    }
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
    return true;
}

void scopeGetAllObjects(Scope *scope, Array *objects) {
    VERIFY(arrayLength(objects) == 0);
    tableMap(&scope->variables, collect_objects_callback, (void *)objects);
    tableMap(&scope->functions, collect_objects_callback, (void *)objects);
    //tableMap(&scope->structures, collect_objects_callback, (void *)objects);
    //tableMap(&scope->enums, collect_objects_callback, (void *)objects);
}
