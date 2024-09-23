#ifndef CHECKED_AST_H
#define CHECKED_AST_H

#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Strings.h"
#include "Table.h"
#include "Array.h"
#include "Arena.h"
#include "Ast/ast_common.h"


/* pre-declarations */

// Can't include Types/CheckedType.h because it includes this file.
typedef struct checked_type CheckedType; // from Types/CheckedType.h

typedef struct ast_checked_obj ASTCheckedObj;
typedef struct ast_checked_block_stmt ASTCheckedBlockStmt;


/* CheckedScope */

// TODO: Are two exact declarations for scope (parsed & checked) really needed?
typedef struct checked_scope {
    bool is_block_scope;
    Array objects; // Array<ASTCheckedObj *> - owns the objects stored in all the tables below.
    Table variables; // Table<ASTInternedString, ASTCheckedObj *>
    Table functions; // Table<ASTInternedString, ASTCheckedObj *>
    Table structures; // Table<ASTInternedString, ASTCheckedObj *>
    //Table enums; // Table<ASTInternedString, ASTCheckedObj *>
    Table types; // Table<CheckedType *, void>
    ScopeID parent;
    struct { // The Array type cannot be used because ScopeID is not used as a pointer type.
        ScopeID *children_scope_ids;
        usize length;
        usize capacity;
    } children;
} CheckedScope;

/***
 * Create a new CheckedScope.
 *
 * @param parent_scope The ScopeID of the parent scope.
 * @param is_block_scope Set to true if the scope will be a block scope.
 * @return The new scope.
 ***/
CheckedScope *checkedScopeNew(ScopeID parent_scope, bool is_block_scope);

/***
 * Add a child to a CheckedScope.
 *
 * @param parent The parent scope.
 * @param child_id The ScopeID of the child scope to add.
 ***/
void checkedScopeAddChild(CheckedScope *parent, ScopeID child_id);

/***
 * Intern a CheckedType saving it in scope's type table.
 * If the type is already in the scope, the previous type is returned.
 * NOTE: 'ty' MUST be heap allocated! Ownership of it is taken!
 *
 * @param scope The scope to save the type in.
 * @param type A heap allocated CheckedType to intern.
 ****/
CheckedType *checkedScopeAddType(CheckedScope *scope, CheckedType *type);

/***
 * Free a list of CheckedScope's.
 *
 * @param scope_list The head of the scope list.
 ***/
void checkedScopeFree(CheckedScope *scope_list);


/***
 * Print a CheckedScope to stream [stream].
 * NOTE: all child scopes are printed recursively as well.
 *
 * @param to The stream to print to.
 * @param scope The scope to print.
 ***/
void checkedScopePrint(FILE *to, CheckedScope *scope);

/***
 * Get a struct using its name from scope [sc].
 *
 * @param sc The scope in which to search for the struct.
 * @param name The name of the struct to search for.
 * @return The struct or NULL if it doesn't exist.
 ***/
ASTCheckedObj *checkedScopeGetStruct(CheckedScope *sc, ASTString name);


/* ASTPCheckedObj */

// An ASTCheckedObj holds an object which in this context
// means functions, variables, structs, enums etc.
typedef struct ast_checked_obj {
    ASTObjType type;
    Location location;
    ASTString name;
    CheckedType *data_type;

    union {
        //struct {
        //  bool is_const;
        //  bool is_parameter;
        //  bool is_struct_field;
        //} var; // OBJ_VAR
        struct {
            Array parameters; // Array<ASTCheckedObj *> (OBJ_VAR)
            CheckedType *return_type;
            ASTCheckedBlockStmt *body; // Note: contains the function's scope ScopeID.
        } fn; // OBJ_FN
        struct {
            ScopeID scope;
        } structure; // OBJ_STRUCT
        struct {
            Array parameters; // Array<ASTCheckedObj *>
            CheckedType *return_type;
            Attribute *source_attr; // After validating, guaranteed to be an ATTR_SOURCE.
        } extern_fn; // OBJ_EXTERN_FN
    } as;
} ASTCheckedObj;

