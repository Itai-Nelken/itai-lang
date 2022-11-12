#ifndef TYPES_H
#define TYPES_H

#include <stdio.h> // FILE
#include <stdbool.h>

// Can't include Ast.h ast it includes this file.
typedef char *ASTString; // from Ast.h

typedef enum type_type {
    TY_I32, TY_U32,
    //TY_PTR,
    TY_FN,
    TY_COUNT
} TypeType;

// Update typeEqual() when adding new fields.
typedef struct type {
    TypeType type;
    ASTString name;
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
    } as;
} Type;

#define IS_NUMERIC(ty) ({bool __res; switch((ty).type) { \
    case TY_I32: \
    case TY_U32: __res = true; break; \
    default: \
    __res = false; break; \
    } __res;})

#define IS_UNSIGNED(ty) ({Type *__t = &(ty); VERIFY(IS_NUMERIC(*__t)); \
    bool __res; \
    switch(__t->type) { \
    case TY_U32: __res = true; break; \
    default: __res = false; break; \
 } __res;})

#define IS_SIGNED(ty) (!IS_UNSIGNED(ty))

#define IS_PRIMITIVE(ty) ({Type *_t = &(ty); _t->type == TY_I32 || _t->type == TY_U32;})

#define IS_FUNCTION(ty) ({Type *_t = &(ty); _t->type == TY_FN;})

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
