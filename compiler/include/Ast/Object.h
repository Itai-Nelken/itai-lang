#ifndef AST_OBJ_H
#define AST_OBJ_H

#include <stdio.h> // FILE
#include <stdbool.h>
#include "Token.h"
#include "StringTable.h"
#include "Type.h"

// Ast/StmtNode.h includes this file, so we can't include it.
typedef struct ast_block_statement ASTBlockStmt;

/**
 * An ASTObj represents an object - i.e a variable, function, struct etc. (see ASTObjType).
 * An object stores the name, data type, and any additional information such as parameters or function body.
 **/
typedef enum ast_object_types {
    OBJ_VAR,
    OBJ_FN,
    //OBJ_STRUCT,
    //OBJ_ENUM?
    OBJ_TYPE_COUNT
} ASTObjType;

typedef struct ast_object {
    ASTObjType type;
    Location loc;
    ASTString name;
    Type *dataType;
    union {
        //struct {} var;
        struct {
            Array parameters; // Array<ASTObj *> (OBJ_VAR)
            Type *returnType;
            ASTBlockStmt *body;
        } fn;
        //struct {
        //    Scope *scope;
        //} structure;
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
 * @return The new object // TODO: or NULL on failure (to allocate)?
 **/
ASTObj *astObjectNew(ASTObjType type, Location loc, ASTString name, Type *dataType);

/**
 * Free an ASTObj.
 *
 * @param obj The object to free.
 **/
void astObjectFree(ASTObj *obj);

#endif // AST_OBJ_H
