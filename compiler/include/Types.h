#ifndef TYPES_H
#define TYPES_H

#include <stdio.h> // FILE
#include <stdbool.h>
#include "Token.h" // Location

// Can't include Ast.h ast it includes this file.
typedef char *ASTString; // from Ast.h
typedef usize ModuleID; // from Ast.h

typedef enum type_type {
    TY_VOID,
    TY_I32, TY_U32,
    //TY_PTR,
    TY_FN,
    TY_STRUCT,
    TY_ID, // An identifier type is a place holder for types the parser can't resolve.
    TY_COUNT
} TypeType;

// Update typeEqual() when adding new fields.
typedef struct type {
    TypeType type;
    ASTString name;
    Location decl_location;
    ModuleID decl_module; // The module that contains the type.
    int size;
    //int align;
    union {
        //struct {
        //    struct ast_type *base;
        //} ptr;
        struct {
            struct type *return_type;
            Array parameter_types; // Array<Type *>
        } fn;
        struct {
            Array field_types; // Array<Type *>
        } structure;
    } as;
} Type;

bool typeIsNumeric(Type *ty);
bool typeIsSigned(Type *ty);
bool typeIsUnsigned(Type *ty);
bool typeIsPrimitive(Type *ty);
bool typeIsFunction(Type *ty);

#define IS_NUMERIC(ty) typeIsNumeric((ty))

#define IS_UNSIGNED(ty) typeIsUnsigned((ty))

#define IS_SIGNED(ty) typeIsSigned((ty))

#define IS_PRIMITIVE(ty) typeIsPrimitive((ty))

#define IS_FUNCTION(ty) typeIsFunction((ty))

/***
 * Initialize a Type.
 *
 * @param ty The Type to initialize.
 * @param type The type of the Type.
 * @param name The type's name.
 * @param decl_module The ModuleID of the module containing the type.
 * @param size The size of the type.
 ***/
void typeInit(Type *ty, TypeType type, ASTString name, ModuleID decl_module, int size);

/***
 * Free a Type.
 *
 * @param ty The Type to free.
 ***/
void typeFree(Type *ty);

/***
 * Check if two Types are equal.
 *
 * @param a The first Type.
 * @param b The second Type.
 * @return true if the Types are equal, false if not.
 ***/
bool typeEqual(Type *a, Type *b);

/***
 * Print a Type to stream 'to'.
 *
 * @param to The stream to print to.
 * @param ty The Type to print.
 * @param compact Print a compact version.
 ****/
void typePrint(FILE *to, Type *ty, bool compact);

#endif // TYPES_H
