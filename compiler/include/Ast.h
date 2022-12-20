#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "common.h" // u64
#include "Token.h" // Location
#include "Strings.h"
#include "Table.h"
#include "Types.h"

/** Types **/

// forward declarations
typedef struct ast_obj ASTObj;


/* ASTString */

// An ASTString represents an interned string.
// an ASTString can be used interchangeably with String as long
// as it isn't freed.
typedef char *ASTString;


/* LiteralValue */

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


/* BlockScope */

typedef struct block_scope {
    u32 depth;
    Table visible_locals; // Table<ASTString, ASTObj *>
    // Table type_aliases;???
    struct block_scope *parent;
    Array children; // Array<BlockScope *>
} BlockScope;

typedef struct scope_id {
    u32 depth;
    usize index;
} ScopeID;


/* ControlFlow */

typedef enum control_flow {
    CF_NONE,
    CF_NEVER_RETURNS,
    CF_MAY_RETURN,
    CF_ALWAYS_RETURNS
} ControlFlow;


/* ASTNode */

// An ASTNode holds an expression.
// for example, 1 + 2 is represented as:
// binary(op(+), literal(1), literal(2)).
typedef enum ast_node_type {
    // Literal nodes
    ND_NUMBER_LITERAL,

    // Obj nodes
    ND_VAR_DECL,
    ND_VARIABLE,
    ND_FUNCTION,

    // Binary nodes
    ND_ASSIGN,
    ND_CALL,
    ND_PROPERTY_ACCESS,
    ND_ADD,
    ND_SUBTRACT,
    ND_MULTIPLY,
    ND_DIVIDE,
    ND_EQ, ND_NE,
    ND_LT, ND_LE,
    ND_GT, ND_GE,

    // Conditional nodes
    ND_IF,

    // Loop nodes
    ND_WHILE_LOOP,

    // Unary nodes
    ND_NEGATE,
    ND_RETURN,

    // list nodes
    ND_BLOCK,
    ND_ARGS,

    // other nodes

    // An identifier will usually be replaced with an object.
    // It exists because the parser doesn't always have all the information
    // needed to build an object, so the identifier can be used instead
    // and later the validator/typechecker replaces it with an object if needed.
    ND_IDENTIFIER,
    ND_TYPE_COUNT
} ASTNodeType;

typedef struct ast_node {
    ASTNodeType node_type;
    Location location;
} ASTNode;

typedef struct ast_unary_node {
    ASTNode header;
    ASTNode *operand;
} ASTUnaryNode;

typedef struct ast_binary_node {
    ASTNode header;
    ASTNode *lhs, *rhs;
} ASTBinaryNode;

typedef struct ast_conditional_node {
    ASTNode header;
    ASTNode *condition, *body, *else_;
} ASTConditionalNode;

typedef struct ast_loop_node {
    ASTNode header;
    ASTNode *initializer, *condition, *increment, *body;
} ASTLoopNode;

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

typedef struct ast_list_node {
    ASTNode header;
    ScopeID scope;
    ControlFlow control_flow;
    Array nodes; // Array<ASTNode *>
} ASTListNode;

#define NODE_IS(node, type) ((node)->node_type == (type))
#define AS_NODE(node) ((ASTNode *)(node))
#define AS_UNARY_NODE(node) ((ASTUnaryNode *)(node))
#define AS_BINARY_NODE(node) ((ASTBinaryNode *)(node))
#define AS_CONDITIONAL_NODE(node) ((ASTConditionalNode *)(node))
#define AS_LOOP_NODE(node) ((ASTLoopNode *)(node))
#define AS_LITERAL_NODE(node) ((ASTLiteralValueNode *)(node))
#define AS_OBJ_NODE(node) ((ASTObjNode *)(node))
#define AS_IDENTIFIER_NODE(node) ((ASTIdentifierNode *)(node))
#define AS_LIST_NODE(node) ((ASTListNode *)(node))


/* ASTObj */

// An ASTObj holds a object which in this context
// means functions, variables, structs, enums etc.
typedef enum ast_obj_type {
    OBJ_VAR, OBJ_FN,
    OBJ_STRUCT,
    OBJ_TYPE_COUNT
} ASTObjType;

