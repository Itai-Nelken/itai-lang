#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "common.h" // u64
#include "Token.h" // Location
#include "Strings.h"
#include "Table.h"

/** Types **/
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
    ND_ASSIGN
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

#define NODE_IS(node, type) ((node)->node_type == (type))
#define AS_NODE(node) ((ASTNode *)(node))
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)(node))
#define AS_LITERAL_NODE(node) ((ASTLiteralValueNode *)(node))
#define AS_OBJ_NODE(node) ((ASTObjNode *)(node))

// A ModuleID is an index into the ASTProgram::modules array.
typedef usize ModuleID;

// An ASTString represents an interned string,
// and it can be used interchangeably with String as long
// as it isn't freed.
typedef char *ASTString;

// An ASTObj holds a object which in this context
// means functions, variables, structs, enums etc.
typedef enum ast_obj_type {
    OBJ_VAR
} ASTObjType;

typedef struct ast_obj {
    ASTObjType type;
    Location location;

    union {
        struct {
            ASTString name;
        } var;
    } as;
} ASTObj;

// An ASTModule holds all of a modules data
// which includes functions, structs, enums, global variables, types etc.
typedef struct ast_module {
    ASTString name;
    Array objects; // Array<ASTObj *>
    Array globals; // Array<ASTNode *> (ND_VARIABLE or ND_ASSIGN)
    // Array types; // or something similar.
} ASTModule;

// An ASTProgram holds all the information
// belonging to a program like functions & types.
typedef struct ast_program {
    // holds a single copy of each string in the entire program.
    Table strings; // Table<char *, String>
    Array modules; // Array<ASTModule *>
} ASTProgram;

/** Functions **/

ASTModule *astNewModule(ASTString name);
void astFreeModule(ASTModule *module);

void astProgramInit(ASTProgram *prog);
void astProgramFree(ASTProgram *prog);
// If 'str' is a 'String', ownership of it is taken.
ASTString astProgramAddString(ASTProgram *prog, char *str);
ModuleID astProgramAddModule(ASTProgram *prog, ASTModule *module);
ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id);

void literalValuePrint(FILE *to, LiteralValue value);

ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *lhs, ASTNode *rhs);
ASTNode *astNewLiteralValueNode(ASTNodeType type, Location loc, LiteralValue value);
// Ownership of 'obj' is NOT taken.
ASTNode *astNewObjNode(ASTNodeType type, Location loc, ASTObj *obj);
void astFreeNode(ASTNode *n);
void astPrintNode(FILE *to, ASTNode *n);

ASTObj *astNewObj(ASTObjType type, Location loc);
void astFreeObj(ASTObj *obj);
void astPrintObj(FILE *to, ASTObj *obj);

#endif // AST_H
