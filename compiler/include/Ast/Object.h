#ifndef AST_OBJ_H
#define AST_OBJ_H

#include <stdio.h> // FILE
#include <stdbool.h>
#include "Token.h"
#include "StringTable.h"
#include "Type.h" // Note: defines ModuleID b/c can't include Program.h that includes it and us (indirectly).

// Ast/StmtNode.h includes this file, so we can't include it.
typedef struct ast_block_statement ASTBlockStmt;

// Ast/Scope.h includes this file, so we can't include it.
typedef struct scope Scope;

/**
 * An ASTObj represents an object - i.e a variable, function, struct etc. (see ASTObjType).
 *
 * An object stores the name, data type, and any additional information such as parameters or function body.
 *
 * Since the Validator loses scope resolution information when resolving scopes, objects also store the ModuleID
 * of the module that owns them, making it easy and efficient to know to which module each object belongs to.
 *
 * Some objects are related to a different object, such as struct fields+methods to the struct that contains them.
 * To make this easier to keep track of, each object has a "parent" pointer to another object which is usually NULL
 * but when related to another struct, it points to its parent.
 **/
typedef enum ast_object_types {
    OBJ_VAR,
    OBJ_FN,
    OBJ_STRUCT,
    //OBJ_ENUM?
    OBJ_TYPE_COUNT
} ASTObjType;

typedef struct ast_object {
    ASTObjType type;
    Location location;
    ASTString name;
    Type *dataType;
    ModuleID ownerModule;
    struct ast_object *parent;
    union {
        //struct {} var;
        struct {
            Array parameters; // Array<ASTObj *> (OBJ_VAR)
            Type *returnType;
            ASTBlockStmt *body;
        } fn;
        struct {
            Scope *scope;
        } structure;
    } as;
} ASTObj;


/**
 * Pretty print an ASTObj.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 * @param compact print in a compact form?
 **/
void astObjectPrint(FILE *to, ASTObj *obj, bool compact);

/**
 * Create a new ASTObj.
 *
 * @param type The ASTObjType for the new object.
 * @param loc The location of the object
 * @param name The name for the new object.
 * @param dataType The data type for the new object.
 * @param ownerModule The ModuleID of the module that will own this object.
 * @param parent The parent of the object or NULL (e.g. struct for methods,fields.)
 * @return The new object // TODO: or NULL on failure (to allocate)?
 **/
ASTObj *astObjectNew(ASTObjType type, Location loc, ASTString name, Type *dataType, ModuleID moduleOwner, ASTObj *parent);

/**
 * Free an ASTObj.
 *
 * @param obj The object to free.
 **/
void astObjectFree(ASTObj *obj);

#endif // AST_OBJ_H
