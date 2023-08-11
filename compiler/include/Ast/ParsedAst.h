#ifndef PARSED_AST_H
#define PARSED_AST_H

#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Strings.h"
#include "Table.h"
#include "Array.h"
#include "Arena.h"
#include "Ast/ast_common.h"


/* pre-declarations */

// Can't include Types/ParsedType.h because it includes this file.
typedef struct parsed_type ParsedType; // from Types/ParsedType.h

typedef struct ast_parsed_obj ASTParsedObj;
typedef struct ast_parsed_block_stmt ASTParsedBlockStmt;


/* ParsedScope */

typedef struct parsed_scope {
    bool is_block_scope;
    Array objects; // Array<ASTParsedObj *> - owns the objects stored in all the tables below.
    Table variables; // Table<ASTInternedString, ASTParsedObj *>
    Table functions; // Table<ASTInternedString, ASTParsedObj *>
    Table structures; // Table<ASTInternedString, ASTParsedObj *>
    //Table enums; // Table<ASTInternedString, ASTParsedObj *>
    Table types; // Table<ParsedType *, void>
    ScopeID parent;
    struct { // The Array type cannot be used because ScopeID is not used as a pointer type.
        ScopeID *children_scope_ids;
        usize length;
        usize capacity;
    } children;
} ParsedScope;

/***
 * Create a new ParsedScope.
 *
 * @param parent_scope The ScopeID of the parent scope.
 * @param is_block_scope Set to true if the scope will be a block scope.
 * @return The new scope.
 ***/
ParsedScope *parsedScopeNew(ScopeID parent_scope, bool is_block_scope);

/***
 * Add a child to a ParsedScope.
 *
 * @param parent The parent scope.
 * @param child_id The ScopeID of the child scope to add.
 ***/
void parsedScopeAddChild(ParsedScope *parent, ScopeID child_id);

/***
 * Intern a ParsedType saving it in scope's type table.
 * NOTE: 'type' MUST be heap allocated! Ownership of it is taken!
 *
 * @param scope The scope to save the type in.
 * @param type A heap allocated ParsedType to intern.
 ****/
ParsedType *parsedScopeAddType(ParsedScope *scope, ParsedType *type);

/***
 * Free a list of ParsedScope's.
 *
 * @param scope_list The head of the scope list.
 ***/
void parsedScopeFree(ParsedScope *scope_list);


/***
 * Print a ParsedScope to stream [stream].
 * NOTE: all child scopes are printed recursively as well.
 *
 * @param to The stream to print to.
 * @param scope The scope to print.
 ***/
void parsedScopePrint(FILE *to, ParsedScope *scope);


/* ASTParsedObj */

// An ASTParsedObj holds an object which in this context
// means functions, variables, structs, enums etc.
typedef struct ast_parsed_obj {
    ASTObjType type;
    Location location;
    ASTString name;
    ParsedType *data_type;

    union {
        //struct {
        //  bool is_const;
        //  bool is_parameter;
        //  bool is_struct_field;
        //} var; // OBJ_VAR
        struct {
            Array parameters; // Array<ASTParsedObj *> (OBJ_VAR)
            ParsedType *return_type;
            ASTParsedBlockStmt *body; // Note: contains the function's scope ScopeID.
        } fn; // OBJ_FN
        struct {
            ScopeID scope;
        } structure; // OBJ_STRUCT
        struct {
            Array parameters; // Array<ASTParsedObj *>
            ParsedType *return_type;
            Attribute *source_attr; // After validating, guaranteed to be an ATTR_SOURCE.
        } extern_fn; // OBJ_EXTERN_FN
    } as;
} ASTParsedObj;

/***
 * Create a new ASTParsedObj.
 *
 * @param type The type of the object.
 * @param loc The location of the object.
 * @param name_loc The location of the name of the object.
 * @param name The name of the object.
 * @param data_type The data type of the object.
 * @return The new object.
 ***/
ASTParsedObj *astNewParsedObj(ASTObjType type, Location loc, ASTString name, ParsedType *data_type);

/***
 * Free an ASTParsedObj.
 *
 * @param obj The object to free.
 ***/
void astFreeParsedObj(ASTParsedObj *obj);

/***
 * Print an ASTParsedObj.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 ***/
void astPrintParsedObj(FILE *to, ASTParsedObj *obj);

/***
 * Print an ASTParsedObj in a compact format.
 *
 * @param to The stream to print to.
 * @param obj The object to print.
 ***/
void astParsedObjPrintCompact(FILE *to, ASTParsedObj *obj);


/* ASTParsedModule */