/***
 * Create a new ASTCheckedObj.
 *
 * @param type The type of the object.
 * @param loc The location of the object.
 * @param name_loc The location of the name of the object.
 * @param name The name of the object.
 * @param data_type The data type of the object.
 * @return The new object.
 ***/
ASTCheckedObj *astNewCheckedObj(ASTObjType type, Location loc, ASTString name, CheckedType *data_type);

/***
 * Free an ASTCheckedObj.
 *
 * @param obj The object to free.
 ***/
void astFreeCheckedObj(ASTCheckedObj *obj);

/***
 * Print an ASTCheckedObj.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 ***/
void astPrintCheckedObj(FILE *to, ASTCheckedObj *obj);

/***
 * Print an ASTCheckedObj in a compact format.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 ***/
void astCheckedObjPrintCompact(FILE *to, ASTCheckedObj *obj);


/* ASTCheckedModule */

// An ASTCheckedModule holds all of a modules data
// which includes functions, structs, enums, global variables, types etc.
typedef struct ast_checked_module {
    ModuleID id;
    ASTString name; // FIXME: Root module name has no location.
    struct {
        Arena storage;
        Allocator alloc;
    } ast_allocator;
    Array scopes; // Array<CheckedScope *>
    CheckedScope *module_scope; // Owned by the above array.
    Array globals; // Array<ASTCheckedVarDeclStmt *> (ND_VAR_DECL) - Stores declarations for module scope variables.
} ASTCheckedModule;

/***
 * Create a new ASTCheckedModule.
 *
 * @param name The ASTString containing the module's name.
 * @return A new module.
 ***/
ASTCheckedModule *astNewCheckedModule(ASTString name);

/***
 * Free an ASTCheckedModule.
 *
 * @param module The module to free.
 ***/
void astFreeCheckedModule(ASTCheckedModule *module);

/***
 * Print an ASTCheckedModule.
 *
 * @param to The stream to print to.
 * @param module The module to print.
 ****/
void astPrintCheckedModule(FILE *to, ASTCheckedModule *module);

/***
 * Add a scope to an ASTCheckedModule.
 *
 * @param module The module to add the scope to.
 * @param scope The scope to add.
 * @return The ScopeID of the added scope.
 ***/
ScopeID astCheckedModuleAddScope(ASTCheckedModule *module, CheckedScope *scope);

/***
 * Get a CheckedScope stored in an ASTCheckedModule.
 * NOTE: This function will panic if [id] is invalid.
 *
 * @param module The module containing the scope.
 * @param id The ScopeID that refers to the wanted scope.
 * @return A pointer to the CheckedScope refered to by [id].
 ***/
CheckedScope *astCheckedModuleGetScope(ASTCheckedModule *module, ScopeID id);

/***
 * Return the ScopeID of [module]'s scope.
 *
 * @param module An ASTCheckedModule.
 * @return The ScopeID of [module]'s scope.
 ***/
ScopeID astCheckedModuleGetModuleScopeID(ASTCheckedModule *module);


/* ASTCheckedProgram */

// An ASTCheckedProgram holds all the information
// belonging to a program like functions & types.
typedef struct ast_checked_program {
    struct {
        // Primitive types (are owned by the root module).
        // Note: ASTCheckedProgram & astCheckedProgramInit() have to be updated when adding new primitives.
        CheckedType *void_;
        CheckedType *int32;
        CheckedType *uint32;
        CheckedType *str;
    } primitives;
    // Holds a single copy of each string in the entire program.
    ASTStringTable strings;
    // Holds all modules in a program.
    Array modules; // Array<ASTCheckedModule *>
} ASTCheckedProgram;

/***
 * Initialize an ASTCheckedProgram.
 *
 * @param prog The program to initialize.
 ***/
void astCheckedProgramInit(ASTCheckedProgram *prog);

/***
 * Free an ASTCheckedProgram.
 *
 * @param prog The program to free.
 ***/
