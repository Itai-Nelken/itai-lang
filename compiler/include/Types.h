#ifndef TYPES_H
#define TYPES_H

#include <stdio.h> // FILE
#include <stdbool.h>

// Can't include Ast.h ast it includes this file.
typedef char *ASTString; // from Ast.h

typedef enum type_type {
    TY_I32, TY_U32,
    //TY_PTR,
    //TY_FN,
    TY_COUNT
} TypeType;

// Update typeEqual() when adding new fields.
typedef struct type {
    TypeType type;
    ASTString name;
    int size;
    //int align;
    //union {
    //    struct {
    //        struct ast_type *base;
    //    } ptr;
    //    struct {
    //        struct ast_type *return_type;
    //        Array parameter_types; // Array<Type *>
    //    } fn;
    //} as;
} Type;

/***
 * Initialize a Type.
 *
 * @param ty The Type to initialize.
 * @param type The type of the Type.
 * @param name The type's name.
 * @param size The size of the type.
 ***/
void typeInit(Type *ty, TypeType type, ASTString name, int size);

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
 ****/
void typePrint(FILE *to, Type *ty);

#endif // TYPES_H