// An ASTParsedModule holds all of a modules data
// which includes functions, structs, enums, global variables, types etc.
typedef struct ast_parsed_module {
    ModuleID id;
    ASTString name; // FIXME: Root module name has no location.
    struct {
        Arena storage;
        Allocator alloc;
    } ast_allocator;
    Array scopes; // Array<ParsedScope *>
    ParsedScope *module_scope; // Owned by the above array.
    Array globals; // Array<ASTParsedVarDeclStmt *> (ND_VAR_DECL)
} ASTParsedModule;

/***
 * Create a new ASTParsedModule.
 *
 * @param name The ASTString containing the module's name.
 * @return A new module.
 ***/
ASTParsedModule *astNewParsedModule(ASTString name);

/***
 * Free an ASTParsedModule.
 *
 * @param module The module to free.
 ***/
void astFreeParsedModule(ASTParsedModule *module);

/***
 * Print an ASTParsedModule.
 *
 * @param to The stream to print to.
 * @param module The module to print.
 ****/
void astPrintParsedModule(FILE *to, ASTParsedModule *module);

/***
 * Add a scope to an ASTParsedModule.
 *
 * @param module The module to add the scope to.
 * @param scope The scope to add.
 * @return The ScopeID of the added scope.
 ***/
ScopeID astParsedModuleAddScope(ASTParsedModule *module, ParsedScope *scope);

/***
 * Get a ParsedScope stored in an ASTParsedModule.
 * NOTE: This function will panic if [id] is invalid.
 *
 * @param module The module containing the scope.
 * @param id The ScopeID that refers to the wanted scope.
 * @return A pointer to the ParsedScope refered to by [id].
 ***/
ParsedScope *astParsedModuleGetScope(ASTParsedModule *module, ScopeID id);

/***
 * Return the ScopeID of [module]'s scope.
 *
 * @param module An ASTParsedModule.
 * @return The ScopeID of [module]'s scope.
 ***/
ScopeID astParsedModuleGetModuleScopeID(ASTParsedModule *module);


/* ASTParsedProgram */

// An ASTParsedProgram holds all the information
// belonging to a program like functions & types.
typedef struct ast_parsed_program {
    struct {
        // Primitive types (are owned by the root module).
        // Note: astParsedProgramInit() has to be updated when adding new primitives.
        ParsedType *void_;
        ParsedType *int32;
        ParsedType *uint32;
        ParsedType *str;
    } primitives;
    // Holds a single copy of each string in the entire program.
    ASTStringTable strings;
    // Holds all modules in a program.
    Array modules; // Array<ASTParsedModule *>
} ASTParsedProgram;

/***
 * Initialize an ASTParsedProgram.
 *
 * @param prog The program to initialize.
 ***/
void astParsedProgramInit(ASTParsedProgram *prog);

/***
 * Free an ASTParsedProgram.
 *
 * @param prog The program to free.
 ***/
void astParsedProgramFree(ASTParsedProgram *prog);

/***
 * Print an ASTParsedProgram.
 *
 * @param to The stream to print to.
 * @param prog The program to print.
 ***/
void astParsedProgramPrint(FILE *to, ASTParsedProgram *prog);

/***
 * Intern a string.
 * NOTE: Ownership of [str] is NOT taken.
 *
 * @param prog The ASTParsedProgram to save the string in.
 * @param str The string to intern.
 ***/
static inline ASTInternedString astParsedProgramAddString(ASTParsedProgram *prog, char *str) {
    return astStringTableAddString(&prog->strings, str);
}

/***
 * Add an ASTParsedModule to an ASTParsedProgram and return its id.
 * NOTE: ownership of 'module' is taken.
 *
 * @param prog The ASTParsedProgram to add the module to.
 * @param module The module to add (ownership of it is taken).
 * @return The ModuleID of the added module.
 ***/
ModuleID astParsedProgramAddModule(ASTParsedProgram *prog, ASTParsedModule *module);

/***
 * Get an ASTParsedModule using its ModuleID from an ASTParsedProgram.
 * NOTE: This function will panic if [id] is invalid.
 *
 * @param prog The ASTParsedProgram to get the module from.
 * @param id The ModuleID of the module to get.
 * @return A pointer to the module or NULL if the id is out of bounds.
 ***/
ASTParsedModule *astParsedProgramGetModule(ASTParsedProgram *prog, ModuleID id);


/* AST nodes - ASTParsedExprNode */

