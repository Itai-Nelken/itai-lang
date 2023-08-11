#ifndef PARSED_TYPE_H
#define PARSED_TYPE_H

#include <stdio.h> // FILE
#include <stdbool.h>
#include "Types/types_common.h"
#include "Array.h"
#include "Token.h" // Location
#include "Ast/ParsedAst.h" // ASTString

// Update typeEqual() when adding new fields.
typedef struct parsed_type {
    TypeType type;
    ASTString name;
    Location decl_location;
    ModuleID decl_module; // The module that contains the type.
    union {
        struct {
            struct parsed_type *inner_type;
        } ptr;
        struct {
            struct parsed_type *return_type;
            Array parameter_types; // Array<ParsedType *>
        } fn;
        struct {
            Array field_types; // Array<ParsedType *>
        } structure;
    } as;
} ParsedType;

/***
 * Initialize a ParsedType.
 *
 * @param ty The ParsedType to initialize.
 * @param type The type of the ParsedType.
 * @param name The type's name.
 * @param decl_module The ModuleID of the module containing the type.
 ***/
void parsedTypeInit(ParsedType *ty, TypeType type, ASTString name, ModuleID decl_module);

/***
 * Free a ParsedType.
 *
 * @param ty The ParsedType to free.
 ***/
void parsedTypeFree(ParsedType *ty);

/***
 * Check if two ParsedTypes are equal.
 *
 * @param a The first ParsedType.
 * @param b The second ParsedType.
 * @return true if the ParsedTypes are equal, false if not.
 ***/
bool parsedTypeEqual(ParsedType *a, ParsedType *b);

/***
 * Hash a ParsedType.
 *
 * @param ty The ParsedType to hash.
 * @return The hash of the ParsedType.
 ***/
unsigned parsedTypeHash(ParsedType *ty);

/***
 * Print a ParsedType to stream 'to'.
 *
 * @param to The stream to print to.
 * @param ty The ParsedType to print.
 * @param compact Print a compact version.
 ****/
void parsedTypePrint(FILE *to, ParsedType *ty, bool compact);

#endif // PARSED_TYPE_H
