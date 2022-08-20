#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "common.h"
#include "Table.h"

// A SymbolID is a key in the symbols hash table that refers to a symbol.
typedef usize SymbolID;

typedef struct symbol_table {
    SymbolID next_id; // used for generating new symbolID's.
    Table symbols; // Table<SymbolID, Symbol *> - Symbol is an internal struct.
} SymbolTable;

/***
 * Initialize a symbol table.
 *
 * @param syms The symbol table to initialize.
 ***/
void symbolTableInit(SymbolTable *syms);

/***
 * Free a symbol table.
 *
 * @param syms The symbol table to free.
 ***/
void symbolTableFree(SymbolTable *syms);

/***
 * Add an identifier to a symbol table.
 *
 * @param syms The symbol table to use.
 * @param txt The string containing the identifier (doesn't have to be null terminated).
 * @param length The identifiers length.
 * @return The id of the symbol.
 ***/
SymbolID symbolTableAddIdentifier(SymbolTable *syms, char *txt, usize length);

/***
 * Get an identifier from a symbol table.
 *
 * @param syms The symbol table to use.
 * @param id The ID of the identifier to get.
 * @return The identifier (as a null terminated string) or NULL on failure.
 ***/
const char *symbolTableGetIdentifier(SymbolTable *syms, SymbolID id);

/***
 * Print a symbol table to stream 'to'.
 *
 * @param to The stream to print to.
 * @param syms The SymbolTable to print.
 ***/
void symbolTablePrint(FILE *to, SymbolTable *syms);

#endif // SYMBOLS_H
