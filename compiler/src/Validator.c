#include <stdarg.h>
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Error.h"
#include "Array.h"
#include "Table.h"
#include "Strings.h"
#include "Compiler.h"
#include "Token.h" // Location
#include "Ast.h"
#include "Types.h"
#include "Validator.h"

void validatorInit(Validator *v, Compiler *c) {
    v->compiler = c;
    v->program = NULL;
    v->current_module = 0;
    v->current_function = NULL;
    v->current_scope = NULL;
    v->found_main = false;
    v->had_error = false;
    tableInit(&v->global_ids_in_current_module, NULL, NULL);
}

void validatorFree(Validator *v) {
    v->compiler = NULL;
    v->program = NULL;
    v->current_module = 0;
    v->current_function = NULL;
    v->current_scope = NULL;
    v->found_main = false;
    v->had_error = false;
    tableFree(&v->global_ids_in_current_module);
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

// utility functions and macros

#define FOR(index_var_name, array) for(usize index_var_name = 0; index_var_name < (array).used; ++index_var_name)

#define TRY(expr) ({bool __tmp = (expr); if(!__tmp) { return false; }; __tmp;})

static inline const char *type_name(Type *ty) {
    if(ty == NULL) {
        return "none";
    }
    return (const char *)ty->name;
}

static bool check_types(Validator *v, Location loc, Type *a, Type *b) {
    if(!a || !b) {
        error(v, loc, "Type mismatch: expected '%s' but got '%s'.", type_name(a), type_name(b));
        return false;
    }
    // Equal types might not have equal addresses if they are stored in different modules.
    // The only types that will always be equal if they have equal addresses are primitive types
    // which are stored only in the root module.
    if(IS_PRIMITIVE(a) && IS_PRIMITIVE(b)) {
        if(a != b) {
            error(v, loc, "Type mismatch: expected '%s' but got '%s'.", type_name(a), type_name(b));
            return false;
        }
    } else {
        if(!typeEqual(a, b)) {
            error(v, loc, "Type mismatch: expected '%s' but got '%s'.", type_name(a), type_name(b));
            return false;
        }
    }
    return true;
}

static inline bool is_callable(ASTObj *obj) {
    return obj->data_type->type == TY_FN;
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
    ASTModule *current_module = astProgramGetModule(v->program, v->current_module);
    for(usize i = 0; i < current_module->globals.used; ++i) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &current_module->globals, i);
        VERIFY(g->node_type == ND_ASSIGN || g->node_type == ND_VAR_DECL);
        ASTObj *var = AS_OBJ_NODE(g->node_type == ND_ASSIGN ? AS_BINARY_NODE(g)->lhs : g)->obj;
        VERIFY(var->type == OBJ_VAR);
        if(var->name == name) {
            return var;
        }
    }
    return NULL;
}

static ASTObj *find_local_var(Validator *v, ASTString name) {
    VERIFY(v->current_function); // inside a function.
    BlockScope *scope = v->current_scope ? v->current_scope : v->current_function->as.fn.scopes;
    while(scope) {
        TableItem *i = tableGet(&scope->visible_locals, (void *)name);
        if(i) {
            return (ASTObj *)i->value;
        }
        scope = scope->parent;
    }
    return NULL;
}

static ASTObj *find_variable(Validator *v, ASTString name) {
    ASTObj *result = NULL;
    // If we are inside a function, search for a local variable first.
    if((v->current_function) && (result = find_local_var(v, name)) != NULL) {
        return result;
    }
    // otherwise search for a global variable.
    return find_global_var(v, name);
}

static ASTObj *find_function(Validator *v, ASTString name) {
    ASTModule *current_module = astProgramGetModule(v->program, v->current_module);
    for(usize i = 0; i < current_module->objects.used; ++i) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &current_module->objects, i);
        if(obj->type == OBJ_FN && obj->name == name) {
            return obj;
        }
    }
    return NULL;
}

