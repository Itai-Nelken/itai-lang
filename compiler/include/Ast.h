#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "common.h" // u64
#include "memory.h" // Allocator
#include "Token.h" // Location
#include "Strings.h"
#include "Table.h"
#include "Types.h"
#include "Arena.h"

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
    LIT_NUMBER,
    LIT_STRING
} LiteralValueType;

typedef struct literal_value {
    LiteralValueType type;
    union {
        u64 number; // Prefix operators are handled by the parser, so 'number' can be unsigned.
        ASTString str;
    } as;
} LiteralValue;


/* Scope */

#define FUNCTION_SCOPE_DEPTH 1

typedef struct scope_id {
    ModuleID module;
    usize index; // Index into the ASTModule::scopes array.
} ScopeID;

// FIXME: find a better way to represent an empty ScopeID
#define EMPTY_SCOPE_ID() ((ScopeID){.module = 0, .index = (usize)-1})

typedef struct scope {
    bool is_block_scope;
    u32 depth; // For block scopes. // FIXME: find way to store "empty" depth.
    Array objects;
    Table variables; // Table<ASTString, ASTObj *>
    Table functions; // Table<ASTString, ASTObj *>
    Table structures; // Table<ASTString, ASTObj *>
    // Table enums; // Table<ASTString, ASTObj *>
    Table types; // Table<ASTString, Type *>
    ScopeID parent;
    struct { // Array cannot be used because ScopeID is not used as a pointer type.
        ScopeID *children_scope_ids;
        usize length;
        usize capacity;
    } children;
} Scope;

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
    ND_STRING_LITERAL,

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
    ND_DEFER,
    ND_ADDROF, // For pointers: &<obj>
    ND_DEREF, // Pointer dereference: *<obj>

    // list nodes
    ND_BLOCK,
    ND_ARGS,

    // other nodes

    // An identifier will be replaced with an object.
    // It exists because the parser doesn't always have all the information
    // needed to build an object, so the identifier us used instead,
    // and later the validator replaces it with an object.
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
    Type *ty; // For postfix types, can be NULL;
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


/* Attribute */

// Note: update tables in attributeTypeString() and attribute_type_name() when adding new types.
typedef enum attribute_type {
    ATTR_SOURCE,
    //ATTR_DESTRUCTOR
} AttributeType;

typedef struct attribute {
    AttributeType type;
    Location location;
    union {
        ASTString source;
        //ASTObj *destructor;
    } as;
} Attribute;


/* ASTObj */

// An ASTObj holds a object which in this context
// means functions, variables, structs, enums etc.
typedef enum ast_obj_type {
    OBJ_VAR, OBJ_FN,
    OBJ_STRUCT,
    OBJ_EXTERN_FN,
    OBJ_TYPE_COUNT
} ASTObjType;

typedef struct ast_obj {
    ASTObjType type;
    Location location, name_location;
    ASTString name;
    Type *data_type;

    union {
        //struct {
        //  bool is_const;
        //  bool is_parameter;
        //  bool is_struct_field;
        //} var; // OBJ_VAR
        struct {
            Array parameters; // Array<ASTObj *> (OBJ_VAR)
            Type *return_type;
            Array defers; // Array<ASTNode *>
            ASTListNode *body; // Note: contains the function's scope ScopeID.
        } fn; // OBJ_FN
        struct {
            ScopeID scope;
        } structure; // OBJ_STRUCT
        struct {
            Array parameters; // Array<ASTObj *>
            Type *return_type;
            Attribute *source_attr; // After validating, guaranteed to be an ATTR_SOURCE.
        } extern_fn; // OBJ_EXTERN_FN
    } as;
} ASTObj;

/* ASTModule */

// A ModuleID is an index into the ASTProgram::modules array.
typedef usize ModuleID;

