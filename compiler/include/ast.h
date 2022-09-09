#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdbool.h>
#include "Token.h"
#include "Strings.h"
#include "Array.h"
#include "Symbols.h"
#include "Types.h"

typedef struct ast_identifier {
    Location location;
    SymbolID id;
} ASTIdentifier;

/***
 * Initialize an ast identifier.
 *
 * @param loc The location of the identifier.
 * @param id The SymbolID of the identifier.
 * @return An initialized heap-allocated identifier.
 ***/
ASTIdentifier *astNewIdentifier(Location loc, SymbolID id);

/***
 * Free an ast identifier.
 *
 * @param id The identifier to free.
 ***/
void astFreeIdentifier(ASTIdentifier *id);

/***
 * Print an ast identifier.
 *
 * @param to The stream to print to.
 * @param id The identifier to print.
 ***/
void astPrintIdentifier(FILE *to, ASTIdentifier *id);

// Update astPrint(), astFree(), node_type_name(), node_name() in ast.c
// and token_to_node_type() in Parser.c when adding new types.
typedef enum ast_node_type {
    // expressions
    ND_ADD, ND_SUB,
    ND_MUL, ND_DIV,
    ND_NEG,
    ND_EQ, ND_NE,
    ND_CALL,
    ND_ASSIGN,

    // literals, identifiers
    ND_NUMBER,
    ND_IDENTIFIER,

    // statements
    ND_EXPR_STMT,
    ND_IF, ND_BLOCK,
    ND_LOOP,
    ND_RETURN
} ASTNodeType;

typedef struct ast_node {
    ASTNodeType type;
    Location location;
} ASTNode;

typedef struct ast_identifier_node {
    ASTNode header;
    SymbolID data_type;
    ASTIdentifier *id;
} ASTIdentifierNode;

typedef struct ast_number_node {
    ASTNode header;
    NumberConstant value;
} ASTNumberNode;

typedef struct ast_unary_node {
    ASTNode header;
    ASTNode *operand;
} ASTUnaryNode;

typedef struct ast_binary_node {
    ASTNode header;
    ASTNode *left, *right;
} ASTBinaryNode;

typedef struct ast_list_node {
    ASTNode header;
    Array body; // Array<ASTNode *>
} ASTListNode;

typedef struct ast_conditional_node {
    ASTNode header;
    ASTNode *condition;
    ASTListNode *body;
    ASTNode *else_;
} ASTConditionalNode;

typedef struct ast_loop_node {
    ASTNode header;
    ASTNode *initializer;
    ASTNode *condition;
    ASTNode *increment;
    ASTListNode *body;
} ASTLoopNode;

#define AS_NODE(node) ((ASTNode *)(node))
#define AS_IDENTIFIER_NODE(node) ((ASTIdentifierNode *)(node))
#define AS_NUMBER_NODE(node) ((ASTNumberNode *)(node))
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)(node))
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)(node))
#define AS_LIST_NODE(node) ((ASTListNode *)(node))
#define AS_CONDITIONAL_NODE(node) ((ASTConditionalNode *)(node))
#define AS_LOOP_NODE(node) ((ASTLoopNode *)(node))

// update astFreeObj(), astPrintObj() & obj_name() when adding new types.
typedef enum ast_obj_type {
    OBJ_FUNCTION,
    OBJ_GLOBAL, OBJ_LOCAL, //OBJ_GENERIC_PARAMETER,
    //OBJ_STRUCT, OBJ_ENUM
} ASTObjType;

typedef struct ast_obj {
    ASTObjType type;
    Location location;
    ASTIdentifier *name;
    SymbolID data_type; // fn type, var type.
    //ScopeID scope;
    //bool is_public;
} ASTObj;

//typedef struct ast_record_obj {
//    ASTObj header;
//    Array fields; // Array<ASTObj *>
//    Array functions; // Array<ASTFunctionObj *>
//} ASTRecordObj;

typedef struct ast_function_obj {
    ASTObj header;
    SymbolID return_type;
    //Array parameters; // Array<ASTObj *> (OBJ_PARAMETER)
    //Array generic_parameters; // Array<ASTObj *> (OBJ_TYPEDEF)
    Array locals; // Array<ASTVariableNode *> (OBJ_LOCAL)
    ASTListNode *body;
} ASTFunctionObj;

typedef struct ast_variable_obj {
    ASTObj header;
    ASTNode *initializer;
    //bool is_const;
} ASTVariableObj;

typedef struct ast_module {
    ASTIdentifier *name;
    Array objects; // Array<ASTObj *> // functions, globals, records.
} ASTModule;

typedef usize ModuleID; // An index into the ASTProgram::modules array.

typedef enum primitive_data_types {
    TY_VOID, // used internally for functions returning nothing, but it isn't a valid primitive type in the language.
    TY_I32,
    TY_COUNT
} PrimitiveType;

typedef struct ast_program {
    SymbolID primitive_ids[TY_COUNT];
    SymbolTable symbols;
    ASTFunctionObj *entry_point;
    ModuleID root_module;
    Array modules; // Array<ASTModule *>
} ASTProgram;

#define AS_OBJ(obj) ((ASTObj *)(obj))
//#define AS_RECORD_OBJ(obj) ((ASTRecordObj *)(obj))
#define AS_FUNCTION_OBJ(obj) ((ASTFunctionObj *)(obj))
#define AS_VARIABLE_OBJ(obj) ((ASTVariableObj *)(obj))

