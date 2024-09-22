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

// Validator

// TODO/FIXME:
// * Validator - Rebuild AST (Parsed -> Checked), make sure AST is valid (so no null checks etc. needed).
// * Typechecker - Check the AST for problems with the LANGUAGE (and not the ast itself as that is the validator's job).

/***
 * @return Pointer to validated CheckedType
 ***/
static CheckedType *validate_data_type(Validator *v, ParsedType *parsed_type) {
    CheckedScope *sc = astCheckedModuleGetScope(get_current_module(v), v->current.scope);
    // If type has already been validated, return the validated version.
    TableItem *item;
    if((item = tableGet(&sc->types, (void *)parsed_type->name.data)) != NULL) {
        return (CheckedType *)item->key;
    }

    // Otherwise validate it.
    CheckedType *ty;
    NEW0(ty);
    switch(parsed_type->type) {
        // Primitives - simply create a CheckedType with the same data.
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
            checkedTypeInit(ty, parsed_type->type, parsed_type->name, parsed_type->decl_module, parsed_type->decl_location);
            break;
        case TY_PTR:
        case TY_FN:
        case TY_STRUCT:
        case TY_ID:
        default:
            UNREACHABLE();
    }
    checkedScopeAddType(sc, ty);
    return ty;
}

static void validate_object_cb(void *object, void *validator) {
    Validator *v = (Validator *)validator;
    ASTParsedObj *parsed_obj = (ASTParsedObj *)object;
    ASTCheckedObj *checked_obj = astNewCheckedObj(parsed_obj->type, parsed_obj->location, parsed_obj->name, NULL);
    CheckedScope *current_scope = astCheckedModuleGetScope(get_current_module(v), v->current.scope);
    switch(parsed_obj->type) {
        case OBJ_VAR:
            // The type might be NULL if it hasn't been infered yet.
            if(parsed_obj->data_type != NULL) {
                checked_obj->data_type = validate_data_type(v, parsed_obj->data_type);
            }
            VERIFY(tableSet(&current_scope->variables, (void *)checked_obj->name.data, (void *)checked_obj) == NULL);
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

static void validate_module_cb(void *module, size_t module_index, void *validator) {
    Validator *v = (Validator *)validator;
    ASTParsedModule *parsed_module = (ASTParsedModule *)module;
    ASTCheckedModule *checked_module = astNewCheckedModule(parsed_module->name);
    // TODO: what do I need to copy from parsed module to checked module
    v->current.module = astCheckedProgramAddModule(v->checked_prog, checked_module);
    VERIFY(v->current.module == module_index);

    // Set current scope (so enter_scope() knows if the current scope is the module scopeS).
    v->current.scope = astCheckedModuleGetModuleScopeID(checked_module);
    // REMEMBER 'globals' (remove them, see comment over validatorValidate(...))
    arrayMap(&parsed_module->scopes, validate_scope_cb, validator);
}






// TODO:
// * Move StringTable to Compiler (since it doesn't change)
// * Remove primitives from CheckedProgram since they are impossible to populate cleanly & I'm not sure they're useful.
// * Remove Module.globals since they are stored in module_scope.variables
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
