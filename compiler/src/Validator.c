#include <string.h>
#include "common.h"
#include "memory.h"
#include "Ast/ParsedAst.h"
#include "Ast/CheckedAst.h"
#include "Types/ParsedType.h"
#include "Types/CheckedType.h"
#include "Array.h"
#include "Table.h"
#include "Strings.h"
#include "Compiler.h"
#include "Error.h"
#include "Validator.h"



void validatorInit(Validator *v, Compiler *c) {
    v->compiler = c;
    v->current.allocator = NULL;
    v->current.module = EMPTY_MODULE_ID;
    v->current.scope = EMPTY_SCOPE_ID;
    v->had_error = false;
    v->parsed_prog = NULL;
    v->checked_prog = NULL;
}

void validatorFree(Validator *v) {
    memset(v, 0, sizeof(*v));
}

// Utility functions & macros

#define TRY(type, result) ({ \
 type _tmp = (result);\
 if(!_tmp) { \
    return NULL; \
 } \
_tmp;})

#define FOR(index_var_name, array) for(usize index_var_name = 0; index_var_name < (array).used; ++index_var_name)

// Note: [msg] is NOT freed.
static void __add_error_to_compiler(Validator *v, Location loc, ErrorType type, String msg) {
    Error *err;
    NEW0(err);
    errorInit(err, type, true, loc, msg);

    v->had_error = true;
    compilerAddError(v->compiler, err);
}

static void error(Validator *v, Location loc, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String msg = stringVFormat(format, ap);
    va_end(ap);

    __add_error_to_compiler(v, loc, ERR_ERROR, msg);
    stringFree(msg);
}

static void hint(Validator *v, Location loc, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String msg = stringVFormat(format, ap);
    va_end(ap);

    __add_error_to_compiler(v, loc, ERR_HINT, msg);
    stringFree(msg);
}

/***
 * @return Pointer to current checked module
 ***/
static inline ASTCheckedModule *get_current_module(Validator *v) {
    return astCheckedProgramGetModule(v->checked_prog, v->current.module);
}

// TODO: Maybe return new scope id?
//       * Maybe recieve scope to enter as parameter?
static void enter_scope(Validator *v, bool is_block_scope) {
    ScopeID current_module_scope = astCheckedModuleGetModuleScopeID(get_current_module(v));
    if(scopeIDCompare(v->current.scope, current_module_scope)) {
        v->current.scope = current_module_scope;
        return;
    }
    CheckedScope *new_scope = checkedScopeNew(v->current.scope, is_block_scope);
    ScopeID new_scope_id = astCheckedModuleAddScope(get_current_module(v), new_scope);
    v->current.scope = new_scope_id;
}

// TODO: Maybe return new scope id?
static void leave_scope(Validator *v) {
    CheckedScope *current_scope = astCheckedModuleGetScope(get_current_module(v), v->current.scope);
    v->current.scope = current_scope->parent;
}

static bool is_const_expr(ASTParsedExprNode *n) {
    switch(n->type) {
        case PARSED_EXPR_NUMBER_CONSTANT:
        case PARSED_EXPR_STRING_CONSTANT:
            return true;
        case PARSED_EXPR_ADD:
        case PARSED_EXPR_SUBTRACT:
        case PARSED_EXPR_MULTIPLY:
        case PARSED_EXPR_DIVIDE:
        case PARSED_EXPR_EQ:
        case PARSED_EXPR_LT:
        case PARSED_EXPR_GT:
        case PARSED_EXPR_NE:
        case PARSED_EXPR_LE:
        case PARSED_EXPR_GE:
            return is_const_expr(NODE_AS(ASTParsedBinaryExpr, n)->lhs) &&
                   is_const_expr(NODE_AS(ASTParsedBinaryExpr, n)->rhs);
        case PARSED_EXPR_NEGATE:
            return is_const_expr(NODE_AS(ASTParsedUnaryExpr, n)->operand);
        default:
            return false;
    }
}

static CheckedType *make_checked_ptr_type(Validator *v, CheckedType *inner_type) {
    CheckedType *ty = NULL;
    NEW0(ty);
    String formatted_name = stringFormat("&%s", inner_type->name.data);
    ASTString name = {
        .data = astStringTableAddString(&v->checked_prog->strings, formatted_name),
        .location = EMPTY_LOCATION
    };
    stringFree(formatted_name);
    checkedTypeInit(ty, TY_PTR, name, v->current.module);
    return checkedScopeAddType(astCheckedModuleGetScope(get_current_module(v), v->current.scope), ty);
}

