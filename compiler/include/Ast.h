#ifndef AST_H
#define AST_H

#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Strings.h"
#include "Table.h"
#include "Array.h"
#include "Arena.h"


// Can't include Types.h as it includes this file.
typedef struct type Type; // from Types.h

/* pre-declarations */
typedef struct ast_obj ASTObj;
typedef u64 ModuleID;
#define EMPTY_MODULE_ID ((ModuleID)-1)

/* ASTString */

// An ASTInternedString holds a unique string.
// It is guaranteed that to equal ASTInternedStrings will point
// to the same string in memory.
typedef char *ASTInternedString;

// An ASTString is an interned string with a location.
// It is used to represent strings in the source code (such as identifiers & string literals).
// [data] should NOT be changed.
// [data] can be used as a 'String' (with stringDuplicate() for example) as long as it isn't changed.
typedef struct ast_string {
    Location location;
    ASTInternedString data;
} ASTString;

/***
 * Print an ASTString.
 *
 * @param to The stream to print to.
 * @param str The ASTString to print.
 ***/
void astStringPrint(FILE *to, ASTString *str);


/* ConstantValue */

typedef enum value_type {
    VAL_NUMBER,
    VAL_STRING
} ValueType;

typedef struct value {
    ValueType type;
    union {
        u64 number;
        ASTString string;
    } as;
} Value;

/***
 * Create a new Value.
 *
 * @param literal_type (type: ValueType) The type of the value.
 * @param value_name (type: C identifier) The name of the value's field in the 'as' union.
 * @param value (type: any) The value itself (must be a constexpr).
 ***/
#define MAKE_VALUE(value_type, value_name, value) ((Value){.type = (value_type), .as.value_name = (value)})

/***
 * Print a Value.
 *
 * @param to The stream to print to.
 * @param value The value to print.
 ***/
void valuePrint(FILE *to, Value value);


/* Scope */

typedef struct scope_id {
    ModuleID module;
    usize index; // Index into the ASTModule::scopes array.
} ScopeID;

// FIXME: find a better way to represent an empty ScopeID.
#define EMPTY_SCOPE_ID ((ScopeID){.module = EMPTY_MODULE_ID, .index = (usize)-1})

typedef struct scope {
    Array objects; // Array<ASTObj *> - owns the objects stored in all the tables below.
    Table variables; // Table<ASTInternedString, ASTObj *>
    Table functions; // Table<ASTInternedString, ASTObj *>
    Table structures; // Table<ASTInternedString, ASTObj *>
    //Table enums; // Table<ASTInternedString, ASTObj *>
    Table types; // Table<ASTInternedString, Type *>
    ScopeID parent;
    struct { // The Array type cannot be used because ScopeID is not used as a pointer type.
        ScopeID *children_scope_ids;
        usize length;
        usize capacity;
    } children;
} Scope;

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

typedef enum control_flow {
    CF_NONE,
    CF_NEVER_RETURNS,
    CF_MAY_RETURN,
    CF_ALWAYS_RETURNS,
    __CF_STATE_COUNT
} ControlFlow;

/***
 * Update a ControlFlow with a new one.
 *
 * @param old The old ControlFlow.
 * @param new The new ControlFlow.
 * @return The updated ControlFlow.
 ***/
ControlFlow controlFlowUpdate(ControlFlow old, ControlFlow new);


/* AST nodes - common */

#define NODE_IS(node, type_) ((node)->type == (type_))

/* AST nodes - ASTExprNode */

typedef enum ast_expression_node_type {
    // Constant value nodes
    EXPR_NUMBER_CONSTANT,
    EXPR_STRING_CONSTANT,

    // Obj nodes
    EXPR_VARIABLE,
    EXPR_FUNCTION,

    // Binary nodes
    EXPR_ASSIGN,
    EXPR_PROPERTY_ACCESS,
    EXPR_ADD,
    EXPR_SUBTRACT,
    EXPR_MULTIPLY,
    EXPR_DIVIDE,
    EXPR_EQ, EXPR_NE,
    EXPR_LT, EXPR_LE,
    EXPR_GT, EXPR_GE,

    // Unary nodes
    EXPR_NEGATE,
    EXPR_ADDROF, // For pointers: &<obj>
    EXPR_DEREF, // Pointer dereference: *<obj>

    // Call node
    EXPR_CALL,

    // Other nodes

    // An identifier will be replaced with an object.
    // It exists because the parser doesn't always have all the information
    // needed to build an object, so the identifier us used instead,
    // and later the validator replaces it with an object.
    EXPR_IDENTIFIER,
    __EXPR_TYPE_COUNT
} ASTExprNodeType;

// The base expression node type.
typedef struct ast_expression_node {
    ASTExprNodeType type;
    Location location;
    Type *data_type; // The data type the expression evaluates to.
} ASTExprNode;

