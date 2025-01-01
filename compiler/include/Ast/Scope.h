#ifndef AST_SCOPE_H
#define AST_SCOPE_H

#include <stdio.h>
#include <stdbool.h>
#include "Array.h"
#include "Table.h"
#include "Object.h"

/**
 * A Scope instance represents a namespace and possibly also a block scope.
 * It's a namespace because it contains all the variables, functions, and structs in a certain namespace (e.g. a module).
 * Since a block scope is basically a namespace where structs are not allowed, a Scope can also represent a block scope.
 * All objects in a scope are owned by the scope and stored in an Array,
 * however they are sorted by type to Tables for easier and more efficient access.
 * 
 * Each scope has an array of all its children, and a pointer to its parent scope.
 * An example of a possible scope tree is as following:
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
    // TODO: What about scope depth (for block scopes)?
    Array objects; // Array<ASTObj *> (owns all objects)
    // Note: Key is obj.name for all tables.
    Table variables; // Table<char *, ASTObj *> (OBJ_VAR)
    Table functions; // Table<char *, ASTObj *> (OBJ_FN)
    //Table structures; // Table<char *, ASTObj *> (OBJ_STRUCT)

    bool isBlockScope;
    Array children; // Array<Scope *>
    struct scope *parent;
} Scope;


/**
 * Pretty print a Scope.
 * WARNING: If the scope is recursive (which should never happen) and [recursive] is true, this function will cause a crash.
 *          For example, if sc.parent == sc, this function will recursively print [sc] until the stack overflows and the program crashes.
 *
 * @param to The stream to print to.
 * @param sc The scope to print.
 * @param recursive print all child scopes and their contents as well?
 **/
void scopePrint(FILE *to, Scope *sc, bool recursive);

/**
 * Create (allocate) a new scope.
 *
 * @param parent The parent scope.
 * @return A new initialized scope (heap allocated).
 **/
Scope *scopeNew(Scope *parent);

/**
 * Free a scope and all it's children (WARNING: Use only on the root scope!).
 *
 * @param scope The root of the scope tree to free.
 **/
void scopeFree(Scope *scope);

/**
 * Add a child scope to a scope.
 *
 * @param parent The parent scope.
 * @param child The child scope (C.R.E for child.parent != parent, parent == child).
 **/
void scopeAddChild(Scope *parent, Scope *child);

/**
 * Check if a scope contains an object.
 *
 * @param scope The scope to check in.
 * @param obj The object to check (C.R.E for obj == NULL).
 * @return true if [obj] exists in [scope] or false otherwise.
 **/
bool scopeHasObject(Scope *scope, ASTObj* obj);

/**
 * Add an ASTObj to a scope.
 * NOTE: Ownership of [obj] is taken!
 *
 * @param scope The scope to add the object to.
 * @param obj The ASTObj to add (C.R.E for [obj] to already exist in scope).
 **/
void scopeAddObject(Scope *scope, ASTObj *obj);

#endif // AST_SCOPE_H