static CheckedType *get_expr_data_type(Validator *v, ASTCheckedExprNode *node) {
    CheckedType *ty = NULL;
    switch(node->type) {
        case CHECKED_EXPR_NUMBER_CONSTANT:
            if(node->data_type != NULL) {
                ty = node->data_type;
            } else {
                // The default type for number literals is i32.
                ty = v->checked_prog->primitives.int32;
            }
            break;
        case CHECKED_EXPR_STRING_CONSTANT:
            ty = v->checked_prog->primitives.str;
            break;
        case CHECKED_EXPR_VARIABLE:
        case CHECKED_EXPR_FUNCTION:
            ty = NODE_AS(ASTCheckedObjExpr, node)->obj->data_type;
            break;
        case CHECKED_EXPR_PROPERTY_ACCESS:
            // FIXME: The following correct code will fail here: returnsObj().field
            VERIFY(NODE_IS(node, CHECKED_EXPR_VARIABLE));
            // The type of a property access expression is the right-most
            // node. For example: the type of a.b.c is the type of c.
            ty = get_expr_data_type(v, NODE_AS(ASTCheckedBinaryExpr, node)->rhs);
            break;
        case CHECKED_EXPR_CALL:
            ty = get_expr_data_type(v, NODE_AS(ASTCheckedCallExpr, node)->callee);
            break;
        // Binary/Infix expressions (a [operator] b)
        case CHECKED_EXPR_ASSIGN:
        case CHECKED_EXPR_ADD:
        case CHECKED_EXPR_SUBTRACT:
        case CHECKED_EXPR_MULTIPLY:
        case CHECKED_EXPR_DIVIDE:
        case CHECKED_EXPR_EQ:
        case CHECKED_EXPR_NE:
        case CHECKED_EXPR_LT:
        case CHECKED_EXPR_LE:
        case CHECKED_EXPR_GT:
        case CHECKED_EXPR_GE:
            // The type of a binary expression is the type of the left side.
            // For example: the type of the expression a + b is the type of a.
            // The typechecker will make sure that b can be added to a.
            ty = get_expr_data_type(v, NODE_AS(ASTCheckedBinaryExpr, node)->lhs);
            break;
        // Unary/Prefix expressions ([operator] a)
        case CHECKED_EXPR_NEGATE:
        case CHECKED_EXPR_DEREF:
            ty = get_expr_data_type(v, NODE_AS(ASTCheckedUnaryExpr, node)->operand);
            if(NODE_IS(node, CHECKED_EXPR_DEREF) && ty->type == TY_PTR) {
                ty = ty->as.ptr.inner_type;
            }
            break;
        case CHECKED_EXPR_ADDROF:
            ty = make_checked_ptr_type(v, get_expr_data_type(v, NODE_AS(ASTCheckedUnaryExpr, node)->operand));
            break;
        default:
            UNREACHABLE();
    }
    return ty;
}

// Validator

// TODO/FIXME:
// * Validator - Rebuild AST (Parsed -> Checked), make sure AST is valid (so no null checks etc. needed).
// * Typechecker - Check the AST for problems with the LANGUAGE (and not the ast itself as that is the validator's job).

/***
 * @return Pointer to validated CheckedType
 ***/
static CheckedType *validate_data_type(Validator *v, ParsedType *parsed_type) {
    CheckedScope *sc = astCheckedModuleGetScope(get_current_module(v), v->current.scope);

    // Otherwise validate it.
    CheckedType *ty = NULL;
    bool is_primitive = false;
    switch(parsed_type->type) {
        // Primitives - simply create a CheckedType with the same data.
        case TY_VOID:
            ty = v->checked_prog->primitives.void_;
            is_primitive = true;
            break;
        case TY_I32:
            ty = v->checked_prog->primitives.int32;
            is_primitive = true;
            break;
        case TY_U32:
            ty = v->checked_prog->primitives.uint32;
            is_primitive = true;
            break;
        case TY_STR:
            ty = v->checked_prog->primitives.str;
            is_primitive = true;
            break;
        case TY_PTR:
            ty = make_checked_ptr_type(v, validate_data_type(v, parsed_type->as.ptr.inner_type));
            break;
        case TY_FN:
        case TY_STRUCT:
        case TY_ID:
            // TODO
        default:
            UNREACHABLE();
    }
    if(!is_primitive) { // FIXME: I'm too tired to figure out why this if is needed.
        // checkedScopeAddType() frees 'ty' and returns existing type if type already exists
        ty = checkedScopeAddType(sc, ty);
    }
    return ty;
}

