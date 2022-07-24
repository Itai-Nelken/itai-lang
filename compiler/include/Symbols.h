#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <stdbool.h>
#include <assert.h>
#include "common.h"
#include "memory.h"
#include "Table.h"
#include "Array.h"
#include "Strings.h"

typedef enum symbol_type {
    SYM_LOCAL,
    SYM_GLOBAL
} SymbolType;

typedef struct symbol {
    SymbolType type;
    int id;
} Symbol;

typedef struct symbol_table {
    SymbolType type;
    Table identifiers; // Table<char *, Symbol>
    Array data; // Array<void *>
    void (*free_data_fn)(void *value, void *cl);
    void *cl;
} SymTable;

void initSymTable(SymTable *t, SymbolType type, void (*free_data_callback)(void *value, void *cl), void *cl);

void freeSymTable(SymTable *t);

int addSymbol(SymTable *t, char *name, void *value);

bool symbolExists(SymTable *t, char *name);

void *getSymbol(SymTable *t, int id);

int getIdFromName(SymTable *t, char *name);

#define GET_SYMBOL_AS(type, table, id) ((type)getSymbol(table, id))
#define GET_SYMBOL_FROM_NAME_AS(type, table, name) ((type)getSymbolFromName(table, name))

#endif // SYMBOLS_H