typedef enum ast_parsed_expression_node_type {
    // Constant value nodes
    PARSED_EXPR_NUMBER_CONSTANT,
    PARSED_EXPR_STRING_CONSTANT,

    // Obj nodes
    PARSED_EXPR_VARIABLE,
    PARSED_EXPR_FUNCTION,

    // Binary nodes
    PARSED_EXPR_ASSIGN,
    PARSED_EXPR_PROPERTY_ACCESS,
    PARSED_EXPR_ADD,
    PARSED_EXPR_SUBTRACT,
    PARSED_EXPR_MULTIPLY,
    PARSED_EXPR_DIVIDE,
    PARSED_EXPR_EQ, PARSED_EXPR_NE,
    PARSED_EXPR_LT, PARSED_EXPR_LE,
    PARSED_EXPR_GT, PARSED_EXPR_GE,

    // Unary nodes
    PARSED_EXPR_NEGATE,
    PARSED_EXPR_ADDROF, // For pointers: &<obj>
    PARSED_EXPR_DEREF, // Pointer dereference: *<obj>

    // Call node
    PARSED_EXPR_CALL,

    // Other nodes
    PARSED_EXPR_IDENTIFIER,

    __PARSED_EXPR_TYPE_COUNT
} ASTParsedExprNodeType;

// The base expression node type.
typedef struct ast_parsed_expression_node {
    ASTParsedExprNodeType type;
    Location location;
    ParsedType *data_type; // The data type the expression evaluates to.
} ASTParsedExprNode;

// Stores a constant value using the 'Value' container type.
typedef struct ast_parsed_constant_value_expr {
    ASTParsedExprNode header;
    Value value;
} ASTParsedConstantValueExpr;

// Stores a pointer to an ASTParsedObj.
typedef struct ast_parsed_obj_expr {
    ASTParsedExprNode header;
    ASTParsedObj *obj;
} ASTParsedObjExpr;

// Used for unary expressions (such as -<lval/rval>, &<lval>, *<lval> etc.).
typedef struct ast_parsed_unary_expr {
    ASTParsedExprNode header;
    ASTParsedExprNode *operand;
} ASTParsedUnaryExpr;

// Used for binary expressions (such as <lval/rval> + <lval/rval> etc.)
typedef struct ast_parsed_binary_expr {
    ASTParsedExprNode header;
    ASTParsedExprNode *lhs;
    ASTParsedExprNode *rhs;
} ASTParsedBinaryExpr;

typedef struct ast_parsed_call_expr {
    ASTParsedExprNode header;
    ASTParsedExprNode *callee;
    Array arguments; // Array<ASTParsedExprNode *>
} ASTParsedCallExpr;

typedef struct ast_parsed_identifier_expr {
    ASTParsedExprNode header;
    ASTString id;
} ASTParsedIdentifierExpr;


/***
 * Print an ASTParsedExprNode tree.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astParsedExprNodePrint(FILE *to, ASTParsedExprNode *n);

/***
 * Create a new ASTParsedConstantValueExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param value The Value to store in the node.
 * @param ty (optional) The type of the value.
 * @return The node as an ASTParsedExprNode.
 ***/
ASTParsedExprNode *astNewParsedConstantValueExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, Value value, ParsedType *value_ty);

/***
 * Create a new ASTParsedObjExpr node.
 * NOTE: Ownership of 'obj' is NOT taken.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param obj The ASTParsedObj to store in the node.
 * @return The node as an ASTParsedExprNode.
 ***/
ASTParsedExprNode *astNewParsedObjExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, ASTParsedObj *obj);


// TODO: unary+binary expr: add type hint?
/***
 * Create a new ASTParsedUnaryExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param operand The operand.
 * @return The node as an ASTParsedExprNode.
 ***/
ASTParsedExprNode *astNewParsedUnaryExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, ASTParsedExprNode *operand);

/***
 * Create a new ASTParsedBinaryExpr node.
 *
 * @param a The allocator to use.
 * @param type The expr node type.
 * @param loc The Location of the node.
 * @param lhs The left child.
 * @param rhs The right child.
 * @return The node as an ASTParsedExprNode.
 ***/
ASTParsedExprNode *astNewParsedBinaryExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, ASTParsedExprNode *lhs, ASTParsedExprNode *rhs);

/***
 * Create a new ASTParsedCallExpr node.
 * NOTE: Ownership of [arguments] is NOT taken.
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param callee The callee.
 * @param arguments An Array<ASTParsedExprNode *> containing the arguments.
 ***/
ASTParsedExprNode *astNewParsedCallExpr(Allocator *a, Location loc, ASTParsedExprNode *callee, Array *arguments);


/***
 * Create a new ASTParsedIdentifierExpr node (node type: PARSED_EXPR_IDENTIFIER).
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param id An ASTString containing the identifier.
 * @return The node as an ASTParsedExprNode.
 ***/
ASTParsedExprNode *astNewParsedIdentifierExpr(Allocator *a, Location loc, ASTString id);


/* AST node - ASTParsedStmtNode */

typedef enum ast_parsed_stmt_node_type {
    // VarDecl node
    PARSED_STMT_VAR_DECL,

    // Block node
    PARSED_STMT_BLOCK,

    // Conditional node
    PARSED_STMT_IF,

    // Loop node
    PARSED_STMT_WHILE_LOOP,

    // Expr nodes
    PARSED_STMT_RETURN,
    PARSED_STMT_DEFER,
    PARSED_STMT_EXPR,

    __PARSED_STMT_TYPE_COUNT
} ASTParsedStmtNodeType;

