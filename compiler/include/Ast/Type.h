#ifndef AST_TYPE_H
#define AST_TYPE_H

#include <stdio.h> // FILE
#include <stdbool.h>
#include "Array.h"
#include "StringTable.h"
#include "Token.h"

// We can't include Module.h,Program.h since they includes us.
typedef struct ast_module ASTModule; // Ast/Module.h
typedef usize ModuleID; // Ast/Program.h

/**
 * Type represents a data type (such as i32, u32, char, str etc.) including pointer, function, and struct types.
 * A Type stores:
 *  - The source code location in which it was defined (which will be empty in the case of primitives that are not defined by the compiler).
 *  - The module in which it was defined.
 *  - The string representation of the type (e.g. "i32", "fn(i32, i32)->i32").
 *  - Any information required for the type (e.g. parameter and return types for function types).
 **/

typedef enum type_type {
    TY_VOID, TY_I32, TY_U32, TY_STR,
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
    ModuleID declModule;
    union {
        struct {
            struct type *innerType;
        } ptr;
        struct {
            struct type *returnType;
            Array parameterTypes; // Array<Type *>
        } fn;
        //struct {
        //    Array fieldTypes; // Array<Type *>
        //} structure;
    } as;
} Type;


/**
 * Pretty print a Type.
 *
 * @param to The stream to print to.
 * @param obj The type to print.
 * @param compact print in a compact form?
 **/
void typePrint(FILE *to, Type *ty, bool compact);

/**
 * Create a new Type.
 *
 * @param type The type of the Type (e.g. TY_VOID)
 * @param name The name of the type.
 * @param declLocation The location in which the type was declared.
 * @param declModule The ModuleID of the module that owns the type.
 * @return A new Type initialized with all the above info.
 **/
Type *typeNew(TypeType type, ASTString name, Location declLocation, ModuleID declModule);

/**
 * Free a Type.
 *
 * @param ty The type to free.
 **/
void typeFree(Type *ty);

/**
 * Check if two Types are equal.
 *
 * @param a The first type.
 * @param b The second type.
 * @return true if a and b are equal, false if they are not.
 **/
bool typeEqual(Type *a, Type *b);

/**
 * Check if a Type represents a primitive type.
 *
 * @param ty The type to check.
 * @return true if [ty] is a primitive, false if not.
 **/
bool typeIsPrimitive(Type *ty);

#endif // AST_TYPE_H