// An ASTModule holds all of a modules data
// which includes functions, structs, enums, global variables, types etc.
typedef struct ast_module {
    ModuleID id;
    ASTString name;
    struct {
        Arena storage;
        Allocator alloc;
    } ast_allocator;
    Array scopes; // Array<Scope *>
    Scope *module_scope; // Owned by above array.
    Array globals; // Array<ASTNode *> (ND_VAR_DECL or ND_ASSIGN)
} ASTModule;

/* ASTProgram */

// An ASTProgram holds all the information
// belonging to a program like functions & types.
typedef struct ast_program {
    struct {
        // Primitive types (are owned by the root module).
        // Note: astProgramInit() has to be updated when adding new primitives.
        Type *void_;
        Type *int32;
        Type *uint32;
        Type *str;
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


/* Scope */

/***
 * Create a new Scope.
 *
 * @param parent_scope The ScopeID of the parent scope.
 * @param is_block_scope Set to true if the scope will be a block scope.
 * @return The new scope.
 ***/
Scope *scopeNew(ScopeID parent_scope, bool is_block_scope);

/***
 * Add a child to a Scope.
 *
 * @param parent The parent scope.
 * @param child_id The ScopeID of the child scope to add.
 ***/
void scopeAddChild(Scope *parent, ScopeID child_id);

/***
 * Intern a type saving it in scope's type table.
 * NOTE: 'ty' MUST be heap allocated. ownership of it is taken.
 *
 * @param scope The scope to save the type in.
 * @param ty A heap allocated type to intern.
 ****/
Type *scopeAddType(Scope *scope, Type *ty);

/***
 * Free a list of Scope's.
 *
 * @param scope_list The head of the scope list.
 ***/
void scopeFree(Scope *scope_list);


/***
 * Print a Scope to stream [stream].
 * NOTE: all child scopes are printed recursively as well.
 *
 * @param to The stream to print to.
 * @param scope The scope to print.
 ***/
void scopePrint(FILE *to, Scope *scope);

/***
 * Print a ScopeID to stream [stream].
 *
 * @param to The stream to print to.
 * @param scope_id The ScopeID to print.
 * @param compact Print a compact version.
 ***/
void scopeIDPrint(FILE *to, ScopeID scope_id, bool compact);

/***
 * Get a struct using its name from scope [sc].
 *
 * @param sc The scope in which to search for the struct.
 * @param name The name of the struct to search for.
 * @return The struct or NULL if it doesn't exist.
 ***/
ASTObj *scopeGetStruct(Scope *sc, ASTString name);


/* ControlFlow */

ControlFlow controlFlowUpdate(ControlFlow old, ControlFlow new);


/* ASTNode */

/***
 * Create a new ASTUnaryNode.
 *
 * @param a The allocator to use.
 * @param type The node type.
 * @param loc The Location of the node.
 * @param operand The operand.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewUnaryNode(Allocator *a, ASTNodeType type, Location loc, ASTNode *operand);

/***
 * Create a new ASTBinaryNode.
 *
 * @param a The allocator to use.
 * @param type The node type.
 * @param loc The Location of the node.
 * @param lhs The left child.
 * @param rhs The right child.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewBinaryNode(Allocator *a, ASTNodeType type, Location loc, ASTNode *lhs, ASTNode *rhs);

/***
 * Create a new ASTConditionalNode.
 *
 * @param a The allocator to use.
 * @param type The node type.
 * @param loc The Location of the node.
 * @param condition The condition expression node.
 * @param body The body block node.
 * @param else_ The else_ block node.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewConditionalNode(Allocator *a, ASTNodeType type, Location loc, ASTNode *condition, ASTNode *body, ASTNode *else_);

/***
 * Create a new ASTLoopNode.
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param init The initializer caluse expression node.
 * @param cond The condition caluse expression node.
 * @param inc The increment caluse expression node.
 * @param body The body block node.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewLoopNode(Allocator *a, Location loc, ASTNode *init, ASTNode *cond, ASTNode *inc, ASTNode *body);

/***
 * Create a new ASTLiteralValueNode.
 *
 * @param a The allocator to use.
 * @param type The node type.
 * @param loc The Location of the node.
 * @param value The LiteralValue to store in the node.
 * @param ty The type of the literal (optional).
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewLiteralValueNode(Allocator *a, ASTNodeType type, Location loc, LiteralValue value, Type *ty);

/***
 * Create a new ASTObjNode.
 * NOTE: Ownership of 'obj' is NOT taken.
 *
 * @param a The allocator to use.
 * @param type The node type.
 * @param loc The Location of the node.
 * @param obj The ASTObj to store in the node.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewObjNode(Allocator *a, ASTNodeType type, Location loc, ASTObj *obj);

/***
 * Create a new ASTIdentifierNode (node type: ND_IDENTIFIER).
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param str The ASTString containing the identifier.
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewIdentifierNode(Allocator *a, Location loc, ASTString str);

/***
 * Create a new ASTListNode.
 *
 * @param a The Allocator to use to allocate the node.
 * @param type The type of the node.
 * @param loc The Location of the node.
 * @param scope The ScopeID of the block the node represents.
 * @param node_count The amount of nodes (if unknown, can be 0).
 * @return The node as an ASTNode.
 ***/
ASTNode *astNewListNode(Allocator *a, ASTNodeType type, Location loc, ScopeID scope, size_t node_count);

/***
 * Print an AST.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astNodePrint(FILE *to, ASTNode *n);


/* Attribute */

/***
 * Create a new Attributute.
 *
 * @param type The AttributeType of the new attribute.
 * @param loc The Location where the attribute was defined.
 * @return The new attribute (heap allocated).
 ***/
Attribute *attributeNew(AttributeType type, Location loc);

/***
 * Free an Attribute.
 *
 * @param a The Attribute to free.
 ***/
void attributeFree(Attribute *a);

/***
 * Print an Attribute.
 *
 * @param to The stream to print to.
 * @param a The Attribute to print.
 ***/
void attributePrint(FILE *to, Attribute *a);

/***
 * Return a user friendly string version of an AttributeType.
 *
 * @param type An AttributeType.
 * @return The type as a user friendly string.
 ***/
const char *attributeTypeString(AttributeType type);

/* ASTObj */

/***
 * Create a new ASTObj.
 *
 * @param type The type of the object.
 * @param loc The location of the object.
 * @param name_loc The location of the name of the object.
 * @param name The name of the object.
 * @param data_type The data type of the object.
 * @return The new object.
 ***/
ASTObj *astNewObj(ASTObjType type, Location loc, Location name_loc, ASTString name, Type *data_type);

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

/***
 * Print an ASTObj in a compact format.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 ***/
void astObjPrintCompact(FILE *to, ASTObj *obj);

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
 * Print an ASTModule.
 *
 * @param to The stream to print to.
 * @param module The module to print.
 ****/
void astModulePrint(FILE *to, ASTModule *module);

/***
 * Add a scope to an ASTModule.
 *
 * @param module The module to add the scope to.
 * @param scope The scope to add.
 * @return The ScopeID of the added scope.
 ***/
ScopeID astModuleAddScope(ASTModule *module, Scope *scope);

/***
 * Get a Scope stored in an ASTModule.
 * NOTE: This function will panic if [id] is invalid.
 *
 * @param module The module containing the scope.
 * @param id The ScopeID that refers to the wanted scope.
 * @return A pointer to the Scope refered to by [id].
 ***/
Scope *astModuleGetScope(ASTModule *module, ScopeID id);

/***
 * Return the ScopeID of [module]'s scope.
 *
 * @param module An ASTModule.
 * @return The ScopeID of [module]'s scope.
 ***/
ScopeID astModuleGetModuleScopeID(ASTModule *module);


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
 * NOTE: This function will panic if [id] is invalid.
 *
 * @param prog The ASTProgram to get the module from.
 * @param id The ModuleID of the module to get.
 * @return A pointer to the module or NULL if the id is out of bounds.
 ***/
ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id);

#endif // AST_H