/***
 * Create a new AST function object.
 * NOTE: ownership of 'name' and 'body' is taken.
 *
 * @param loc The Location of the function.
 * @param name The functions name.
 * @param return_type_id The SymbolID of the return type.
 * @param body The body of the function.
 * @return A new function object.
 ***/
ASTObj *astNewFunctionObj(Location loc, ASTIdentifier *name, SymbolID return_type_id, ASTListNode *body);

/***
 * Create a new AST variable object.
 * NOTE: ownership of 'name' and 'initializer' is taken.
 *
 * @param type The type (OBJ_GLOBAL/OBJ_LOCAL).
 * @param loc The location of the variable.
 * @param name The variables name.
 * @param data_type The SymbolID of the variables type.
 * @param initializer The initializer (optional (can be NULL)).
 * @return A new variable object.
 ***/
ASTObj *astNewVariableObj(ASTObjType type, Location loc, ASTIdentifier *name, SymbolID data_type, ASTNode *initializer);

/***
 * Free an AST object.
 *
 * @param obj The object to free.
 ***/
void astFreeObj(ASTObj *obj);

/***
 * Print an AST object.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 ***/
void astPrintObj(FILE *to, ASTObj *obj);

/***
 * Create a new ast module.
 * NOTE: ownership of 'name' is taken.
 *
 * @param name The modules name.
 ***/
ASTModule *astNewModule(ASTIdentifier *name);

/***
 * Free an ast module.
 *
 * @param m The module to free.
 ***/
void astFreeModule(ASTModule *m);

/***
 * Print an ast module.
 *
 * @param to The stream to print to.
 * @param m The module to print.
 ***/
void astPrintModule(FILE *to, ASTModule *m);

/***
 * Initialize an AST program.
 * NOTE: ownership of 'functions' is taken.
 *
 * @param prog The program to initialize.
 ***/
void astInitProgram(ASTProgram *prog);

/***
 * Free an AST program.
 *
 * @param prog The program to free.
 ***/
void astFreeProgram(ASTProgram *prog);

/***
 * Return the SymbolID of a primitive type.
 *
 * @param prog A initialized program.
 * @param ty The primitive type.
 * @return The SymbolID of the primitive type.
 ***/
SymbolID astProgramGetPrimitiveType(ASTProgram *prog, PrimitiveType ty);

/***
 * Add a module to an ast program.
 * NOTE: ownership of 'module' is taken.
 *
 * @param prog The program to add the module to.
 * @param module The module to add.
 * @return The ModuleID of the module.
 ***/
ModuleID astProgramAddModule(ASTProgram *prog, ASTModule *module);

/***
 * Get a module from an ast program.
 * NOTE: do NOT free the returned module, it's owned by the ASTProgram.
 *
 * @param prog The program containing the module,
 * @param module_id The ModuleID of the module to get.
 * @return A pointer to the module or NULL on failure.
 ***/
ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID module_id);

/***
 * Print an AST program.
 *
 * @param to The stream to print to.
 * @param prog The program to print.
 ***/
void astPrintProgram(FILE *to, ASTProgram *prog);

/***
 * Create a new AST identifier node.
 *
 * @param data_type The data type of the identifier (for varibles, the type of the variable. for functions, the return type etc.)
 * @param id The identifier.
 * @return The new AST node.
 ***/
ASTNode *astNewIdentifierNode(SymbolID data_type, ASTIdentifier *id);

/***
 * Create a new AST number node.
 *
 * @param loc The Location of the number.
 * @param value The value of the number.
 * @return The new AST node.
 ***/
ASTNode *astNewNumberNode(Location loc, NumberConstant value);

/***
 * Create a new AST unary node.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param operand The operand.
 * @return The new AST node.
 ***/
ASTNode *astNewUnaryNode(ASTNodeType type, Location loc, ASTNode *operand);

/***
 * Create a new AST binary node.
 *
 * @param type The type of the node.
 * @param loc A Location.
 * @param left The left side.
 * @param right The right side.
 * @return The new AST node.
 ***/
ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right);

/***
 * Create a new AST list node.
 * NOTE: ownership of 'body' is taken.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param body An initialized Array containing the list of nodes.
 * @return The new AST node.
 ***/
ASTNode *astNewListNode(ASTNodeType type, Location loc, Array body);

/***
 * Create a new AST conditional node.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param condition The condition.
 * @param body The body (to be executed if the condition is true).
 * @param else The else clause (to be executed if the condition is false).
 * @return The new AST node.
 ***/
ASTNode *astNewConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTListNode *body, ASTNode *else_);

/***
 * Create a new AST loop node.
 *
 * @param type The type of the node.
 * @param loc A Location
 * @param initializer The initializer.
 * @param condition The condition.
 * @param increment The increment clause (to be executed after every iteration).
 * @param body The body of the loop.
 * @return The new AST node.
 ***/
ASTNode *astNewLoopNode(ASTNodeType type, Location loc, ASTNode *initializer, ASTNode *condition, ASTNode *increment, ASTListNode *body);

/***
 * Print an AST.
 *
 * @param to The stream to print to.
 * @param node The root of the AST to print.
 ***/
void astPrint(FILE *to, ASTNode *node);

/***
 * Free an AST.
 *
 * @param node The root of the AST to free.
 ***/
void astFree(ASTNode *node);

#endif // AST_H