static ASTObj *find_struct(Validator *v, ASTString name) {
    ASTModule *current_module = astProgramGetModule(v->program, v->current_module);
    for(usize i = 0; i < current_module->objects.used; ++i) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &current_module->objects, i);
        if(obj->type == OBJ_STRUCT && obj->name == name) {
            return obj;
        }
    }
    return NULL;
}

static inline void add_global_id(Validator *v, ASTString id) {
    VERIFY(tableSet(&v->global_ids_in_current_module, (void *)id, NULL) == NULL);
}

static inline bool global_id_exists(Validator *v, ASTString id) {
    return tableGet(&v->global_ids_in_current_module, (void *)id) != NULL;
}


/* forward declarations */
static ASTNode *validate_ast(Validator *v, ASTNode *n);
static bool typecheck_ast(Validator *v, ASTNode *n);
static bool validate_type(Validator *v, Type **ty);


static Type *get_expr_type(Validator *v, ASTNode *expr) {
    Type *ty = NULL;
    switch(expr->node_type) {
        case ND_NUMBER_LITERAL: // The default number literal type is i32.
            ty = v->program->primitives.int32;
            break;
        case ND_VARIABLE:
            ty = AS_OBJ_NODE(expr)->obj->data_type;
            break;
        case ND_FUNCTION:
            ty = AS_OBJ_NODE(expr)->obj->data_type;
            break;
        case ND_PROPERTY_ACCESS:
            VERIFY(NODE_IS(AS_BINARY_NODE(expr)->rhs, ND_VARIABLE));
            ty = AS_OBJ_NODE(AS_BINARY_NODE(expr)->rhs)->obj->data_type;
            break;
        case ND_CALL:
            ty = AS_OBJ_NODE(AS_BINARY_NODE(expr)->lhs)->obj->data_type->as.fn.return_type;
            break;
        // infix expressions (a op b)
        case ND_ASSIGN:
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
            // The type of a binary expression is the type of the left side
            // (e.g. in 'a+b' the type of 'a' is the type of the expression).
            ty = get_expr_type(v, AS_BINARY_NODE(expr)->lhs);
            break;
        // prefix expressions (op a)
        case ND_NEGATE:
            ty = get_expr_type(v, AS_UNARY_NODE(expr)->operand);
            break;
        default:
            UNREACHABLE();
    }
    return ty;
}


/** Validator **/

// NOTE: [decl] is NOT freed.
static ASTNode *validate_variable_declaration(Validator *v, ASTNode *decl) {
    UNUSED(v);
    // TODO: check if variable declared, add as declared/error.
    return astNewObjNode(ND_VAR_DECL, decl->location, AS_OBJ_NODE(decl)->obj);
}

// NOTE: ownership of [n] is taken.
static ASTNode *validate_assignment(Validator *v, ASTNode *n) {
    // 1) Validate [lhs], this will replace any identifier nodes with variable nodes (and check if the variable exists).
    ASTNode *lhs = validate_ast(v, AS_BINARY_NODE(n)->lhs);
    if(!lhs) {
        astNodeFree(AS_BINARY_NODE(n)->rhs, true);
        astNodeFree(n, false);
        return NULL;
    }
    VERIFY(NODE_IS(lhs, ND_VARIABLE) || NODE_IS(lhs, ND_VAR_DECL) || NODE_IS(lhs, ND_PROPERTY_ACCESS));
    // 2) Validate [rhs] - the expression whose value is being assigned to the variable.
    ASTNode *rhs = validate_ast(v, AS_BINARY_NODE(n)->rhs);
    if(!rhs) {
        astNodeFree(lhs, true);
        astNodeFree(n, false);
        return NULL;
    }
    // 3) If [lhs] is a variable declaration
    if(NODE_IS(lhs, ND_VAR_DECL)) {
        ASTObj *var = AS_OBJ_NODE(lhs)->obj;
        // a) Check that the variable isn't being assigned itself.
        if(NODE_IS(rhs, ND_VARIABLE) && AS_OBJ_NODE(rhs)->obj == var) {
            error(v, rhs->location, "Variable '%s' assigned to itself in declaration.", AS_OBJ_NODE(rhs)->obj->name);
        }
        // b) Try to infer its type if necessary.
        if(var->data_type == NULL) {
            Type *rhs_ty = get_expr_type(v, rhs);
            if(!rhs_ty) {
                error(v, n->location, "Failed to infer type (Hint: consider adding an explicit type).");
            } else {
                var->data_type = rhs_ty;
            }
        }
    }
    // 4) Clean up.
    Location loc = n->location;
    astNodeFree(n, false);
    // 5) Construct the validated assignment.
    return astNewBinaryNode(ND_ASSIGN, loc, lhs, rhs);
}

