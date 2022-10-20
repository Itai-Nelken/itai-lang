#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "common.h" // u64
#include "Token.h" // Location
#include "Strings.h"
#include "Table.h"
#include "Types.h"

/** Types **/
// An ASTString represents an interned string.
// an ASTString can be used interchangeably with String as long
// as it isn't freed.
typedef char *ASTString;

// forward declarations
typedef struct ast_obj ASTObj;

// A LiteralValue holds, well, a literal value.
// For example number literals & strings.
typedef enum literal_value_type {
    LIT_NUMBER
} LiteralValueType;

typedef struct literal_value {
    LiteralValueType type;
    union {
        u64 number; // Prefix operators are handled by the parser, so 'number' can be unsigned.
    } as;
} LiteralValue;

#define LITERAL_VALUE(literal_type, value_name, value) ((LiteralValue){.type = (literal_type), .as.value_name = (value)})

// An ASTNode holds an expression.
// for example, 1 + 2 is represented as:
// binary(op(+), literal(1), literal(2)).
typedef enum ast_node_type {
    // LiteralNodes
    ND_NUMBER_LITERAL, // ASTLiteralNode

    // ObjNodes
    ND_VARIABLE, // ASTObjNode

    // Binary nodes
    ND_ASSIGN,

    // other
    // An identifier will usually be replaced with an object.
    // It exists because the parser doesn't always have all the information
    // needed to build an object, so the identifier can be used instead
    // and later the validator/typechecker replaces it with an object.
    ND_IDENTIFIER,
    ND_TYPE_COUNT
} ASTNodeType;

typedef struct ast_node {
    ASTNodeType node_type;
    Location location;
} ASTNode;

typedef struct ast_binary_node {
    ASTNode header;
    ASTNode *lhs, *rhs;
} ASTBinaryNode;

typedef struct ast_expression_node {
    ASTNode header;
    LiteralValue value;
} ASTLiteralValueNode;

typedef struct ast_obj_node {
    ASTNode header;
    ASTObj *obj;
} ASTObjNode;

typedef struct ast_identifier_node {
    ASTNode header;
    ASTString identifier;
} ASTIdentifierNode;

#define NODE_IS(node, type) ((node)->node_type == (type))
#define AS_NODE(node) ((ASTNode *)(node))
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)(node))
#define AS_LITERAL_NODE(node) ((ASTLiteralValueNode *)(node))
#define AS_OBJ_NODE(node) ((ASTObjNode *)(node))
#define AS_IDENTIFIER_NODE(node) ((ASTIdentifierNode *)(node))

// A ModuleID is an index into the ASTProgram::modules array.
typedef usize ModuleID;

// An ASTObj holds a object which in this context
// means functions, variables, structs, enums etc.
typedef enum ast_obj_type {
    OBJ_VAR,
    OBJ_TYPE_COUNT
} ASTObjType;

typedef struct ast_obj {
    ASTObjType type;
    Location location;

    union {
        struct {
            ASTString name;
            Type *type;
        } var;
    } as;
} ASTObj;

// An ASTModule holds all of a modules data
// which includes functions, structs, enums, global variables, types etc.
typedef struct ast_module {
    ASTString name;
    Array objects; // Array<ASTObj *>
    Array globals; // Array<ASTNode *> (ND_VARIABLE or ND_ASSIGN)
    Table types; // Table<Type *, void>
} ASTModule;

// An ASTProgram holds all the information
// belonging to a program like functions & types.
typedef struct ast_program {
    struct {
        // Primitive types (belong to the root module).
        Type *int32;
        Type *uint32;
    } primitives;
    // Holds a single copy of each string in the entire program.
    Table strings; // Table<char *, String>
    // Holds a single copy of each type in the entire program.
    Array modules; // Array<ASTModule *>
} ASTProgram;

/** Functions **/

ASTModule *astModuleNew(ASTString name);
void astModuleFree(ASTModule *module);
// 'ty' MUST be heap allocated. ownership of it is taken.
Type *astModuleAddType(ASTModule *module, Type *ty);
void astModulePrint(FILE *to, ASTModule *module);

void astProgramInit(ASTProgram *prog);
void astProgramFree(ASTProgram *prog);
void astProgramPrint(FILE *to, ASTProgram *prog);
// If 'str' is a 'String', ownership of it is taken.
ASTString astProgramAddString(ASTProgram *prog, char *str);
ModuleID astProgramAddModule(ASTProgram *prog, ASTModule *module);
ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id);

void literalValuePrint(FILE *to, LiteralValue value);

ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *lhs, ASTNode *rhs);
ASTNode *astNewLiteralValueNode(ASTNodeType type, Location loc, LiteralValue value);
// Ownership of 'obj' is NOT taken.
ASTNode *astNewObjNode(ASTNodeType type, Location loc, ASTObj *obj);
ASTNode *astNewIdentifierNode(Location loc, ASTString str);

void astNodeFree(ASTNode *n);
void astNodePrint(FILE *to, ASTNode *n);

ASTObj *astNewObj(ASTObjType type, Location loc);
void astFreeObj(ASTObj *obj);
void astPrintObj(FILE *to, ASTObj *obj);

#endif // AST_H
