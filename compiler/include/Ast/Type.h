#ifndef AST_TYPE_H
#define AST_TYPE_H

/**
 * Type represents a data type (such as i32, u32, char, str etc.) including pointer, function, and struct types.
 * A Type stores:
 *  - The source code location in which it was defined (which will be empty in the case of primitives that are not defined by the compiler).
 *  - The module in which it was defined.
 *  - The string representation of the type (e.g. "i32", "fn(i32, i32)->i32").
 *  - Any information required for the type (e.g. parameter and return types for function types).
 **/

#endif // AST_TYPE_H
