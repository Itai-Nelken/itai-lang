#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include "common.h"
#include "Token.h"
#include "Symbols.h"

typedef enum primitive_data_types {
    TY_VOID, // used internally for functions returning nothing, but it isn't a valid primitive type in the language.
    TY_I32,
    TY_COUNT
} PrimitiveType;

typedef struct data_type {
    SymbolID name;
    usize size;
    bool is_signed;
} DataType;

/***
 * Create a new data type.
 *
 * @param name_id The SymbolID of the name identifier.
 * @param size The size (in bytes) of the type.
 * @param is_signed Is the type signed?
 * @return The new type.
 ***/
DataType dataTypeNew(SymbolID name_id, usize size, bool is_signed);

/***
 * Print a data type.
 *
 * @param to The stream to print to.
 * @param ty The data type to print.
 ***/
void dataTypePrint(FILE *to, DataType ty);

#endif // TYPES_H