// Stores a constant value using the 'Value' container type.
typedef struct ast_constant_value_expr {
    ASTExprNode header;
    Value value;
} ASTConstantValueExpr;

// Stores a pointer to an ASTObj.
typedef struct ast_obj_expr {
    ASTExprNode header;
    ASTObj *obj;
} ASTObjExpr;

// Used for unary expressions (such as -<lval/rval>, &<lval>, *<lval> etc.).
typedef struct ast_unary_expr {
    ASTExprNode header;
    ASTExprNode *operand;
} ASTUnaryExpr;

// Used for binary expressions (such as <lval/rval> + <lval/rval> etc.)
typedef struct ast_binary_expr {
    ASTExprNode header;
    ASTExprNode *lhs;
    ASTExprNode *rhs;
} ASTBinaryExpr;

typedef struct ast_call_expr {
    ASTExprNode header;
    ASTExprNode *callee;
    Array arguments; // Array<ASTExprNode *>
} ASTCallExpr;

typedef struct ast_identifier_expr {
    ASTExprNode header;
    ASTString id;
} ASTIdentifierExpr;

#define AS_CONSTANT_VALUE_EXPR(expr) ((ASTConstantValueExpr *)(expr))
#define AS_OBJ_EXPR(expr) ((ASTObjExpr *)(expr))
#define AS_UNARY_EXPR(expr) ((ASTUnaryExpr *)(expr))
#define AS_BINARY_EXPR(expr) ((ASTBinaryExpr *)(expr))
#define AS_CALL_EXPR(expr) ((ASTCallExpr *)(expr))
#define AS_IDENTIFIER_EXPR(expr) ((ASTIdentifierExpr *)(expr))

/***
 * Print an ASTExprNode tree.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astExprNodePrint(FILE *to, ASTExprNode *n);

/***
 * Create a new ASTConstantValueExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param value The Value to store in the node.
 * @param ty (optional) The type of the value.
 * @return The node as an ASTExprNode.
 ***/
ASTExprNode *astNewConstantValueExpr(Allocator *a, ASTExprNodeType type, Location loc, Value value, Type *value_ty);

/***
 * Create a new ASTObjExpr node.
 * NOTE: Ownership of 'obj' is NOT taken.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param obj The ASTObj to store in the node.
 * @return The node as an ASTExprNode.
 ***/
ASTExprNode *astNewObjExpr(Allocator *a, ASTExprNodeType type, Location loc, ASTObj *obj);


// TODO: unary+binary expr: add type hint?
/***
 * Create a new ASTUnaryExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param operand The operand.
 * @return The node as an ASTExprNode.
 ***/
ASTExprNode *astNewUnaryExpr(Allocator *a, ASTExprNodeType type, Location loc, ASTExprNode *operand);

/***
 * Create a new ASTBinaryExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param lhs The left child.
 * @param rhs The right child.
 * @return The node as an ASTExprNode.
 ***/
ASTExprNode *astNewBinaryExpr(Allocator *a, ASTExprNodeType type, Location loc, ASTExprNode *lhs, ASTExprNode *rhs);

/***
 * Create a new ASTCallExpr node.
 * NOTE: Ownership of [arguments] is NOT taken.
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param callee The callee.
 * @param arguments An Array<ASTExprNode *> containing the arguments.
 ***/
ASTExprNode *astNewCallExpr(Allocator *a, Location loc, ASTExprNode *callee, Array *arguments);


/***
 * Create a new ASTIdentifierExpr node (node type: EXPR_IDENTIFIER).
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param id An ASTString containing the identifier.
 * @return The node as an ASTExprNode.
 ***/
ASTExprNode *astNewIdentifierExpr(Allocator *a, Location loc, ASTString id);


/* AST node - ASTStmtNode */

typedef enum ast_stmt_node_type {
    // VarDecl node
    STMT_VAR_DECL,

    // Block node
    STMT_BLOCK,

    // Conditional node
    STMT_IF,

    // Loop node
    STMT_WHILE_LOOP,

    // Expr nodes
    STMT_RETURN,
    STMT_DEFER,
    STMT_EXPR,

    __STMT_TYPE_COUNT
} ASTStmtNodeType;

typedef struct ast_stmt_node {
    ASTStmtNodeType type;
    Location location;
} ASTStmtNode;

typedef struct ast_var_decl_stmt {
    ASTStmtNode header;
    ASTObj *variable;
    ASTExprNode *initializer; // optional
} ASTVarDeclStmt;

typedef struct ast_block_stmt {
    ASTStmtNode header;
    ScopeID scope;
    ControlFlow control_flow;
    Array nodes; // Array<AstStmtNode *>
} ASTBlockStmt;

typedef struct ast_conditional_stmt {
    ASTStmtNode header;
    ASTExprNode *condition;
    ASTBlockStmt *then;
    ASTStmtNode *else_;
} ASTConditionalStmt;

