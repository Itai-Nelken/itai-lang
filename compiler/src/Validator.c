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
    v->had_error = false;
}

void validatorFree(Validator *v) {
    v->compiler = NULL;
    v->program = NULL;
    v->current_module = 0;
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
        error(v, loc, "Type mismatch (Expected '%s' but got '%s').", type_name(a), type_name(b));
        return false;
    }
    return true;
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

static Type *get_expr_type(Validator *v, ASTNode *expr) {
    Type *ty = NULL;
    switch(expr->node_type) {
        case ND_NUMBER_LITERAL: // The default number literal type is i32.
            ty = v->program->primitives.int32;
            break;
        case ND_IDENTIFIER: {
            // TODO: when local variables are added, check for them first.
            ASTObj *var = find_global_var(v, AS_IDENTIFIER_NODE(expr)->identifier);
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

static void global_variable_validate_callback(void *global, void *validator) {
    ASTNode *g = AS_NODE(global);
    Validator *v = (Validator *)validator;

    // If the expression is assignment, verify that the type is set or try to infer it if it isn't.
    if(g->node_type == ND_ASSIGN) {
        ASTObj *var = AS_OBJ_NODE(AS_BINARY_NODE(g)->lhs)->obj;
        VERIFY(var->type == OBJ_VAR);
        if(var->as.var.type == NULL) {
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(g)->rhs);
            if(!rhs_ty) {
                error(v, var->location, "Failed to infer type (Hint: consider adding an explicit type).");
                return;
            }
            var->as.var.type = rhs_ty;
        }
    }
}

static void module_validate_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;

    v->current_module = (ModuleID)index;
    arrayMap(&m->globals, global_variable_validate_callback, validator);
}


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
        // ignored nodes (no typechecking to do).
        case ND_NUMBER_LITERAL:
        case ND_VARIABLE:
        case ND_ASSIGN:
        case ND_IDENTIFIER:
            return true;
        default:
            UNREACHABLE();
    }
    return true;
}

static void global_variable_typecheck_callback(void *global, void *validator) {
    ASTNode *g = AS_NODE(global);
    Validator *v = (Validator *)validator;

    switch(g->node_type) {
        case ND_VARIABLE:
            if(AS_OBJ_NODE(g)->obj->as.var.type == NULL) {
                error(v, g->location, "Variable '%s' has no type (Hint: consider adding an explicit type).", AS_OBJ_NODE(g)->obj->as.var.name);
                return;
            }
            break;
        case ND_ASSIGN: {
            typecheck_ast(v, AS_BINARY_NODE(g)->rhs);
            Type *lhs_ty = AS_OBJ_NODE(AS_BINARY_NODE(g)->lhs)->obj->as.var.type;
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(g)->rhs);
            // allow assigning number literals to u32 variables.
            if(IS_UNSIGNED(*lhs_ty)
                && IS_SIGNED(*rhs_ty)
                && AS_BINARY_NODE(g)->rhs->node_type == ND_NUMBER_LITERAL) {
                    break;
            }
            check_types(v, g->location, lhs_ty, rhs_ty);
            break;
        }
        default:
            UNREACHABLE();
    }
}

static void module_typecheck_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;

    v->current_module = (ModuleID)index;
    arrayMap(&m->globals, global_variable_typecheck_callback, validator);
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
