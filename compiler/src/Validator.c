#include <stdarg.h>
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Error.h"
#include "Compiler.h"
#include "Token.h" // Location
#include "Ast.h"
#include "Validator.h"

void validatorInit(Validator *v, Compiler *c) {
    v->compiler = c;
    v->program = NULL;
    v->current_module = 0;
    v->current_function = NULL;
    v->current_scope = NULL;
    v->had_error = false;
}

void validatorFree(Validator *v) {
    v->compiler = NULL;
    v->program = NULL;
    v->current_module = 0;
    v->current_function = NULL;
    v->current_scope = NULL;
    v->had_error = false;
}

static void error(Validator *v, Location loc, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String msg = stringVFormat(format, ap);
    va_end(ap);

    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, true, loc, msg);
    stringFree(msg);

    v->had_error = true;
    compilerAddError(v->compiler, err);
}

// utility macros

#define CHECK(result) ({ if(!(result)) { return false; } })

static inline const char *type_name(Type *ty) {
    if(ty == NULL) {
        return "none";
    }
    return (const char *)ty->name;
}

static bool check_types(Validator *v, Location loc, Type *a, Type *b) {
    if(a != b) {
        error(v, loc, "Type mismatch: expected '%s' but got '%s'.", type_name(a), type_name(b));
        return false;
    }
    return true;
}

static inline void enter_scope(Validator *v, ScopeID scope) {
    VERIFY(v->current_function);
    if(v->current_scope == NULL) {
        // entering function scope.
        v->current_scope = v->current_function->as.fn.scopes;
    } else {
        // entering block scope.
        v->current_scope = blockScopeGetChild(v->current_scope, scope);
    }
}

static inline void leave_scope(Validator *v) {
    v->current_scope = v->current_scope->parent;
}

static ASTObj *find_global_var(Validator *v, ASTString name) {
    for(usize i = 0; i < astProgramGetModule(v->program, v->current_module)->globals.used; ++i) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &astProgramGetModule(v->program, v->current_module)->globals, i);
        VERIFY(g->node_type == ND_ASSIGN || g->node_type == ND_VARIABLE);
        ASTObj *var = AS_OBJ_NODE(g->node_type == ND_ASSIGN ? AS_BINARY_NODE(g)->lhs : g)->obj;
        VERIFY(var->type == OBJ_VAR);
        if(var->as.var.name == name) {
            return var;
        }
    }
    return NULL;
}

static ASTObj *find_local_var(Validator *v, ASTString name) {
    VERIFY(v->current_function && v->current_scope); // depth > 0.
    BlockScope *scope = v->current_scope;
    while(scope) {
        TableItem *i = tableGet(&scope->visible_locals, (void *)name);
        if(i) {
            return (ASTObj *)i->value;
        }
    }
    return NULL;
}

static ASTObj *find_variable(Validator *v, ASTString name) {
    ASTObj *result = NULL;
    // If the scope depth is larger than 0 (meaning we are inside a function),
    // search for a local variable first.
    if((v->current_function && v->current_scope) && (result = find_local_var(v, name)) != NULL) {
        return result;
    }
    // otherwise search for a global variable.
    return find_global_var(v, name);
}

static Type *get_expr_type(Validator *v, ASTNode *expr) {
    Type *ty = NULL;
    switch(expr->node_type) {
        case ND_NUMBER_LITERAL: // The default number literal type is i32.
            ty = v->program->primitives.int32;
            break;
        case ND_IDENTIFIER: {
            ASTObj *var = find_variable(v, AS_IDENTIFIER_NODE(expr)->identifier);
            if(!var) {
                error(v, expr->location, "Variable '%s' not found.", AS_IDENTIFIER_NODE(expr)->identifier);
                break;
            }
            VERIFY(var->type == OBJ_VAR);
            ty = var->as.var.type;
            break;
        }
        case ND_ADD:
            // The type of a binary expression is the type of the left side
            // (e.g. in 'a+b' the type of 'a' is the type of the expression).
            ty = get_expr_type(v, AS_BINARY_NODE(expr)->lhs);
            break;
        default:
            UNREACHABLE();
    }
    return ty;
}

static void variable_validate_callback(void *variable, void *validator) {
    ASTNode *var = AS_NODE(variable);
    Validator *v = (Validator *)validator;

    // If the expression is assignment, verify that the type is set or try to infer it if it isn't.
    if(var->node_type == ND_ASSIGN) {
        ASTObj *var_obj = AS_OBJ_NODE(AS_BINARY_NODE(var)->lhs)->obj;
        VERIFY(var_obj->type == OBJ_VAR);
        if(var_obj->as.var.type == NULL) {
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(var)->rhs);
            if(!rhs_ty) {
                error(v, var->location, "Failed to infer type (Hint: consider adding an explicit type).");
                return;
            }
            var_obj->as.var.type = rhs_ty;
        }
    }
}

static bool validate_ast(Validator *v, ASTNode *n) {
    switch(n->node_type) {
        case ND_BLOCK: {
            // typecheck all nodes in the block, even if some fail.
            bool failed = false;
            enter_scope(v, AS_BLOCK_NODE(n)->scope);
            for(usize i = 0; i < AS_BLOCK_NODE(n)->nodes.used; ++i) {
                failed = validate_ast(v, ARRAY_GET_AS(ASTNode *, &AS_BLOCK_NODE(n)->nodes, i));
            }
            leave_scope(v);
            // failed == false -> success (return true).
            // failed == true -> failure (return false).
            return !failed;
        }
        case ND_ASSIGN: // fallthrough
        case ND_VARIABLE: {
            bool old_had_error = v->had_error;
            variable_validate_callback((void *)n, (void *)v);
            if(old_had_error != v->had_error) {
                return false;
            }
            break;
        }
        // ignored nodes (no typechecking to do).
        case ND_NUMBER_LITERAL:
        case ND_IDENTIFIER:
        case ND_ADD:
            return true;
        default:
            UNREACHABLE();
    }
    return true;
}