static ASTCheckedObj *validate_object(Validator *v, ASTParsedObj *parsed_obj) {
    ASTCheckedObj *checked_obj = astNewCheckedObj(parsed_obj->type, parsed_obj->location, parsed_obj->name, NULL);
    CheckedScope *current_scope = astCheckedModuleGetScope(get_current_module(v), v->current.scope);
    switch(parsed_obj->type) {
        case OBJ_VAR:
            // The type will be NULL if it hasn't been infered yet.
            if(parsed_obj->data_type != NULL) {
                checked_obj->data_type = validate_data_type(v, parsed_obj->data_type);
            }
            // FIXME: We don't check if item is redefined since global vars will already be in the table
            tableSet(&current_scope->variables, (void *)checked_obj->name.data, (void *)checked_obj);
            break;
        case OBJ_FN:
            // validate_fn();
            // tableSet(scope.fns, checked_obj)
        case OBJ_STRUCT:
        case OBJ_EXTERN_FN:
        default:
            UNREACHABLE();
    }
    arrayPush(&current_scope->objects, (void *)checked_obj);
    return checked_obj;
}

static ASTCheckedExprNodeType parsed_expr_type_to_checked_type(ASTParsedExprNodeType parsed_type) {
    ASTCheckedExprNodeType checked_type = __CHECKED_EXPR_TYPE_COUNT;
    switch(parsed_type) {
        case PARSED_EXPR_NUMBER_CONSTANT:
            checked_type = CHECKED_EXPR_NUMBER_CONSTANT;
            break;
        case PARSED_EXPR_STRING_CONSTANT:
            checked_type = CHECKED_EXPR_STRING_CONSTANT;
            break;
        case PARSED_EXPR_VARIABLE:
            checked_type = CHECKED_EXPR_VARIABLE;
            break;
        case PARSED_EXPR_FUNCTION:
            checked_type = CHECKED_EXPR_FUNCTION;
            break;
        case PARSED_EXPR_ASSIGN:
            checked_type = CHECKED_EXPR_ASSIGN;
            break;
        case PARSED_EXPR_PROPERTY_ACCESS:
            checked_type = CHECKED_EXPR_PROPERTY_ACCESS;
            break;
        case PARSED_EXPR_ADD:
            checked_type = CHECKED_EXPR_ADD;
            break;
        case PARSED_EXPR_SUBTRACT:
            checked_type = CHECKED_EXPR_SUBTRACT;
            break;
        case PARSED_EXPR_MULTIPLY:
            checked_type = CHECKED_EXPR_MULTIPLY;
            break;
        case PARSED_EXPR_DIVIDE:
            checked_type = CHECKED_EXPR_DIVIDE;
            break;
        case PARSED_EXPR_EQ:
            checked_type = CHECKED_EXPR_EQ;
            break;
        case PARSED_EXPR_NE:
            checked_type = CHECKED_EXPR_NE;
            break;
        case PARSED_EXPR_LT:
            checked_type = CHECKED_EXPR_LT;
            break;
        case PARSED_EXPR_LE:
            checked_type = CHECKED_EXPR_LE;
            break;
        case PARSED_EXPR_GT:
            checked_type = CHECKED_EXPR_GT;
            break;
        case PARSED_EXPR_GE:
            checked_type = CHECKED_EXPR_GE;
            break;
        case PARSED_EXPR_NEGATE:
            checked_type = CHECKED_EXPR_NEGATE;
            break;
        case PARSED_EXPR_ADDROF:
            checked_type = CHECKED_EXPR_ADDROF;
            break;
        case PARSED_EXPR_DEREF:
            checked_type = CHECKED_EXPR_DEREF;
            break;
        case PARSED_EXPR_CALL:
            checked_type = CHECKED_EXPR_CALL;
            break;
        case PARSED_EXPR_IDENTIFIER:
        default:
            UNREACHABLE();
    }
    return checked_type;
}