// NOTE: ownership of [n] is taken.
static ASTNode *validate_ast(Validator *v, ASTNode *n) {
    ASTNode *result = NULL;
    switch(n->node_type) {
        case ND_VAR_DECL:
            result = validate_variable_declaration(v, n);
            break;
        case ND_ASSIGN:
            // NOTE: return early because validate_assignment() frees [n].
            return validate_assignment(v, n);
        case ND_BLOCK: {
            ASTListNode *new_n = AS_LIST_NODE(astNewListNode(ND_BLOCK, n->location, AS_LIST_NODE(n)->scope));
            new_n->control_flow = AS_LIST_NODE(n)->control_flow;
            bool had_error = false;
            enter_scope(v, AS_LIST_NODE(n)->scope);
            FOR(i, AS_LIST_NODE(n)->nodes) {
                ASTNode *checked_node = validate_ast(v, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(n)->nodes, i));
                if(checked_node) {
                    arrayPush(&new_n->nodes, (void *)checked_node);
                } else {
                    had_error = true;
                }
            }
            leave_scope(v);
            if(had_error) {
                astNodeFree(AS_NODE(new_n), true);
                break;
            }
            result = AS_NODE(new_n);
            break;
        }
        // unary nodes
        case ND_NEGATE: {
            ASTNode *operand = validate_ast(v, AS_UNARY_NODE(n)->operand);
            if(!operand)
                break;
            result = astNewUnaryNode(n->node_type, n->location, operand);
            break;
        }
        case ND_RETURN:
            if(v->current_function->as.fn.return_type && AS_UNARY_NODE(n)->operand == NULL) {
                error(v, n->location, "'return' with no value in function '%s' returning '%s'.",
                      v->current_function->name, type_name(v->current_function->as.fn.return_type));
                break;
            }
            if(AS_UNARY_NODE(n)->operand) {
                ASTNode *operand = validate_ast(v, AS_UNARY_NODE(n)->operand);
                if(!operand)
                    break;
                result = astNewUnaryNode(n->node_type, n->location, operand);
            }
            break;
        // binary nodes
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE: {
            ASTNode *lhs = validate_ast(v, AS_BINARY_NODE(n)->lhs);
            if(!lhs)
                break;
            ASTNode *rhs = validate_ast(v, AS_BINARY_NODE(n)->rhs);
            if(!rhs) {
                astNodeFree(lhs, true);
                break;
            }
            result = astNewBinaryNode(n->node_type, n->location, lhs, rhs);
            break;
        }
        case ND_IF: {
            ASTNode *condition = validate_ast(v, AS_CONDITIONAL_NODE(n)->condition);
            if(!condition)
                break;
            ASTNode *body = validate_ast(v, AS_CONDITIONAL_NODE(n)->body);
            if(!body) {
                astNodeFree(condition, true);
                break;
            }
            ASTNode *else_ = validate_ast(v, AS_CONDITIONAL_NODE(n)->else_);
            if(!else_) {
                astNodeFree(condition, true);
                astNodeFree(body, true);
                break;
            }

            result = astNewConditionalNode(n->node_type, n->location, condition, body, else_);
            break;
        }
        case ND_WHILE_LOOP: {
            // NOTE: ND_FOR_LOOP and ND_FOR_ITERATOR_LOOP should be handled separately
            //       because they use the rest of the clauses (initializer, increment).
            ASTNode *condition = validate_ast(v, AS_LOOP_NODE(n)->condition);
            if(!condition)
                break;
            ASTNode *body = validate_ast(v, AS_LOOP_NODE(n)->body);
            if(!body) {
                astNodeFree(condition, true);
                break;
            }
            result = astNewLoopNode(n->location, NULL, condition, NULL, body);
            break;
        }
        case ND_CALL: {
            ASTNode *callee_node = validate_ast(v, AS_BINARY_NODE(n)->lhs);
            if(!callee_node) {
                astNodeFree(AS_BINARY_NODE(n)->rhs, true);
                break;
            }
            ASTObj *callee = AS_OBJ_NODE(callee_node)->obj;
            ASTListNode *arguments = AS_LIST_NODE(AS_BINARY_NODE(n)->rhs);
            if(!is_callable(callee)) {
                error(v, n->location, "Called object '%s' of type '%s' isn't callable.", callee->name, type_name(callee->data_type));
                return false;
            }
            ASTListNode *new_args = AS_LIST_NODE(astNewListNode(ND_ARGS, arguments->header.location, arguments->scope));
            for(usize i = 0; i < arguments->nodes.used; ++i) {
                ASTNode *arg = validate_ast(v, ARRAY_GET_AS(ASTNode *, &arguments->nodes, i));
                if(arg) {
                    arrayPush(&new_args->nodes, (void *)arg);
                }
            }
            // break if an error was encountered when validating the arguments.
            if(v->had_error)
                break;
            Array *parameters = &callee->data_type->as.fn.parameter_types;
            if(parameters->used != arguments->nodes.used) {
                error(v, arguments->header.location, "Expected %zu %s but got %zu.",
                      parameters->used, parameters->used == 1 ? "argument" : "arguments",
                      arguments->nodes.used);
            }
            // Free [arguments] (rhs) but not the actual nodes because they where already freed.
            astNodeFree(AS_NODE(arguments), false);
            result = astNewBinaryNode(n->node_type, n->location, callee_node, AS_NODE(new_args));
            break;
        }
        case ND_IDENTIFIER: {
            // find the object (variable visible in current scope or function) the identifier refers to.
            ASTObj *obj = find_variable(v, AS_IDENTIFIER_NODE(n)->identifier);
            if(!obj) {
                obj = find_function(v, AS_IDENTIFIER_NODE(n)->identifier);
                if(!obj) {
                    error(v, n->location, "Symbol '%s' doesn't exist in this scope.", AS_IDENTIFIER_NODE(n)->identifier);
                    break;
                }
            }
            result = astNewObjNode(obj->type == OBJ_VAR ? ND_VARIABLE : ND_FUNCTION, n->location, obj);
            break;
        }
        case ND_PROPERTY_ACCESS: {
            // FIXME: this doesn't allow nested property access (a.b.c for example).
            ASTNode *var = validate_ast(v, AS_BINARY_NODE(n)->lhs); // validate_ast() so the id node is replaced.
            if(!var) {
                astNodeFree(AS_BINARY_NODE(n)->rhs, true);
                break;
            }
            ASTIdentifierNode *property_name = AS_IDENTIFIER_NODE(AS_BINARY_NODE(n)->rhs);
            VERIFY(NODE_IS(var, ND_VARIABLE) && AS_OBJ_NODE(var)->obj->data_type);
            ASTObj *var_obj = AS_OBJ_NODE(var)->obj;
            if(var_obj->data_type->type != TY_STRUCT) {
                error(v, property_name->header.location, "Field access on value of non-struct type '%s'.", type_name(var_obj->data_type));
                astNodeFree(AS_NODE(property_name), true);
                break;
            }
            ASTObj *s = find_struct(v, var_obj->data_type->name);
            VERIFY(s); // The struct should exist if its type exists.
            FOR(i, s->as.structure.fields) {
                ASTObj *field = ARRAY_GET_AS(ASTObj *, &s->as.structure.fields, i);
                if(field->name == property_name->identifier) {
                    ASTNode *new_property_node = astNewObjNode(ND_VARIABLE, field->location, field);
                    astNodeFree(AS_NODE(property_name), true);
                    result = astNewBinaryNode(ND_PROPERTY_ACCESS, n->location, var, new_property_node);
                    goto property_access_name_case_label_end;
                }
            }
            error(v, property_name->header.location, "Field '%s' doesn't exist in struct '%s'.", property_name->identifier, s->name);
            astNodeFree(AS_NODE(property_name), true);
property_access_name_case_label_end:
            break;
        }
        // ignored nodes (no validating to do).
        case ND_NUMBER_LITERAL:
        case ND_VARIABLE:
        case ND_FUNCTION:
            // Return early so the node isn't freed (there is no need to create a copy of nodes we don't change).
            return n;
        case ND_ARGS: // Argument nodes should never appear outside of a call (which doesn't validate them using this function).
        default:
            UNREACHABLE();
    }
    astNodeFree(n, false); // A node's children are NOT freed because they should be freed by the validating.
    return result;
}

