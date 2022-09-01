#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <limits.h>
#include "common.h"
#include "Table.h"

// can't include Types.h as it includes this file, so the DataType type has to be pre-declared.
typedef struct data_type DataType;

// A SymbolID is a key in the symbols hash table that refers to a symbol.
typedef usize SymbolID;

#define EMPTY_SYMBOL_ID (SIZE_MAX - 1)

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
 * Add a type to a symbol table.
 *
 * @param syms The symbol table to use.
 * @param ty The type.
 * @return The id of the symbol.
 ***/
SymbolID symbolTableAddType(SymbolTable *syms, DataType ty);

/***
 * Get a type from a symbol table.
 *
 * @param syms The symbol table to use.
 * @param id The ID of the type to get.
 * @return A pointer to the type or NULL on failure.
 ***/
DataType *symbolTableGetType(SymbolTable *syms, SymbolID id);

/***
 * Print a symbol table to stream 'to'.
 *
 * @param to The stream to print to.
 * @param syms The SymbolTable to print.
 ***/
void symbolTablePrint(FILE *to, SymbolTable *syms);

#endif // SYMBOLS_H