void astCheckedProgramFree(ASTCheckedProgram *prog);

/***
 * Print an ASTCheckedProgram.
 *
 * @param to The stream to print to.
 * @param prog The program to print.
 ***/
void astCheckedProgramPrint(FILE *to, ASTCheckedProgram *prog);

/***
 * Intern a string.
 * NOTE: Ownership of [str] is NOT taken.
 *
 * @param prog The ASTCheckedProgram to save the string in.
 * @param str The string to intern.
 ***/
static inline ASTInternedString astCheckedProgramAddString(ASTCheckedProgram *prog, char *str) {
    return astStringTableAddString(&prog->strings, str);
}

/***
 * Add an ASTCheckedModule to an ASTCheckedProgram and return its id.
 * NOTE: ownership of 'module' is taken.
 *
 * @param prog The ASTCheckedProgram to add the module to.
 * @param module The module to add (ownership of it is taken).
 * @return The ModuleID of the added module.
 ***/
ModuleID astCheckedProgramAddModule(ASTCheckedProgram *prog, ASTCheckedModule *module);

/***
 * Get an ASTCheckedModule using its ModuleID from an ASTCheckedProgram.
 * NOTE: This function will panic if [id] is invalid.
 *
 * @param prog The ASTCheckedProgram to get the module from.
 * @param id The ModuleID of the module to get.
 * @return A pointer to the module or NULL if the id is out of bounds.
 ***/
ASTCheckedModule *astCheckedProgramGetModule(ASTCheckedProgram *prog, ModuleID id);


/* AST nodes - ASTCheckedExprNode */

typedef enum ast_checked_expression_node_type {
    // Constant value nodes
    CHECKED_EXPR_NUMBER_CONSTANT,
    CHECKED_EXPR_STRING_CONSTANT,

    // Obj nodes
    CHECKED_EXPR_VARIABLE,
    CHECKED_EXPR_FUNCTION,

    // Binary nodes
    CHECKED_EXPR_ASSIGN,
    CHECKED_EXPR_PROPERTY_ACCESS,
    CHECKED_EXPR_ADD,
    CHECKED_EXPR_SUBTRACT,
    CHECKED_EXPR_MULTIPLY,
    CHECKED_EXPR_DIVIDE,
    CHECKED_EXPR_EQ, CHECKED_EXPR_NE,
    CHECKED_EXPR_LT, CHECKED_EXPR_LE,
    CHECKED_EXPR_GT, CHECKED_EXPR_GE,

    // Unary nodes
    CHECKED_EXPR_NEGATE,
    CHECKED_EXPR_ADDROF, // For pointers: &<obj>
    CHECKED_EXPR_DEREF, // Pointer dereference: *<obj>

    // Call node
    CHECKED_EXPR_CALL,

    __CHECKED_EXPR_TYPE_COUNT
} ASTCheckedExprNodeType;

// The base expression node type.
typedef struct ast_checked_expression_node {
    ASTCheckedExprNodeType type;
    Location location;
    CheckedType *data_type; // The data type the expression evaluates to.
} ASTCheckedExprNode;

// Stores a constant value using the 'Value' container type.
typedef struct ast_checked_constant_value_expr {
    ASTCheckedExprNode header;
    Value value;
} ASTCheckedConstantValueExpr;

// Stores a pointer to an ASTCheckedObj.
typedef struct ast_checked_obj_expr {
    ASTCheckedExprNode header;
    ASTCheckedObj *obj;
} ASTCheckedObjExpr;

// Used for unary expressions (such as -<lval/rval>, &<lval>, *<lval> etc.).
typedef struct ast_checked_unary_expr {
    ASTCheckedExprNode header;
    ASTCheckedExprNode *operand;
} ASTCheckedUnaryExpr;

// Used for binary expressions (such as <lval/rval> + <lval/rval> etc.)
typedef struct ast_checked_binary_expr {
    ASTCheckedExprNode header;
    ASTCheckedExprNode *lhs;
    ASTCheckedExprNode *rhs;
} ASTCheckedBinaryExpr;