static bool validate_type(Validator *v, Type **ty) {
    if(*ty && (*ty)->type == TY_ID) {
        ASTObj *s = find_struct(v, (*ty)->name);
        if(s) {
            *ty = s->data_type;
        } else {
            error(v, (*ty)->decl_location, "Unknown type '%s'.", (*ty)->name);
            return false;
        }
    }
    return true;
}

static bool validate_function(Validator *v, ASTObj *fn) {
    if(stringEqual(fn->name, "main")) {
        v->found_main = true;
        // TODO: validate main():
        //       -  return type.
        //       - parameters.
    }

    v->current_function = fn;

    FOR(i, fn->as.fn.locals) {
        ASTObj *l = ARRAY_GET_AS(ASTObj *, &fn->as.fn.locals, i);
        validate_type(v, &l->data_type);
    }
    FOR(i, fn->as.fn.parameters) {
        ASTObj *p = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        validate_type(v, &p->data_type);
    }
    // FIXME: un-duplicate parameter types (currently both in fn object and fn type).
    FOR(i, fn->data_type->as.fn.parameter_types) {
        Type **ty = (Type **)(fn->data_type->as.fn.parameter_types.data + i);
        validate_type(v, ty);
    }

    FOR(i, fn->as.fn.body->nodes) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &fn->as.fn.body->nodes, i);
        ASTNode *new_n = validate_ast(v, n);
        // Always insert [new_n], even if it is NULL
        // as [n] is always freed so we want to get rid to any references to it.
        // (astFreeNode() skips NULL nodes).
        arrayInsert(&fn->as.fn.body->nodes, i, (void *)new_n);
    }

    if(fn->as.fn.return_type) {
        if(fn->as.fn.body->control_flow == CF_NEVER_RETURNS) {
            // The location created points to the closing '}' of the function body.
            error(v, locationNew(fn->as.fn.body->header.location.end - 1,
                                 fn->as.fn.body->header.location.end,
                                fn->as.fn.body->header.location.file),
                  "Control reaches end of non-void function '%s'.", fn->name);
        }
    }

    v->current_function = NULL;
    return true;
}