// TODO: Give all expressions a data type
static ASTCheckedExprNode *validate_expression(Validator *v, ASTParsedExprNode *parsed_expr) {
    ASTCheckedExprNode *node = NULL;
    switch(parsed_expr->type) {
        // Constant value nodes
        case PARSED_EXPR_NUMBER_CONSTANT:
        case PARSED_EXPR_STRING_CONSTANT: {
            node = astNewCheckedConstantValueExpr(v->current.allocator, parsed_expr_type_to_checked_type(parsed_expr->type), parsed_expr->location, NODE_AS(ASTParsedConstantValueExpr, parsed_expr)->value, NULL);
            break;
        }
        // Obj nodes
        case PARSED_EXPR_VARIABLE:
        case PARSED_EXPR_FUNCTION: {
            ASTCheckedObj *checked_obj = validate_object(v, NODE_AS(ASTParsedObjExpr, parsed_expr)->obj);
            node = astNewCheckedObjExpr(v->current.allocator, parsed_expr_type_to_checked_type(parsed_expr->type), parsed_expr->location, checked_obj);
            break;
        }
        // Binary nodes
        case PARSED_EXPR_ASSIGN:
        case PARSED_EXPR_PROPERTY_ACCESS:
        case PARSED_EXPR_ADD:
        case PARSED_EXPR_SUBTRACT:
        case PARSED_EXPR_MULTIPLY:
        case PARSED_EXPR_DIVIDE:
        case PARSED_EXPR_EQ:
        case PARSED_EXPR_NE:
        case PARSED_EXPR_LT:
        case PARSED_EXPR_LE:
        case PARSED_EXPR_GT:
        case PARSED_EXPR_GE: {
            ASTCheckedExprNode *lhs = TRY(ASTCheckedExprNode *,
                                          validate_expression(v,NODE_AS(ASTParsedBinaryExpr, parsed_expr)->lhs));
            ASTCheckedExprNode *rhs = TRY(ASTCheckedExprNode *,
                                          validate_expression(v,NODE_AS(ASTParsedBinaryExpr, parsed_expr)->rhs));
            node = astNewCheckedBinaryExpr(v->current.allocator, parsed_expr_type_to_checked_type(parsed_expr->type), parsed_expr->location, lhs, rhs);
            break;
        }
        // Unary nodes
        case PARSED_EXPR_NEGATE:
        case PARSED_EXPR_ADDROF:
        case PARSED_EXPR_DEREF: {
            ASTCheckedExprNode *operand = TRY(ASTCheckedExprNode *,
                                              validate_expression(v,NODE_AS(ASTParsedUnaryExpr, parsed_expr)->operand));
            node = astNewCheckedUnaryExpr(v->current.allocator, parsed_expr_type_to_checked_type(parsed_expr->type), parsed_expr->location, operand);
            break;
        }
        case PARSED_EXPR_CALL:
        case PARSED_EXPR_IDENTIFIER:
        default:
            UNREACHABLE();
    }

    node->data_type = get_expr_data_type(v, node);
    // All expression nodes must have a data type (of the value they evaluate to).
    VERIFY(node->data_type != NULL);
    return node;
}

static ASTCheckedStmtNode *validate_statement(Validator *v, ASTParsedStmtNode *parsed_stmt) {
    ASTCheckedStmtNode *n = NULL;
    switch(parsed_stmt->type) {
        case CHECKED_STMT_VAR_DECL: {
            ASTParsedVarDeclStmt *decl = NODE_AS(ASTParsedVarDeclStmt, parsed_stmt);
            ASTCheckedObj *obj = validate_object(v, decl->variable);
            ASTCheckedExprNode *initializer = validate_expression(v, decl->initializer);
            obj->data_type = obj->data_type == NULL ? initializer->data_type : obj->data_type;
            n = astNewCheckedVarDeclStmt(v->current.allocator, parsed_stmt->location, obj, initializer);
            break;
        }
        case CHECKED_STMT_BLOCK:
        case CHECKED_STMT_IF:
        case CHECKED_STMT_WHILE_LOOP:
        case CHECKED_STMT_RETURN:
        case CHECKED_STMT_EXPR:
        case CHECKED_STMT_DEFER:
        default:
            UNREACHABLE();
    }
    return n;
}