typedef struct ast_checked_call_expr {
    ASTCheckedExprNode header;
    ASTCheckedExprNode *callee;
    Array arguments; // Array<ASTCheckedExprNode *>
} ASTCheckedCallExpr;


/***
 * Print an ASTCheckedExprNode tree.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astCheckedExprNodePrint(FILE *to, ASTCheckedExprNode *n);

/***
 * Create a new ASTCheckedConstantValueExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param value The Value to store in the node.
 * @param ty (optional) The type of the value.
 * @return The node as an ASTCheckedExprNode.
 ***/
ASTCheckedExprNode *astNewCheckedConstantValueExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, Value value, CheckedType *value_ty);

/***
 * Create a new ASTCheckedObjExpr node.
 * NOTE: Ownership of 'obj' is NOT taken.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param obj The ASTCheckedObj to store in the node.
 * @return The node as an ASTCheckedExprNode.
 ***/
ASTCheckedExprNode *astNewCheckedObjExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, ASTCheckedObj *obj);


// TODO: unary+binary expr: add type hint?
/***
 * Create a new ASTCheckedUnaryExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param operand The operand.
 * @return The node as an ASTCheckedExprNode.
 ***/
ASTCheckedExprNode *astNewCheckedUnaryExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, ASTCheckedExprNode *operand);

/***
 * Create a new ASTCheckedBinaryExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param lhs The left child.
 * @param rhs The right child.
 * @return The node as an ASTCheckedExprNode.
 ***/
ASTCheckedExprNode *astNewCheckedBinaryExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, ASTCheckedExprNode *lhs, ASTCheckedExprNode *rhs);

/***
 * Create a new ASTCheckedCallExpr node.
 * NOTE: Ownership of [arguments] is NOT taken.
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param callee The callee.
 * @param arguments An Array<ASTCheckedExprNode *> containing the arguments.
 ***/
ASTCheckedExprNode *astNewCheckedCallExpr(Allocator *a, Location loc, ASTCheckedExprNode *callee, Array *arguments);


/* AST node - ASTCheckedStmtNode */

typedef enum ast_checked_stmt_node_type {
    // VarDecl node
    CHECKED_STMT_VAR_DECL,

    // Block node
    CHECKED_STMT_BLOCK,

    // Conditional node
    CHECKED_STMT_IF,

    // Loop node
    CHECKED_STMT_WHILE_LOOP,

    // Expr nodes
    CHECKED_STMT_RETURN,
    CHECKED_STMT_EXPR,

    // Defer node
    CHECKED_STMT_DEFER,

    __CHECKED_STMT_TYPE_COUNT
} ASTCheckedStmtNodeType;

typedef struct ast_checked_stmt_node {
    ASTCheckedStmtNodeType type;
    Location location;
} ASTCheckedStmtNode;

typedef struct ast_checked_var_decl_stmt {
    ASTCheckedStmtNode header;
    ASTCheckedObj *variable;
    ASTCheckedExprNode *initializer; // optional
} ASTCheckedVarDeclStmt;

typedef struct ast_checked_block_stmt {
    ASTCheckedStmtNode header;
    ScopeID scope;
    ControlFlow control_flow;
    Array nodes; // Array<AstCheckedStmtNode *>
} ASTCheckedBlockStmt;

typedef struct ast_checked_conditional_stmt {
    ASTCheckedStmtNode header;
    ASTCheckedExprNode *condition;
    ASTCheckedBlockStmt *then;
    ASTCheckedStmtNode *else_;
} ASTCheckedConditionalStmt;

typedef struct ast_checked_loop_stmt {
    ASTCheckedStmtNode header;
    ASTCheckedStmtNode *initializer;
    ASTCheckedExprNode *condition;
    ASTCheckedExprNode *increment;
    ASTCheckedBlockStmt *body;
} ASTCheckedLoopStmt;