static bool validate_struct(Validator *v, ASTObj *s) {
    FOR(i, s->as.structure.fields) {
        ASTObj *field = ARRAY_GET_AS(ASTObj *, &s->as.structure.fields, i);
        if(!validate_type(v, &field->data_type))
            continue;
        if(typeEqual(field->data_type, s->data_type)) {
            error(v, field->location, "struct '%s' cannot have a field that recursively contains it.", s->name);
            return false; // FIXME: check all fields before returning.
        }
    }
    return true;
}

static bool validate_object(Validator *v, ASTObj *obj) {
    switch(obj->type) {
        case OBJ_VAR:
            return validate_type(v, &obj->data_type);
        case OBJ_FN:
            if(global_id_exists(v, obj->name)) {
                error(v, obj->location, "Symbol '%s' already exists.", obj->name);
            } else {
                add_global_id(v, obj->name);
            }
            return validate_function(v, obj);
        case OBJ_STRUCT:
            return validate_struct(v, obj);
        default:
            UNREACHABLE();
    }
    return true;
}

static ASTNode *validate_global_variable(Validator *v, ASTNode *g) {
    ASTNode *result = NULL;
    switch(g->node_type) {
        case ND_VAR_DECL:
            result = validate_variable_declaration(v, g);
            astNodeFree(g, true);
            break;
        case ND_ASSIGN:
            VERIFY(NODE_IS(AS_BINARY_NODE(g)->lhs, ND_VAR_DECL));
            result = validate_assignment(v, g);
            break;
        default:
            UNREACHABLE();
    }
    return result;
}