typedef struct ast_obj {
    ASTObjType type;
    Location location;
    ASTString name;
    Type *data_type;

    union {
        //struct {
        //  bool is_const;
        //} var;
        struct {
            Array parameters; // Array<ASTObj *> (OBJ_VAR)
            Type *return_type;
            BlockScope *scopes;
            Array locals; // Array<ASTObj *>
            ASTListNode *body;
        } fn;
        struct {
            Array fields; // Array<ASTObj *> (OBJ_VAR)
        } structure;
    } as;
} ASTObj;

/* ASTModule */

// A ModuleID is an index into the ASTProgram::modules array.
typedef usize ModuleID;

// An ASTModule holds all of a modules data
// which includes functions, structs, enums, global variables, types etc.
typedef struct ast_module {
    ASTString name;
    Array objects; // Array<ASTObj *>
    Array globals; // Array<ASTNode *> (ND_VARIABLE or ND_ASSIGN)
    Table types; // Table<ASTString, Type *>
} ASTModule;

/* ASTProgram */

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
    // Holds all modules in a program.
    Array modules; // Array<ASTModule *>
} ASTProgram;


/** Functions **/

/* LiteralValue */

/***
 * Create a new LiteralValue.
 *
 * @param literal_type (type: LiteralValueType) The type of the value.
 * @param value_name (type: C identifier) The name of the value's field in the 'as' union.
 * @param value (type: any) The value itself (must be a constexpr).
 ***/
#define LITERAL_VALUE(literal_type, value_name, value) ((LiteralValue){.type = (literal_type), .as.value_name = (value)})

/***
 * Print a LiteralValue.
 *
 * @param to The stream to print to.
 * @param value The value to print.
 ***/
void literalValuePrint(FILE *to, LiteralValue value);


/* BlockScope */

/***
 * Create a new BlockScope.
 *
 * @param parent_scope The previous scope.
 * @param depth The depth of the scope.
 * @return The new scope.
 ***/
BlockScope *blockScopeNew(BlockScope *parent_scope, u32 depth);

/***
 * Add a child to a BlockScope.
 *
 * @param parent The parent scope.
 * @param child The child scope to add.
 * @return The ScopeID of the added scope.
 ***/
ScopeID blockScopeAddChild(BlockScope *parent, BlockScope *child);

/***
 * Get a child BlockScope.
 *
 * @param parent The parent scope.
 * @param child_id The ScopeID of the child.
 * @return The child scope (always, panics on invalid scopeID);
 ***/
BlockScope *blockScopeGetChild(BlockScope *parent, ScopeID child_id);

/***
 * Free a list of BlockScopes's.
 *
 * @param scope_list The head of the scope list.
 ***/
void blockScopeFree(BlockScope *scope_list);

/***
 * Print a ScopeID.
 *
 * @param to The stream to print to.
 * @param scope_id The ScopeID to print.
 * @param compact Print a compact version.
 ***/
void scopeIDPrint(FILE *to, ScopeID scope_id, bool compact);


/* ControlFlow */

ControlFlow controlFlowUpdate(ControlFlow old, ControlFlow new);


/* ASTNode */

