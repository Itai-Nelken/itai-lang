#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "common.h"
#include "memory.h"
#include "Table.h"
#include "Strings.h"
#include "Types.h"
#include "Symbols.h"

/***
 * A note about the implementation:
 * A hashtable is used even though known incices are used because
 * it allows for easier checking if a symbol already exists, as well
 * as providing the option to change the SymbolID type to for example a string in the future.
 ***/

typedef enum symbol_type {
    SYM_IDENTIFIER,
    SYM_TYPE
} SymbolType;

typedef struct symbol {
    SymbolType type;
    union {
        String identifier;
        DataType type;
    } as;
} Symbol;

static unsigned hash_usize(void *key) {
    // As there are no two different SymbolID's that hold
    // the same number, we can simply return the value
    // and be sure that there will be no colisions.
    return (unsigned)(usize)key;
}

static bool compare_usize(void *a, void *b) {
    return (usize)a == (usize)b;
}

void symbolTableInit(SymbolTable *syms) {
    syms->next_id = 0;
    tableInit(&syms->symbols, hash_usize, compare_usize);
}

static void free_symbol(Symbol *sym) {
    if(sym->type == SYM_IDENTIFIER) {
        stringFree(sym->as.identifier);
    }
    FREE(sym);
}

static void free_symbol_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(cl);
    UNUSED(is_last);
    free_symbol((Symbol *)item->value);
}

void symbolTableFree(SymbolTable *syms) {
    tableMap(&syms->symbols, free_symbol_callback, NULL);
    tableFree(&syms->symbols);
    syms->next_id = 0;
}

static inline SymbolID gen_id(SymbolTable *syms) {
    assert(syms->next_id + 1 < EMPTY_SYMBOL_ID);
    return syms->next_id++;
}

static SymbolID add_symbol(SymbolTable *syms, Symbol *sym) {
    SymbolID id = gen_id(syms);
    // symbols can't be readded.
    assert(tableSet(&syms->symbols, (void *)id, (void *)sym) == NULL);
    return id;
}

static Symbol *get_symbol(SymbolTable *syms, SymbolID id) {
    TableItem *item = tableGet(&syms->symbols, (void *)id);
    if(!item) {
        return NULL;
    }
    return (Symbol *)item->value;
}

SymbolID symbolTableAddIdentifier(SymbolTable *syms, char *txt, usize length) {
    Symbol *sym;
    NEW0(sym);
    sym->type = SYM_IDENTIFIER;
    sym->as.identifier = stringNCopy(txt, length);
    return add_symbol(syms, sym);
}

const char *symbolTableGetIdentifier(SymbolTable *syms, SymbolID id) {
    Symbol *sym = get_symbol(syms, id);
    if(!sym || sym->type != SYM_IDENTIFIER) {
        return NULL;
    }
    return (const char *)sym->as.identifier;
}

SymbolID symbolTableAddType(SymbolTable *syms, DataType ty) {
    Symbol *sym;
    NEW0(sym);
    sym->type = SYM_TYPE;
    sym->as.type = ty;
    return add_symbol(syms, sym);
}

DataType *symbolTableGetType(SymbolTable *syms, SymbolID id) {
    Symbol *sym = get_symbol(syms, id);
    if(!sym || sym->type != SYM_TYPE) {
        return NULL;
    }
    return &sym->as.type;
}

static void print_symbol_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    Symbol *sym = (Symbol *)item->value;
    fprintf(to, "Symbol{\x1b[1mid:\x1b[0;34m %zu\x1b[0m, \x1b[1mvalue:\x1b[0m ", (SymbolID)item->key);
    switch(sym->type) {
        case SYM_IDENTIFIER:
            fprintf(to, "'%s'", sym->as.identifier);
            break;
        case SYM_TYPE:
            dataTypePrint(to, sym->as.type);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
    if(!is_last) {
        fputs(", ", to);
    }
}

void symbolTablePrint(FILE *to, SymbolTable *syms) {
    fputs("SymbolTable{\x1b[1msymbols:\x1b[0m [", to);
    tableMap(&syms->symbols, print_symbol_callback, (void *)to);
    fputs("]}", to);
}
