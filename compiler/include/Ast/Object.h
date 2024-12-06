#ifndef AST_OBJ_H
#define AST_OBJ_H

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

typedef struct ast_object {} ASTObj;

#endif // AST_OBJ_H
