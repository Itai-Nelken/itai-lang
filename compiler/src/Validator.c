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

static inline const char *type_name(Type *ty) {
    if(ty == NULL) {
        return "none";
    }
    return (const char *)ty->name;
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
                error(v, expr->location, "Variable '%s' not found.");
                break;
            }
            VERIFY(var->type == OBJ_VAR);
            ty = var->as.var.type;
            break;
        }
        default:
            UNREACHABLE();
    }
    return ty;
}

static void global_variable_callback(void *global, void *validator) {
    ASTNode *g = AS_NODE(global);
    Validator *v = (Validator *)validator;

    if(g->node_type == ND_VARIABLE) {
        ASTObj *var = AS_OBJ_NODE(g)->obj;
        VERIFY(var->type == OBJ_VAR);
        if(var->as.var.type == NULL) {
            error(v, var->location, "Variable '%s' has no type (Hint: consider adding an explicit type).", var->as.var.name);
            return;
        }
    } else if(g->node_type == ND_ASSIGN) {
        ASTObj *var = AS_OBJ_NODE(AS_BINARY_NODE(g)->lhs)->obj;
        VERIFY(var->type == OBJ_VAR);
        if(var->as.var.type != NULL) {
            Type *rhs_type = get_expr_type(v, AS_BINARY_NODE(g)->rhs);
            if(var->as.var.type != rhs_type) {
                error(v, g->location, "Type mismatch (expected '%s' but got '%s').", type_name(var->as.var.type), type_name(rhs_type));
                return;
            }
        } else {
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(g)->rhs);
            if(!rhs_ty) {
                error(v, var->location, "Failed to infer type (Hint: consider adding an explicit type).");
                return;
            }
            var->as.var.type = rhs_ty;
        }
    }
}

static void module_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;

    v->current_module = (ModuleID)index;
    arrayMap(&m->globals, global_variable_callback, validator);
}

bool validatorValidate(Validator *v, ASTProgram *prog) {
    v->program = prog;

    arrayMapIndex(&v->program->modules, module_callback, (void *)v);

    v->program = NULL;
    return !v->had_error;
}
