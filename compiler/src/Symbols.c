#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Table.h"
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

static SymbolID gen_id(SymbolTable *syms) {
    return syms->next_id++;
}

static SymbolID add_symbol(SymbolTable *syms, Symbol *sym) {
    SymbolID id = gen_id(syms);
    void *old_sym = tableSet(&syms->symbols, (void *)id, (void *)sym);
    if(old_sym) {
        // The symbol already exists.
        // free the old one as it isn't needed anymore.
        // FIXME: this isn't very efficient memory-wise.
        free_symbol((Symbol *)old_sym);
    }
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

static void print_symbol_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    Symbol *sym = (Symbol *)item->value;
    fprintf(to, "Symbol{\x1b[1mid:\x1b[0;34m %zu\x1b[0m, \x1b[1mvalue:\x1b[0m ", (SymbolID)item->key);
    switch(sym->type) {
        case SYM_IDENTIFIER:
            fprintf(to, "'%s'", sym->as.identifier);
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