static void validate_module_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;
    v->current_module = (ModuleID)index;

    FOR(i, m->globals) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &m->globals, i);
        ASTObj *var = NODE_IS(g, ND_ASSIGN)
                       ? AS_OBJ_NODE(AS_BINARY_NODE(g)->lhs)->obj
                       : AS_OBJ_NODE(g)->obj;
        if(global_id_exists(v, var->name)) {
            error(v, var->location, "Global symbol '%s' redefined.", var->name);
        } else {
            add_global_id(v, var->name);
        }
        ASTNode *new_g = validate_global_variable(v, g);
        if(new_g) {
            arrayInsert(&m->globals, i, (void *)new_g);
        }
    }
    
    // objects
    FOR(i, m->objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &m->objects, i);
        validate_object(v, obj); // Note: no need to handle errors here as we want to validate all objects always.
    }
}

/** Typechecker **/

static bool typecheck_assignment(Validator *v, ASTNode *n) {
    TRY(typecheck_ast(v, AS_BINARY_NODE(n)->rhs));
    // FIXME: support nested property access.
    ASTNode *var_node = NODE_IS(AS_BINARY_NODE(n)->lhs, ND_PROPERTY_ACCESS)
                      ? AS_BINARY_NODE(AS_BINARY_NODE(n)->lhs)->lhs : AS_BINARY_NODE(n)->lhs;
    VERIFY(NODE_IS(var_node, ND_VARIABLE) || NODE_IS(var_node, ND_VAR_DECL));
    // FIXME: support nested property access.
    Type *lhs_ty = NODE_IS(AS_BINARY_NODE(n)->lhs, ND_PROPERTY_ACCESS)
                 ? AS_OBJ_NODE(AS_BINARY_NODE(AS_BINARY_NODE(n)->lhs)->rhs)->obj->data_type
                 : AS_OBJ_NODE(var_node)->obj->data_type;
    Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->rhs);
    // allow assigning number literals to variables with unsigned types.
    if(IS_NUMERIC(lhs_ty) && IS_NUMERIC(rhs_ty) &&
        IS_UNSIGNED(lhs_ty) && IS_SIGNED(rhs_ty)
        && NODE_IS(AS_BINARY_NODE(n)->rhs, ND_NUMBER_LITERAL)) {
            return true;
    }
    TRY(check_types(v, AS_BINARY_NODE(n)->rhs->location, lhs_ty, rhs_ty));
    return true;
}

static bool typecheck_variable_declaration(Validator *v, ASTNode *decl) {
    // FIXME: The validator already does this, change to check if type is 'void' later (if void becomes a type).
    if(AS_OBJ_NODE(decl)->obj->data_type == NULL) {
        error(v, decl->location, "Variable '%s' has no type (Hint: consider adding an explicit type).", AS_OBJ_NODE(decl)->obj->name);
        return false;
    }
    return true;
}