typedef struct ast_parsed_stmt_node {
    ASTParsedStmtNodeType type;
    Location location;
} ASTParsedStmtNode;

typedef struct ast_parsed_var_decl_stmt {
    ASTParsedStmtNode header;
    ASTParsedObj *variable;
    ASTParsedExprNode *initializer; // optional
} ASTParsedVarDeclStmt;

typedef struct ast_parsed_block_stmt {
    ASTParsedStmtNode header;
    ScopeID scope;
    ControlFlow control_flow;
    Array nodes; // Array<AstParsedStmtNode *>
} ASTParsedBlockStmt;

typedef struct ast_parsed_conditional_stmt {
    ASTParsedStmtNode header;
    ASTParsedExprNode *condition;
    ASTParsedBlockStmt *then;
    ASTParsedStmtNode *else_;
} ASTParsedConditionalStmt;

typedef struct ast_parsed_loop_stmt {
    ASTParsedStmtNode header;
    ASTParsedStmtNode *initializer;
    ASTParsedExprNode *condition;
    ASTParsedExprNode *increment;
    ASTParsedBlockStmt *body;
} ASTParsedLoopStmt;

typedef struct ast_parsed_expr_stmt {
    ASTParsedStmtNode header;
    ASTParsedExprNode *expr;
} ASTParsedExprStmt;

/***
 * Print an ASTParsedStmtNode tree.
 *
 * @param to The stream to print to.
 * @param n The root node of the tree.
 ***/
void astParsedStmtNodePrint(FILE *to, ASTParsedStmtNode *n);

/***
 * Create a new ASTParsedVarDeclStmt node (node type: PARSED_STMT_VAR_DECL).
 *
 * @param a The allocator to use.
 * @param loc The Location of the node.
 * @param obj The ASTParsedObj of the variable to declare.
 * @param initializer (optional) The initializer.
 * @return The node as an ASTParsedStmtNode.
 ***/
ASTParsedStmtNode *astNewParsedVarDeclStmt(Allocator *a, Location loc, ASTParsedObj *variable, ASTParsedExprNode *initializer);


/***
 * Create a new ASTParsedBlockStmt node (node type: PARSED_STMT_BLOCK).
 * NOTE: Ownership of [nodes] is NOT taken.
 *
 * @param a The Allocator to use to allocate the node.
 * @param loc The Location of the node.
 * @param scope The ScopeID of the block the node represents.
 * @param control_flow The control flow for the block.
 * @param nodes An Array<ASTParsedStmtNode *> containing the nodes in the block.
 * @return The node as an ASTParsedStmtNode.
 ***/
ASTParsedStmtNode *astNewParsedBlockStmt(Allocator *a, Location loc, ScopeID scope, ControlFlow control_flow, Array *nodes);

/***
 * Create a new ASTParsedConditionalStmt node (node type: PARSED_STMT_IF).
 *
 * @param a The Allocator to use to allocate the node.
 * @param loc The Location of the node.
 * @param condition The condition expression.
 * @param then The body (to be executed if the condition evaluates to 'true').
 * @param else_ (optional) The else clause (to be executed if the condition evaluates to 'false').
 * @return The node as an ASTParsedStmtNode.
 ***/
ASTParsedStmtNode *astNewParsedConditionalStmt(Allocator *a, Location loc, ASTParsedExprNode *condition, ASTParsedBlockStmt *then, ASTParsedStmtNode *else_);

/***
 * Create a new ASTParsedLoopStmt node.
 *
 * @param a The Allocator to use to allocate the node.
 * @param type The stmt node type for the new node.
 * @param loc The Location of the node.
 * @param initializer (optional) The initializer statement.
 * @param condition The condition for the loop.
 * @param increment (optional) The increment expression.
 * @param body The body of the loop.
 * @return The node as an ASTParsedStmtNode.
 ***/
ASTParsedStmtNode *astNewParsedLoopStmt(Allocator *a, ASTParsedStmtNodeType type, Location loc, ASTParsedStmtNode *initializer, ASTParsedExprNode *condition, ASTParsedExprNode *increment, ASTParsedBlockStmt *body);

/***
 * Create a new ASTParsedExprStmt node.
 *
 * @param a The Allocator to use to allocate the node.
 * @param type The stmt node type for the new node
 * @param loc The Location of the node.
 * @param expr The expression.
 * @return The node as an ASTParsedStmtNode.
 ***/
ASTParsedStmtNode *astNewParsedExprStmt(Allocator *a, ASTParsedStmtNodeType type, Location loc, ASTParsedExprNode *expr);

#endif // PARSED_AST_H
