#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Types.h"
#include "Symbols.h"


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

void symbolTableInit(SymbolTable *syms) {
    arrayInit(&syms->symbols);
}

static void free_symbol(Symbol *sym) {
    if(sym->type == SYM_IDENTIFIER) {
        stringFree(sym->as.identifier);
    }
    FREE(sym);
}

static void free_symbol_callback(void *sym, void *cl) {
    UNUSED(cl);
    free_symbol((Symbol *)sym);
}

void symbolTableFree(SymbolTable *syms) {
    arrayMap(&syms->symbols, free_symbol_callback, NULL);
    arrayFree(&syms->symbols);
}

static SymbolID add_symbol(SymbolTable *syms, Symbol *sym) {
    return (SymbolID)arrayPush(&syms->symbols, (void *)sym);
}

static Symbol *get_symbol(SymbolTable *syms, SymbolID id) {
    return arrayGet(&syms->symbols, id);
}

static SymbolID find_identifier(SymbolTable *syms, String txt) {
    for(usize i = 0; i < syms->symbols.used; ++i) {
        Symbol *sym = ARRAY_GET_AS(Symbol *, &syms->symbols, i);
        if(sym->type != SYM_IDENTIFIER) {
            continue;
        } else if(stringEqual(sym->as.identifier, txt)) {
            return (SymbolID)i;
        }
    }
    return EMPTY_SYMBOL_ID;
}

SymbolID symbolTableAddIdentifier(SymbolTable *syms, char *txt, usize length) {
    String identifier = stringNCopy(txt, length);
    SymbolID existing_sym = find_identifier(syms, identifier);
    if(existing_sym != EMPTY_SYMBOL_ID) {
        return existing_sym;
    }
    Symbol *sym;
    NEW0(sym);
    sym->type = SYM_IDENTIFIER;
    sym->as.identifier = identifier;
    return add_symbol(syms, sym);
}

String symbolTableGetIdentifier(SymbolTable *syms, SymbolID id) {
    Symbol *sym = get_symbol(syms, id);
    if(!sym || sym->type != SYM_IDENTIFIER) {
        return NULL;
    }
    return sym->as.identifier;
}

static SymbolID find_type(SymbolTable *syms, DataType ty) {
    for(usize i = 0; i < syms->symbols.used; ++i) {
        Symbol *sym = ARRAY_GET_AS(Symbol *, &syms->symbols, i);
        if(sym->type != SYM_TYPE) {
            continue;
        } else if(sym->as.type.name == ty.name) {
            // FIXME: handle re-declaration of types.
            return (SymbolID)i;
        }
    }
    return EMPTY_SYMBOL_ID;
}

SymbolID symbolTableAddType(SymbolTable *syms, DataType ty) {
    SymbolID existing_sym = find_type(syms, ty);
    if(existing_sym != EMPTY_SYMBOL_ID) {
        return existing_sym;
    }
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

void symbolTablePrint(FILE *to, SymbolTable *syms) {
    fputs("SymbolTable{\x1b[1msymbols:\x1b[0m [", to);
    for(usize i = 0; i < syms->symbols.used; ++i) {
        Symbol *sym = ARRAY_GET_AS(Symbol *, &syms->symbols, i);
        fprintf(to, "Symbol{\x1b[1mid:\x1b[0;34m %zu\x1b[0m, \x1b[1mvalue:\x1b[0m ", (SymbolID)i);
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
        if(i + 1 < syms->symbols.used) {
            fputs(", ", to);
        }
    }
    fputs("]}", to);
}

void symbolIDPrint(FILE *to, SymbolID id) {
    fputs("SymbolID{", to);
    if(id == EMPTY_SYMBOL_ID) {
        fputs("(empty)", to);
    } else {
        fprintf(to, "\x1b[34m%zu\x1b[0m", id);
    }
    fputc('}', to);
}