static bool typecheck_ast(Validator *v, ASTNode *n) {
    switch(n->node_type) {
        case ND_VAR_DECL:
            TRY(typecheck_variable_declaration(v, n));
            break;
        //case ND_VARIABLE:
        case ND_ASSIGN:
            TRY(typecheck_assignment(v, n));
            break;
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE: {
            TRY(typecheck_ast(v, AS_BINARY_NODE(n)->lhs));
            TRY(typecheck_ast(v, AS_BINARY_NODE(n)->rhs));
            Type *lhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->lhs);
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->rhs);
            TRY(check_types(v, AS_BINARY_NODE(n)->rhs->location, lhs_ty, rhs_ty));
            break;
        }
        case ND_IF:
            TRY(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->condition));
            TRY(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->body));
            TRY(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->else_));
            break;
        case ND_WHILE_LOOP:
            // NOTE: ND_FOR_LOOP and ND_FOR_ITERATOR_LOOP should be handled separately
            //       because they use the rest of the clauses (initializer, increment).
            TRY(typecheck_ast(v, AS_LOOP_NODE(n)->condition));
            TRY(typecheck_ast(v, AS_LOOP_NODE(n)->body));
            break;
        case ND_BLOCK: {
            // typecheck all nodes in the block, even if some fail.
            bool failed = false;
            enter_scope(v, AS_LIST_NODE(n)->scope);
            for(usize i = 0; i < AS_LIST_NODE(n)->nodes.used; ++i) {
                failed = typecheck_ast(v, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(n)->nodes, i));
            }
            leave_scope(v);
            // failed == false -> success (return true).
            // failed == true -> failure (return false).
            return !failed;
        }
        case ND_NEGATE:
            return typecheck_ast(v, AS_UNARY_NODE(n)->operand);
        case ND_RETURN: {
            if(AS_UNARY_NODE(n)->operand == NULL) {
                break;
            }
            TRY(typecheck_ast(v, AS_UNARY_NODE(n)->operand));
            Type *operand_ty = get_expr_type(v, AS_UNARY_NODE(n)->operand);
            // When returning a number literal in a function returning an unsigned type,
            // the number literal is implicitly cast to the unsigned type.
            // FIXME: Check the number value fits in the return type.
            if((operand_ty && v->current_function->as.fn.return_type)
               && IS_NUMERIC(operand_ty) && IS_NUMERIC(v->current_function->as.fn.return_type)
               && IS_UNSIGNED(v->current_function->as.fn.return_type) &&
               NODE_IS(AS_UNARY_NODE(n)->operand, ND_NUMBER_LITERAL) && IS_SIGNED(operand_ty)) {
                break;
            }
            TRY(check_types(v, AS_UNARY_NODE(n)->operand->location,
                              v->current_function->as.fn.return_type,
                              operand_ty));
            break;
        }
        case ND_CALL: {
            // lhs: callee (ASTObj *)
            // rhs: arguments (Array<ASTObj *>)
            ASTObj *callee = AS_OBJ_NODE(AS_BINARY_NODE(n)->lhs)->obj;
            ASTListNode *arguments = AS_LIST_NODE(AS_BINARY_NODE(n)->rhs);
            bool had_error = false;
            // The validating pass makes sure the correct amount of arguments is passed,
            // so we can be sure that both arrays (params & args) are of equal length.
            for(usize i = 0; i < callee->data_type->as.fn.parameter_types.used; ++i) {
                Type *param_ty = ARRAY_GET_AS(Type *, &callee->data_type->as.fn.parameter_types, i);
                ASTNode *arg = ARRAY_GET_AS(ASTNode *, &arguments->nodes, i);
                if(!typecheck_ast(v, arg)) {
                    break;
                }
                Type *arg_ty = get_expr_type(v, arg);
                // The type of a number literal is i32, but it is implicitly casted
                // into unsigned types so calls like this: `a(123)` where the parameter
                // has the type of 'u32' and the argument is a number literal but has the type
                // of 'i32' will typecheck correctly without emiting a type mismatch.
                // FIXME: Check that the number value fits in the parameter type.
                if((arg_ty && IS_NUMERIC(arg_ty)) && IS_UNSIGNED(param_ty) &&
                   NODE_IS(arg, ND_NUMBER_LITERAL) && IS_SIGNED(arg_ty)) {
                    continue;
                }
                if(!check_types(v, arg->location, param_ty, arg_ty)) {
                    had_error = true;
                }
            }
            return !had_error;
        }
        // ignored nodes (no typechecking to do).
        case ND_NUMBER_LITERAL:
        case ND_FUNCTION:
        case ND_VARIABLE: // TODO: is it ok to ignore this node?
        case ND_PROPERTY_ACCESS:
            break;
        case ND_ARGS: // see note in validate_ast() for the reason this node is an error here.
        case ND_IDENTIFIER: // see note in validate_ast() for the reason this node is an error here.
        default:
            UNREACHABLE();
    }
    return true;
}

