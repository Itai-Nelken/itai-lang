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
 *                                               (root module scope|SCOPE_DEPTH_NAMESPACE)
 *                                                /                                 \
 *                          (fn main block scope|SCOPE_DEPTH_BLOCK)      (fn add block scope|SCOPE_DEPTH_BLOCK)
 *                           /                                \
 *       (if stmt block scope|SCOPE_DEPTH_BLOCK+1)            (else stmt block scope|SCOPE_DEPTH_BLOCK+1)
 **/

// Note: block scopes after the function/method scope are represented by SCOPE_DEPTH_BLOCK + N
//       N being the depth. For example, in the scope tree example in the scope explanation
//       the depth of (if stmt block scope) would be SCOPE_DEPTH_BLOCK + 1
enum scope_depth {
    SCOPE_DEPTH_MODULE_NAMESPACE = -1, // represents the module scope
    SCOPE_DEPTH_STRUCT           =  0, // represents the struct scope
    SCOPE_DEPTH_BLOCK            =  1, // represents function and method scope
};
typedef i16 ScopeDepth;

typedef struct scope {
    // Note: objects are owned by the module.
    // Note: Key is obj.name for all tables.
    Table variables; // Table<char *, ASTObj *> (OBJ_VAR)
    Table functions; // Table<char *, ASTObj *> (OBJ_FN)
    //Table structures; // Table<char *, ASTObj *> (OBJ_STRUCT)

    ScopeDepth depth;
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
Scope *scopeNew(Scope *parent, ScopeDepth depth);

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
 * Get an object by name and type (e.g. an OBJ_VAR with name "myVar").
 *
 * @param scope The scope to get the object from.
 * @param objType The type of the object to get.
 * @param name The name of the object to get.
 * @return The object or NULL if it doesn't exist.
 **/
ASTObj *scopeGetObject(Scope *scope, ASTObjType objType, ASTString name);

/**
 * Get an object by name only (meaning it can be of any type (variable, function etc.))
 *
 * @param scope The scope to get the object from.
 * @param name The name of the object to get.
 * @return The object or NULL if it doesn't exist.
 **/
ASTObj *scopeGetAnyObject(Scope *scope, ASTString name);

/**
 * Add an ASTObj to a scope.
 *
 * @param scope The scope to add the object to.
 * @param obj The ASTObj to add.
 * @return true if the object was added, false if it already exists.
 **/
bool scopeAddObject(Scope *scope, ASTObj *obj);

/**
 * Collect all objects in this scope and push them to [objects].
 *
 * @param scope The scope to collect all objects from.
 * @param objects an Array<ASTObj> to put all the objects in. C.R.E for arrayLength(objects) > 0.
 **/
void scopeGetAllObjects(Scope *scope, Array *objects);

#endif // AST_SCOPE_H