typedef struct ast_checked_expr_stmt {
    ASTCheckedStmtNode header;
    ASTCheckedExprNode *expr;
} ASTCheckedExprStmt;

typedef struct ast_checked_defer_stmt {
    ASTCheckedStmtNode header;
    ASTCheckedStmtNode *body;
} ASTCheckedDeferStmt;

/***
 * Print an ASTCheckedStmtNode tree.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astCheckedStmtNodePrint(FILE *to, ASTCheckedStmtNode *n);

/***
 * Create a new ASTCheckedVarDeclStmt node (node type: CHECKED_STMT_VAR_DECL).
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param obj The ASTCheckedObj of the variable to declare.
 * @param initializer (optional) The initializer.
 * @return The node as an ASTCheckedStmtNode.
 ***/
ASTCheckedStmtNode *astNewCheckedVarDeclStmt(Allocator *a, Location loc, ASTCheckedObj *variable, ASTCheckedExprNode *initializer);


/***
 * Create a new ASTCheckedBlockStmt node (node type: CHECKED_STMT_BLOCK).
 * NOTE: Ownership of [nodes] is NOT taken.
 *
 * @param a The Allocator to use to allocate the node.
 * @param loc The Location of the node.
 * @param scope The ScopeID of the block the node represents.
 * @param control_flow The control flow for the block.
 * @param nodes An Array<ASTCheckedStmtNode *> containing the nodes in the block.
 * @return The node as an ASTCheckedStmtNode.
 ***/
ASTCheckedStmtNode *astNewCheckedBlockStmt(Allocator *a, Location loc, ScopeID scope, ControlFlow control_flow, Array *nodes);

/***
 * Create a new ASTCheckedConditionalStmt node (node type: CHECKED_STMT_IF).
 *
 * @param a The Allocator to use to allocate the node.
 * @param loc The Location of the node.
 * @param condition The condition expression.
 * @param then The body (to be executed if the condition evaluates to 'true').
 * @param else_ (optional) The else clause (to be executed if the condition evaluates to 'false').
 * @return The node as an ASTCheckedStmtNode.
 ***/
ASTCheckedStmtNode *astNewCheckedConditionalStmt(Allocator *a, Location loc, ASTCheckedExprNode *condition, ASTCheckedBlockStmt *then, ASTCheckedStmtNode *else_);

/***
 * Create a new ASTCheckedLoopStmt node.
 *
 * @param a The Allocator to use to allocate the node.
 * @param type The stmt node type for the new node.
 * @param loc The Location of the node.
 * @param initializer (optional) The initializer statement.
 * @param condition The condition for the loop.
 * @param increment (optional) The increment expression.
 * @param body The body of the loop.
 * @return The node as an ASTCheckedStmtNode.
 ***/
ASTCheckedStmtNode *astNewCheckedLoopStmt(Allocator *a, ASTCheckedStmtNodeType type, Location loc, ASTCheckedStmtNode *initializer, ASTCheckedExprNode *condition, ASTCheckedExprNode *increment, ASTCheckedBlockStmt *body);

/***
 * Create a new ASTCheckedExprStmt node.
 *
 * @param a The Allocator to use to allocate the node.
 * @param type The stmt node type for the new node
 * @param loc The Location of the node.
 * @param expr The expression.
 * @return The node as an ASTCheckedStmtNode.
 ***/
ASTCheckedStmtNode *astNewCheckedExprStmt(Allocator *a, ASTCheckedStmtNodeType type, Location loc, ASTCheckedExprNode *expr);

/***
 * Create a new ASTCheckedDeferStmt node (node type: CHECKED_STMT_DEFER).
 *
 * @param a The Allocator to use to allocate the node.
 * @param loc The Location of the node.
 * @param body The body of the defer statement.
 * @return The node as an ASTCheckedStmtNode.
 ***/
ASTCheckedStmtNode *astNewCheckedDeferStmt(Allocator *a, Location loc, ASTCheckedStmtNode *body);

#endif // CHECKED_AST_H
