#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Symbols.h"

typedef enum symbol_type {
    SYM_IDENTIFIER
} SymbolType;

typedef struct symbol {
    SymbolType type;
    union {
        String identifier;
    } as;
} Symbol;

void symbolTableInit(SymbolTable *syms) {
    arrayInit(&syms->symbols);
}

static void symbol_free_callback(void *sym, void *cl) {
    UNUSED(cl);
    Symbol *s = (Symbol *)sym;
    if(s->type == SYM_IDENTIFIER) {
        stringFree(s->as.identifier);
    }
    FREE(s);
}

void symbolTableFree(SymbolTable *syms) {
    arrayMap(&syms->symbols, symbol_free_callback, NULL);
    arrayFree(&syms->symbols);
}

static SymbolID add_symbol(SymbolTable *syms, Symbol *sym) {
    return arrayPush(&syms->symbols, (void *)sym);
}

static Symbol *get_symbol(SymbolTable *syms, SymbolID id) {
    // arrayGet() handles the case where 'id' is larger than the array.
    return ARRAY_GET_AS(Symbol *, &syms->symbols, id);
}

struct find_symbol_data {
    bool found;
    SymbolID found_id;
    void *data;
};

static void find_identifier_callback(void *sym, void *data) {
    Symbol *s = (Symbol *)sym;
    struct find_symbol_data *d = (struct find_symbol_data *)data;
    if(s->type == SYM_IDENTIFIER && stringEqual(s->as.identifier, (char *)d->data)) {
        d->found = true;
    } else {
        d->found_id++;
    }
}

SymbolID symbolTableAddIdentifier(SymbolTable *syms, char *txt, usize length) {
    // Check if the identifier already exists, if yes return its ID.
    // No need to have multiple copies of each identifier.
    String id = stringNCopy(txt, length);
    struct find_symbol_data data = {
        .found = false,
        .found_id = 0,
        .data = (void *)id
    };
    arrayMap(&syms->symbols, find_identifier_callback, (void *)&data);
    if(data.found) {
        return data.found_id;
    }

    Symbol *sym;
    NEW0(sym);
    sym->type = SYM_IDENTIFIER;
    sym->as.identifier = id;
    return add_symbol(syms, sym);
}

const char *symbolTableGetIdentifier(SymbolTable *syms, SymbolID id) {
    Symbol *sym = get_symbol(syms, id);
    if(!sym || sym->type != SYM_IDENTIFIER) {
        return NULL;
    }
    return (const char *)sym->as.identifier;
}