static bool typecheck_global_variable(Validator *v, ASTNode *g) {
    bool result = true;
    switch(g->node_type) {
        case ND_VAR_DECL:
            result = typecheck_variable_declaration(v, g);
            break;
        case ND_ASSIGN:
            result = typecheck_assignment(v, g);
            break;
        default:
            UNREACHABLE();
    }
    return result;
}

static bool typecheck_function(Validator *v, ASTObj *fn) {
    v->current_function = fn;
    bool had_error = false;

    // The parser checks that parameters have types.

    FOR(i, fn->as.fn.body->nodes) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &fn->as.fn.body->nodes, i);
        if(!typecheck_ast(v, n)) {
            had_error = true;
        }
    }

    v->current_function = NULL;
    return !had_error;
}

static bool typecheck_struct(Validator *v, ASTObj *s) {
    FOR(i, s->as.structure.fields) {
        ASTObj *field = ARRAY_GET_AS(ASTObj *, &s->as.structure.fields, i);
        if(field->data_type == NULL) {
            error(v, field->location, "Field '%s' in struct '%s' has no type.", field->name, s->name);
            return false; // FIXME: typecheck all fields before returning.
        }
    }
    return true;
}

static bool typecheck_object(Validator *v, ASTObj *obj) {
    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            return typecheck_function(v, obj);
        case OBJ_STRUCT:
            return typecheck_struct(v, obj);
        default:
            UNREACHABLE();
    }
    return true;
}

static void typecheck_module_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;
    v->current_module = (ModuleID)index;

    // Global variables
    FOR(i, m->globals) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &m->globals, i);
        typecheck_global_variable(v, g);
    }
    // TODO: handle errors here?
    //       after all we wan't to typecheck everything always, but what about cascading errors?

    // Objects
    FOR(i, m->objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &m->objects, i);
        typecheck_object(v, obj); // Note: no need to handle errors as we wan't to typecheck all objects always.
    }
}

#undef TRY
#undef FOR

bool validatorValidate(Validator *v, ASTProgram *prog) {
    v->program = prog;

    // Validating pass - finish building the AST.
    arrayMapIndex(&prog->modules, validate_module_callback, (void *)v);
    // Check that main() exists.
    if(!v->found_main) {
        Error *err;
        NEW0(err);
        errorInit(err, ERR_ERROR, false, locationNew(0, 0, 0), "No entry point (hint: Consider adding a 'main' function).");
        v->had_error = true;
        compilerAddError(v->compiler, err);
    }

    if(!v->had_error) {
        // Typechecking pass - typecheck the AST.
        arrayMapIndex(&prog->modules, typecheck_module_callback, (void *)v);
    }

    v->program = NULL;
    return !v->had_error;
}
