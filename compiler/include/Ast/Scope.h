#ifndef AST_SCOPE_H
#define AST_SCOPE_H

#include <stdbool.h>
#include "Array.h"
#include "Table.h"

/**
 * A Scope instance represents a namespace and possibly also a block scope.
 * It's a namespace because it contains all the variables, functions, and structs in a certain namespace (e.g. a module).
 * Since a block scope is basically a namespace where structs are not allowed, a Scope can also represent a block scope.
 * All objects in a scope are owned by the scope and stored in an Array,
 * however they are sorted by type to Tables for easier and more efficient access.
 * 
 * Each scope has an array of all its children. An example of a possible scope tree is as following:
 * Example code:
 * =================================
 * fn main() {
 *     if(Strings::compare(Args::args[0], "ten")) {
 *         return add(5 + 5);
 *     } else {
 *         return add(1 + 2);
 *     }
 * }
 * 
 * fn add(a: int, b: int) {
 *     return a + b;
 * }
 * =================================
 * Scope tree for example code:
 *                                               (root module scope)
 *                                                /               \
 *                            (fn main block scope)                (fn add block scope)
 *                             /            \
 *         (if stmt block scope)            (else stmt block scope)
 **/
typedef struct scope {
    Array objects; // Array<ASTObj *> (owns all objects)
    Table variables; // Table<char *, ASTObj *> (OBJ_VAR)
    //Table functions; // Table<char *, ASTObj *> (OBJ_FN)
    //Table structures; // Table<char *, ASTObj *> (OBJ_STRUCT)
    
    bool is_block_scope;
    Array children; // Array<Scope *>
} Scope;

#endif // AST_SCOPE_H
