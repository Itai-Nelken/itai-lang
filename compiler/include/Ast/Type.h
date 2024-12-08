#ifndef AST_TYPE_H
#define AST_TYPE_H

#include "Array.h"
#include "StringTable.h"
#include "Token.h"

/**
 * Type represents a data type (such as i32, u32, char, str etc.) including pointer, function, and struct types.
 * A Type stores:
 *  - The source code location in which it was defined (which will be empty in the case of primitives that are not defined by the compiler).
 *  - The module in which it was defined.
 *  - The string representation of the type (e.g. "i32", "fn(i32, i32)->i32").
 *  - Any information required for the type (e.g. parameter and return types for function types).
 **/

typedef enum type_type {
    TY_PRIMITIVE,
    TY_POINTER,
    TY_FUNCTION,
    TY_STRUCT,
    TY_TYPE_COUNT
} TypeType;

// TODO: How to represent incomplete/unresolved types (i.e. TY_ID)
typedef struct type {
    TypeType type;
    ASTString name;
    Location declLocation;
    // TODO: Do I need to store the module in which the type was declared?
    //union {
    //    struct {
    //        struct type *innerType;
    //    } ptr;
    //    struct {
    //        struct type *returnType;
    //        Array parameterTypes; // Array<Type *>
    //    } fn;
    //    struct {
    //        Array fieldTypes; // Array<Type *>
    //    } structure;
    //} as;
} Type;



#endif // AST_TYPE_H
