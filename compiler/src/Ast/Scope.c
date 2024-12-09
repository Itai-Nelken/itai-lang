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

Scope *scopeNew(Scope *parent) {
    Scope *sc;
    NEW0(sc); // isBlockScope is set to false (0) here.
    sc->parent = parent;
    arrayInit(&sc->children);
    arrayInit(&sc->objects);
    tableInit(&sc->variables, NULL, NULL);

    return sc;
}

void scopeFree(Scope *scope) {
    // First, free all children.
    arrayMap(&scope->children, free_scope_callback, NULL);
    arrayFree(&scope->children);

    // Then, free all objects and also the tables.
    arrayMap(&scope->objects, free_object_callback, NULL);
    arrayFree(&scope->objects);
    tableFree(&scope->variables); // objects are freed above.

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
    bool result = false;
    switch(obj->type) {
        case OBJ_VAR:
            result = tableGet(&scope->variables, (void *)obj->name) != NULL;
            break;
        default:
            UNREACHABLE();
    }
    return result;
}

void scopeAddObject(Scope *scope, ASTObj *obj) {
    VERIFY(!scopeHasObject(scope, obj));
    switch(obj->type) {
        case OBJ_VAR:
            tableSet(&scope->variables, (void *)obj->name, (void *)obj);
            break;
        default:
            UNREACHABLE();
    }
}