/***
 * Create a new ASTUnaryNode.
 *
 * @param type The node type.
 * @param loc The Location of the node.
 * @param operand The operand.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewUnaryNode(ASTNodeType type, Location loc, ASTNode *operand);

/***
 * Create a new ASTBinaryNode.
 *
 * @param type The node type.
 * @param loc The Location of the node.
 * @param lhs The left child.
 * @param rhs The right child.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *lhs, ASTNode *rhs);

/***
 * Create a new ASTConditionalNode.
 *
 * @param type The node type.
 * @param loc The Location of the node.
 * @param condition The condition expression node.
 * @param body The body block node.
 * @param else_ The else_ block node.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTNode *body, ASTNode *else_);

/***
 * Create a new ASTLoopNode.
 *
 * @param loc The Location of the node.
 * @param init The initializer caluse expression node.
 * @param cond The condition caluse expression node.
 * @param inc The increment caluse expression node.
 * @param body The body block node.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewLoopNode(Location loc, ASTNode *init, ASTNode *cond, ASTNode *inc, ASTNode *body);

/***
 * Create a new ASTLiteralValueNode.
 *
 * @param type The node type.
 * @param loc The Location of the node.
 * @param value The LiteralValue to store in the node.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewLiteralValueNode(ASTNodeType type, Location loc, LiteralValue value);

/***
 * Create a new ASTObjNode.
 * NOTE: Ownership of 'obj' is NOT taken.
 *
 * @param type The node type.
 * @param loc The Location of the node.
 * @param obj The ASTObj to store in the node.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewObjNode(ASTNodeType type, Location loc, ASTObj *obj);

/***
 * Create a new ASTIdentifierNode (node type: ND_IDENTIFIER).
 *
 * @param loc The Location of the node.
 * @param str The ASTString containing the identifier.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewIdentifierNode(Location loc, ASTString str);

/***
 * Create a new ASTListNode.
 *
 * @param type The type of the node.
 * @param loc The Location of the node.
 * @param scope The ScopeID of the block the node represents.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewListNode(ASTNodeType type, Location loc, ScopeID scope);

/***
 * Free an AST.
 *
 * @param n The root node of the tree.
 * @param recursive Set to false to only free the node without its chidlren.
 ***/
void astNodeFree(ASTNode *n, bool recursive);

/***
 * Print an AST.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astNodePrint(FILE *to, ASTNode *n);


/* ASTObj */

/***
 * Create a new ASTObj.
 *
 * @param type The type of the object.
 * @param loc The location of the object.
 * @param name The name of the object.
 * @param data_type The data type of the object.
 * @return The new object.
 ***/
ASTObj *astNewObj(ASTObjType type, Location loc, ASTString name, Type *data_type);

/***
 * Free an ASTObj.
 *
 * @param obj The object to free.
 ***/
void astObjFree(ASTObj *obj);

/***
 * Print an ASTObj.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 ***/
void astObjPrint(FILE *to, ASTObj *obj);


/* ASTModule */

/***
 * Create a new ASTModule.
 *
 * @param name The ASTString containing the module's name.
 * @return A new module.
 ***/
ASTModule *astModuleNew(ASTString name);

/***
 * Free an ASTModule.
 *
 * @param module The module to free.
 ***/
void astModuleFree(ASTModule *module);

/***
 * Intern a type saving it in 'module's type table.
 * NOTE: 'ty' MUST be heap allocated. ownership of it is taken.
 *
 * @param module The module to save the type in.
 * @param ty A heap allocated type to intern.
 ****/
Type *astModuleAddType(ASTModule *module, Type *ty);

/***
 * Print an ASTModule.
 *
 * @param to The stream to print to.
 * @param module The module to print.
 ****/
void astModulePrint(FILE *to, ASTModule *module);


/* ASTProgram */

/***
 * Initialize an ASTProgram.
 *
 * @param prog The program to initialize.
 ***/
void astProgramInit(ASTProgram *prog);

/***
 * Free an ASTProgram.
 *
 * @param prog The program to free.
 ***/
void astProgramFree(ASTProgram *prog);

/***
 * Print an ASTProgram.
 *
 * @param to The stream to print to.
 * @param prog The program to print.
 ***/
void astProgramPrint(FILE *to, ASTProgram *prog);

/***
 * Intern a string.
 * NOTE: If 'str' is a String, ownership of it is taken.
 *
 * @param prog The ASTProgram to save the string in.
 * @param str The string to intern (if 'str' is a String, ownership of it is taken).
 ***/
ASTString astProgramAddString(ASTProgram *prog, char *str);

/***
 * Add an ASTModule to an ASTProgram and return its id.
 * NOTE: ownership of 'module' is taken.
 *
 * @param prog The ASTProgram to add the module to.
 * @param module The module to add (ownership of it is taken).
 * @return The ModuleID of the added module.
 ***/
ModuleID astProgramAddModule(ASTProgram *prog, ASTModule *module);

/***
 * Get an ASTModule using its ModuleID from an ASTProgram.
 *
 * @param prog The ASTProgram to get the module from.
 * @param id The ModuleID of the module to get.
 * @return A pointer to the module or NULL if the id is out of bounds.
 ***/
ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id);

#endif // AST_H