typedef struct ast_loop_stmt {
    ASTStmtNode header;
    ASTStmtNode *initializer;
    ASTExprNode *condition;
    ASTExprNode *increment;
    ASTBlockStmt *body;
} ASTLoopStmt;

typedef struct ast_expr_stmt {
    ASTStmtNode header;
    ASTExprNode *expr;
} ASTExprStmt;

#define AS_VAR_DECL_STMT(stmt) ((ASTVarDeclStmt *)(stmt))
#define AS_BLOCK_STMT(stmt) ((ASTBlockStmt *)(stmt))
#define AS_CONDITIONAL_STMT(stmt) ((ASTConditionalStmt *)(stmt))
#define AS_LOOP_STMT(stmt) ((ASTLoopStmt *)(stmt))
#define AS_EXPR_STMT(stmt) ((ASTExprStmt *)(stmt))

/***
 * Print an ASTStmtNode tree.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astStmtNodePrint(FILE *to, ASTStmtNode *n);

/***
 * Create a new ASTVarDeclStmt node (node type: STMT_VAR_DECL).
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param obj The ASTObj of the variable to declare.
 * @param initializer (optional) The initializer.
 * @return The node as an ASTStmtNode.
 ***/
ASTStmtNode *astNewVarDeclStmt(Allocator *a, Location loc, ASTObj *variable, ASTExprNode *initializer);


/***
 * Create a new ASTBlockStmt node (STMT_BLOCK).
 * NOTE: Ownership of [nodes] is NOT taken.
 *
 * @param a The Allocator to use to allocate the node.
 * @param loc The Location of the node.
 * @param scope The ScopeID of the block the node represents.
 * @param control_flow The control flow for the block.
 * @param nodes An Array<ASTStmtNode *> containing the nodes in the block.
 * @return The node as an ASTStmtNode.
 ***/
ASTStmtNode *astNewBlockStmt(Allocator *a, Location loc, ScopeID scope, ControlFlow control_flow, Array *nodes);

/***
 * Create a new ASTConditionalStmt node (STMT_IF).
 *
 * @param a The Allocator to use to allocate the node.
 * @param loc The Location of the node.
 * @param condition The condition expression.
 * @param then The body (to be executed if the condition evaluates to 'true').
 * @param else_ (optional) The else clause (to be executed if the condition evaluates to 'false').
 * @return The node as an ASTStmtNode.
 ***/
ASTStmtNode *astNewConditionalStmt(Allocator *a, Location loc, ASTExprNode *condition, ASTBlockStmt *then, ASTStmtNode *else_);

/***
 * Create a new ASTLoopStmt node.
 *
 * @param a The Allocator to use to allocate the node.
 * @param type The stmt node type for the new node.
 * @param loc The Location of the node.
 * @param initializer (optional) The initializer statement.
 * @param condition The condition for the loop.
 * @param increment (optional) The increment expression.
 * @param body The body of the loop.
 * @return The node as an ASTStmtNode.
 ***/
ASTStmtNode *astNewLoopStmt(Allocator *a, ASTStmtNodeType type, Location loc, ASTStmtNode *initializer, ASTExprNode *condition, ASTExprNode *increment, ASTBlockStmt *body);

/***
 * Create a new ASTExprStmt node.
 *
 * @param a The Allocator to use to allocate the node.
 * @param type The stmt node type for the new node
 * @param loc The Location of the node.
 * @param expr The expression.
 * @return The node as an ASTStmtNode.
 ***/
ASTStmtNode *astNewExprStmt(Allocator *a, ASTStmtNodeType type, Location loc, ASTExprNode *expr);


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
            Array defers; // Array<ASTExprStmt *>
            ASTBlockStmt *body; // Note: contains the function's scope ScopeID.
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

// A ModuleID is an index into the ASTProgram::modules array.
typedef u64 ModuleID;

// An ASTModule holds all of a modules data
// which includes functions, structs, enums, global variables, types etc.
typedef struct ast_module {
    ModuleID id;
    ASTString name; // FIXME: Root module name has no location.
    struct {
        Arena storage;
        Allocator alloc;
    } ast_allocator;
    Array scopes; // Array<Scope *>
    Scope *module_scope; // Owned by above array.
    Array globals; // Array<ASTVarDeclStmt *> (ND_VAR_DECL)
} ASTModule;

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
    Table strings; // Table<ASTInternedString, String>
    // Holds all modules in a program.
    Array modules; // Array<ASTModule *>
} ASTProgram;

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
 * NOTE: Ownership of [str] is NOT taken.
 *
 * @param prog The ASTProgram to save the string in.
 * @param str The string to intern (if 'str' is a String, ownership of it is taken).
 ***/
ASTInternedString astProgramAddString(ASTProgram *prog, char *str);

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
