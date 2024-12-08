#ifndef AST_OBJ_H
#define AST_OBJ_H

#include "StringTable.h"
#include "Type.h"

/**
 * An ASTObj represents an object - i.e a variable, function, struct etc. (see ASTObjType).
 * An object stores the name, data type, and any additional information such as parameters or function body.
 **/
typedef enum ast_object_types {
    OBJ_VAR,
    //OBJ_FN,
    //OBJ_STRUCT,
    //OBJ_ENUM?
    OBJ_TYPE_COUNT
} ASTObjType;

typedef struct ast_object {
    ASTObjType type;
    ASTString name;
    Type *dataType;
    //union {
    //    struct {} var;
    //} as;
} ASTObj;


/**
 * Create a new ASTObj.
 *
 * @param type The ASTObjType for the new object.
 * @param name The name for the new object.
 * @param dataType The data type for the new object.
 * @return The new object // TODO: or NULL on failure (to allocate)?
 **/
ASTObj *astObjectNew(ASTObjType type, ASTString name, Type *dataType);

/**
 * Free an ASTObj.
 *
 * @param obj The object to free.
 **/
void astObjectFree(ASTObj *obj);

#endif // AST_OBJ_H
