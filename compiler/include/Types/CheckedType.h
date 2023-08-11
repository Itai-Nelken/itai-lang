#ifndef CHECKED_TYPE_H
#define CHECKED_TYPE_H

#include <stdio.h> // FILE
#include <stdbool.h>
#include "Types/types_common.h"
#include "Token.h" // Location
#include "Ast/CheckedAst.h" // ASTString

// Update checkedTypeEqual() when adding new fields.
typedef struct checked_type {
    TypeType type;
    ASTString name;
    Location decl_location;
    ModuleID decl_module; // The module that contains the type.
    union {
        struct {
            struct checked_type *inner_type;
        } ptr;
        ASTCheckedObj *fn_obj, *struct_obj;
    } as;
} CheckedType;


/***
 * Initialize a CheckedType.
 *
 * @param ty The CheckedType to initialize.
 * @param type The type of the CheckedType.
 * @param name The type's name.
 * @param decl_module The ModuleID of the module containing the type.
 ***/
void checkedTypeInit(CheckedType *ty, TypeType type, ASTString name, ModuleID decl_module);

/***
 * Free a CheckedType.
 *
 * @param ty The CheckedType to free.
 ***/
void checkedTypeFree(CheckedType *ty);

/***
 * Check if two CheckedTypes are equal.
 *
 * @param a The first CheckedType.
 * @param b The second CheckedType.
 * @return true if the CheckedTypes are equal, false if not.
 ***/
bool checkedTypeEqual(CheckedType *a, CheckedType *b);

/***
 * Hash a CheckedType.
 *
 * @param ty The CheckedType to hash.
 * @return The hash of the CheckedType.
 ***/
unsigned checkedTypeHash(CheckedType *ty);

/***
 * Print a CheckedType to stream 'to'.
 *
 * @param to The stream to print to.
 * @param ty The CheckedType to print.
 * @param compact Print a compact version.
 ****/
void checkedTypePrint(FILE *to, CheckedType *ty, bool compact);

#endif // CHECKED_TYPE_H