static void validate_function(Validator *v, ASTObj *fn) {
    VERIFY(fn->type == OBJ_FN);

    v->current_function = fn;

    for(usize i = 0; i < fn->as.fn.body->nodes.used; ++i) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &fn->as.fn.body->nodes, i);
        validate_ast(v, n);
    }

    v->current_function = NULL;
}

static void validate_object_callback(void *object, void *validator) {
    ASTObj *obj = (ASTObj *)object;
    Validator *v = (Validator *)validator;

    if(obj->type == OBJ_FN) {
        validate_function(v, obj);
    }
}

static void module_validate_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;

    v->current_module = (ModuleID)index;
    arrayMap(&m->globals, variable_validate_callback, validator);
    arrayMap(&m->objects, validate_object_callback, validator);
}

// typechecker forward declarations
static void variable_typecheck_callback(void *variable_node, void *validator);

static bool typecheck_ast(Validator *v, ASTNode *n) {
    switch(n->node_type) {
        case ND_ADD: {
            CHECK(typecheck_ast(v, AS_BINARY_NODE(n)->lhs));
            CHECK(typecheck_ast(v, AS_BINARY_NODE(n)->rhs));
            Type *lhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->lhs);
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->rhs);
            CHECK(check_types(v, n->location, lhs_ty, rhs_ty));
            break;
        }
        case ND_BLOCK: {
            // typecheck all nodes in the block, even if some fail.
            bool failed = false;
            enter_scope(v, AS_BLOCK_NODE(n)->scope);
            for(usize i = 0; i < AS_BLOCK_NODE(n)->nodes.used; ++i) {
                failed = typecheck_ast(v, ARRAY_GET_AS(ASTNode *, &AS_BLOCK_NODE(n)->nodes, i));
            }
            leave_scope(v);
            // failed == false -> success (return true).
            // failed == true -> failure (return false).
            return !failed;
        }
        case ND_ASSIGN: // fallthrough
        case ND_VARIABLE: {
            bool old_had_error = v->had_error;
            variable_typecheck_callback((void *)n, (void *)v);
            if(old_had_error != v->had_error) {
                return false;
            }
            break;
        }
        // ignored nodes (no typechecking to do).
        case ND_NUMBER_LITERAL:
        case ND_IDENTIFIER:
            return true;
        default:
            UNREACHABLE();
    }
    return true;
}

static void variable_typecheck_callback(void *variable_node, void *validator) {
    ASTNode *var = AS_NODE(variable_node);
    Validator *v = (Validator *)validator;

    switch(var->node_type) {
        case ND_VARIABLE:
            if(AS_OBJ_NODE(var)->obj->as.var.type == NULL) {
                error(v, var->location, "Variable '%s' has no type (Hint: consider adding an explicit type).", AS_OBJ_NODE(var)->obj->as.var.name);
                return;
            }
            break;
        case ND_ASSIGN: {
            typecheck_ast(v, AS_BINARY_NODE(var)->rhs);
            Type *lhs_ty = AS_OBJ_NODE(AS_BINARY_NODE(var)->lhs)->obj->as.var.type;
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(var)->rhs);
            // allow assigning number literals to u32 variables.
            if(IS_UNSIGNED(*lhs_ty)
                && IS_SIGNED(*rhs_ty)
                && NODE_IS(AS_BINARY_NODE(var)->rhs, ND_NUMBER_LITERAL)) {
                    break;
            }
            check_types(v, var->location, lhs_ty, rhs_ty);
            break;
        }
        default:
            UNREACHABLE();
    }
}

static void typecheck_function(Validator *v, ASTObj *fn) {
    VERIFY(fn->type == OBJ_FN);

    v->current_function = fn;

    // TODO: check return type once return stmt is implemented.
    for(usize i = 0; i < fn->as.fn.body->nodes.used; ++i) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &fn->as.fn.body->nodes, i);
        typecheck_ast(v, n);
    }

    v->current_function = NULL;
}

static void typecheck_object_callback(void *object, void *validator) {
    ASTObj *obj = (ASTObj *)object;
    Validator *v = (Validator *)validator;

    if(obj->type == OBJ_FN) {
        typecheck_function(v, obj); // We don't care if the typecheck failed because we want to check all functions.
    }
}

static void module_typecheck_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;

    v->current_module = (ModuleID)index;
    arrayMap(&m->globals, variable_typecheck_callback, validator);
    arrayMap(&m->objects, typecheck_object_callback, validator);
}


#undef CHECK

bool validatorValidate(Validator *v, ASTProgram *prog) {
    v->program = prog;

    // Validating pass - finish building the AST.
    arrayMapIndex(&v->program->modules, module_validate_callback, (void *)v);

    if(!v->had_error) {
        // Typechecking pass - typecheck the AST.
        arrayMapIndex(&v->program->modules, module_typecheck_callback, (void *)v);
    }


    v->program = NULL;
    return !v->had_error;
}