static void validate_object_cb(void *object, void *validator) {
    Validator *v = (Validator *)validator;
    ASTParsedObj *parsed_obj = (ASTParsedObj *)object;
    VERIFY(validate_object(v, parsed_obj) != NULL);
}

static void validate_scope_cb(void *scope, void *validator) {
    Validator *v = (Validator *)validator;
    ParsedScope *parsed_scope = (ParsedScope *)scope;
    enter_scope(v, parsed_scope->is_block_scope);

    
    arrayMap(&parsed_scope->objects, validate_object_cb, validator);
    // REMEMBER: types

    for(usize i = 0; i < parsed_scope->children.length; ++i) {
        ScopeID *scope_id = &parsed_scope->children.children_scope_ids[i];
        ParsedScope *p = astParsedModuleGetScope(astParsedProgramGetModule(v->parsed_prog, v->current.module), *scope_id);
        validate_scope_cb((void *)p, validator);
    }

    leave_scope(v);
}

static void validate_global_variable_cb(void *global, void *validator) {
    Validator *v = (Validator *)validator;
    ASTParsedStmtNode *var_decl = (ASTParsedStmtNode *)global;
    ASTCheckedVarDeclStmt *checked_decl = NODE_AS(ASTCheckedVarDeclStmt, validate_statement(v, var_decl));
    VERIFY(checked_decl); // TODO: report errors to user? simply skip NULL nodes since errs already reported?
    arrayPush(&get_current_module(v)->globals, (void *)checked_decl);
}

static void add_primitive_types(ASTCheckedProgram *prog, ASTCheckedModule *root_module) {
#define DEF(typename, type, name) {CheckedType *ty; NEW0(ty); checkedTypeInit(ty, (type), AST_STRING(astCheckedProgramAddString(prog, (name)), EMPTY_LOCATION), root_module->id); prog->primitives.typename = checkedScopeAddType(root_module->module_scope, ty);}

    // FIXME: do we need to add the typenames to the string table (because they are string literals)?
    // NOTE: Update IS_PRIMITIVE() in Types.h when adding new primitives.
    DEF(void_, TY_VOID, "void");
    DEF(int32, TY_I32, "i32");
    DEF(uint32, TY_U32, "u32");
    DEF(str, TY_STR, "str");

#undef DEF
}

static void validate_module_cb(void *module, size_t module_index, void *validator) {
    Validator *v = (Validator *)validator;
    ASTParsedModule *parsed_module = (ASTParsedModule *)module;
    ASTCheckedModule *checked_module = astNewCheckedModule(parsed_module->name);
    // TODO: what do I need to copy from parsed module to checked module
    v->current.module = astCheckedProgramAddModule(v->checked_prog, checked_module);
    v->current.allocator = &checked_module->ast_allocator.alloc;
    VERIFY(v->current.module == module_index);
    // Set current scope (so enter_scope() knows if the current scope is the module scopeS).
    v->current.scope = astCheckedModuleGetModuleScopeID(checked_module);

    if(module_index == 0) { // FIXME: Assuming root module is always 0, which is true, but not nice code
        // Populate primitive types
        // TODO: Update primitive types when more are added.
        add_primitive_types(v->checked_prog, checked_module);
    }

    // Validate all global (module scope) variable declarations
    arrayMap(&parsed_module->globals, validate_global_variable_cb, validator);
    arrayMap(&parsed_module->scopes, validate_scope_cb, validator);
}


// TODO:
// * Move StringTable to Compiler (since it doesn't change)
// * Start typechecker in a new file.
bool validatorValidate(Validator *v, ASTCheckedProgram *checked_prog, ASTParsedProgram *parsed_prog) {
    v->checked_prog = checked_prog;
    v->parsed_prog = parsed_prog;
    { // Swap string tables (so clean one in checked_prog is freed, and populated one in parsed_prog is saved)
        ASTStringTable st = parsed_prog->strings;
        parsed_prog->strings = checked_prog->strings;
        checked_prog->strings = st;
    }

    // Validate the modules
    arrayMapIndex(&v->parsed_prog->modules, validate_module_cb, (void *)v);
    return !v->had_error;
}
