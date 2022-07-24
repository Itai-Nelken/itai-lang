#include <stdbool.h>
#include <assert.h>
#include "common.h"
#include "memory.h"
#include "Table.h"
#include "Array.h"
#include "Strings.h"
#include "Symbols.h"

void initSymTable(SymTable *t, SymbolType type, void (*free_data_callback)(void *value, void *cl), void *cl) {
    t->type = type;
    t->free_data_fn = free_data_callback;
    t->cl = cl;
    initTable(&t->identifiers, NULL, NULL);
    initArray(&t->data);
}

static void free_identifier_callback(TableItem *item, void *cl) {
    UNUSED(cl);
    freeString((char *)item->key);
    FREE(item->value);
}

void freeSymTable(SymTable *t) {
    tableMap(&t->identifiers, free_identifier_callback, NULL);
    if(t->free_data_fn) {
        arrayMap(&t->data, t->free_data_fn, t->cl);
    }
    freeTable(&t->identifiers);
    freeArray(&t->data);
}

int addSymbol(SymTable *t, char *name, void *value) {
    assert(tableGet(&t->identifiers, name) == NULL);
    Symbol *sym = CALLOC(1, sizeof(*sym));
    sym->type = t->type;
    sym->id = arrayPush(&t->data, value);
    tableSet(&t->identifiers, stringCopy(name), sym);
    return sym->id;
}

bool symbolExists(SymTable *t, char *name) {
    return tableGet(&t->identifiers, name) != NULL;
}

void *getSymbol(SymTable *t, int id) {
    return arrayGet(&t->data, id);
}

int getIdFromName(SymTable *t, char *name) {
    TableItem *item = tableGet(&t->identifiers, name);
    assert(item);
    Symbol *sym = (Symbol *)item->value;
    return sym->id;
}
