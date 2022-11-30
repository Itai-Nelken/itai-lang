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
    tableInit(&v->locals_already_declared_in_current_function, NULL, NULL);
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
    tableFree(&v->locals_already_declared_in_current_function);
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

static bool validate_ast(Validator *v, ASTNode *n);
static bool validate_variable(Validator *v, ASTNode *var);

// NOTE: initializer cann be NULL.
static bool validate_var_decl(Validator *v, ASTNode *var_decl, ASTNode *initializer) {
    VERIFY(NODE_IS(var_decl, ND_VAR_DECL));
    ASTObj *var_obj = AS_OBJ_NODE(var_decl)->obj;

    if(var_obj->data_type == NULL) {
        if(initializer) {
            if(NODE_IS(initializer, ND_PROPERTY_ACCESS)) {
                CHECK(validate_variable(v, initializer));
            } else if(NODE_IS(initializer, ND_VARIABLE)
                      && AS_OBJ_NODE(var_decl)->obj == AS_OBJ_NODE(initializer)->obj) {
                error(v, initializer->location, "Variable '%s' assigned to itself in declaration.", AS_OBJ_NODE(initializer)->obj->name);
                return false;
            } else {
                CHECK(validate_ast(v, initializer));
            }
            Type *rhs_ty = get_expr_type(v, initializer);
            if(!rhs_ty) {
                error(v, var_decl->location, "Failed to infer type (Hint: consider adding an explicit type).");
                return false;
            }
            var_obj->data_type = rhs_ty;
        } else {
            error(v, var_decl->location, "Variable '%s' has no type (Hint: consider adding an explicit type).", var_obj->name);
            return false;
        }
    }
    tableSet(&v->locals_already_declared_in_current_function, (void *)var_obj->name, NULL);
    return true;
}

static bool validate_variable(Validator *v, ASTNode *var) {
    if(NODE_IS(var, ND_VAR_DECL)) {
        CHECK(validate_var_decl(v, var, NULL));
        return true;
    } else if(NODE_IS(var, ND_PROPERTY_ACCESS)) {
            // FIXME: This doesn't work with nested property access (e.g. a.b.c).
            CHECK(validate_variable(v, AS_BINARY_NODE(var)->lhs));
            ASTObj *var_obj = AS_OBJ_NODE(AS_BINARY_NODE(var)->lhs)->obj;
            ASTNode **field_node = &(AS_BINARY_NODE(var)->rhs);
            VERIFY(var_obj->data_type);
            if(var_obj->data_type->type != TY_STRUCT) {
                error(v, (*field_node)->location, "Field access on value of non-struct type '%s'.", type_name(var_obj->data_type));
                return false;
            }
            ASTObj *s = find_struct(v, var_obj->data_type->name);
            VERIFY(s);
            for(usize i = 0; i < s->as.structure.fields.used; ++i) {
                ASTObj *field = ARRAY_GET_AS(ASTObj *, &s->as.structure.fields, i);
                if(field->name == AS_IDENTIFIER_NODE(*field_node)->identifier) {
                    ASTNode *new_field_node = astNewObjNode(ND_VARIABLE, field->location, field);
                    astNodeFree(AS_NODE(*field_node));
                    *field_node = new_field_node;
                    return true;
                }
            }
            error(v, (*field_node)->location, "Field '%s' doesn't exist in struct '%s'.", AS_IDENTIFIER_NODE(*field_node)->identifier, s->name);
            return false;
    } else if(NODE_IS(var, ND_ASSIGN)) {
        if(NODE_IS(AS_BINARY_NODE(var)->lhs, ND_VAR_DECL)) {
            return validate_var_decl(v, AS_BINARY_NODE(var)->lhs, AS_BINARY_NODE(var)->rhs);
        }
        CHECK(validate_variable(v, AS_BINARY_NODE(var)->lhs));
        return validate_ast(v, AS_BINARY_NODE(var)->rhs);
    }
    VERIFY(NODE_IS(var, ND_VARIABLE));
    ASTObj *var_obj = AS_OBJ_NODE(var)->obj;
    VERIFY(var_obj->type == OBJ_VAR);
    if(tableGet(&v->locals_already_declared_in_current_function, (void *)var_obj->name) == NULL) {
        error(v, var->location, "Undeclared variable '%s'.", var_obj->name);
        return false;
    }
    return true;
}

static bool validate_ast(Validator *v, ASTNode *n) {
    switch(n->node_type) {
        case ND_BLOCK: {
            // typecheck all nodes in the block, even if some fail.
            bool failed = false;
            enter_scope(v, AS_LIST_NODE(n)->scope);
            for(usize i = 0; i < AS_LIST_NODE(n)->nodes.used; ++i) {
                failed = validate_ast(v, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(n)->nodes, i));
            }
            leave_scope(v);
            // failed == false -> success (return true).
            // failed == true -> failure (return false).
            return !failed;
        }
        case ND_ASSIGN: // No need to validate rhs as validate_variable() already does that.
        case ND_PROPERTY_ACCESS:
        case ND_VARIABLE:
        case ND_VAR_DECL:
            return validate_variable(v, n);
        case ND_NEGATE:
            return validate_ast(v, AS_UNARY_NODE(n)->operand);
        case ND_RETURN:
            if(v->current_function->as.fn.return_type && !AS_UNARY_NODE(n)->operand) {
                error(v, n->location, "'return' with no value in function '%s' returning '%s'.",
                      v->current_function->name, type_name(v->current_function->as.fn.return_type));
                return false;
            }
            if(AS_UNARY_NODE(n)->operand) {
                CHECK(validate_ast(v, AS_UNARY_NODE(n)->operand));
            }
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
        case ND_GE:
            CHECK(validate_ast(v, AS_BINARY_NODE(n)->lhs));
            CHECK(validate_ast(v, AS_BINARY_NODE(n)->rhs));
            break;
        case ND_IF:
            CHECK(validate_ast(v, AS_CONDITIONAL_NODE(n)->condition));
            CHECK(validate_ast(v, AS_CONDITIONAL_NODE(n)->body));
            CHECK(validate_ast(v, AS_CONDITIONAL_NODE(n)->else_));
            break;
        case ND_WHILE_LOOP:
            // NOTE: ND_FOR_LOOP and ND_FOR_ITERATOR_LOOP should be handled separately
            //       because they use the rest of the clauses (initializer, increment).
            CHECK(validate_ast(v, AS_LOOP_NODE(n)->condition));
            CHECK(validate_ast(v, AS_LOOP_NODE(n)->body));
            break;
        case ND_CALL: {
            ASTObj *callee = AS_OBJ_NODE(AS_BINARY_NODE(n)->lhs)->obj;
            ASTListNode *arguments = AS_LIST_NODE(AS_BINARY_NODE(n)->rhs);
            if(!is_callable(callee)) {
                error(v, n->location, "Called object '%s' of type '%s' isn't callable.", callee->name, type_name(callee->data_type));
                return false;
            }
            for(usize i = 0; i < arguments->nodes.used; ++i) {
                CHECK(validate_ast(v, ARRAY_GET_AS(ASTNode *, &arguments->nodes, i)));
            }
            Array *parameters = &callee->data_type->as.fn.parameter_types;
            if(parameters->used != arguments->nodes.used) {
                error(v, arguments->header.location, "Expected %zu %s but got %zu.",
                      parameters->used, parameters->used == 1 ? "argument" : "arguments",
                      arguments->nodes.used);
            }
            break;
        }
        // ignored nodes (no validating to do).
        case ND_NUMBER_LITERAL:
        case ND_FUNCTION:
            return true;
        case ND_ARGS: // argument nodes should never appear outside of a call which doesn't validate it.
        case ND_IDENTIFIER: // identifier nodes should be replaced with object nodes before calling validate_ast().
        default:
            UNREACHABLE();
    }
    return true;
}

static bool replace_all_ids_with_objs(Validator *v, ASTNode **tree) {
    switch((*tree)->node_type) {
        case ND_ARGS:
        case ND_BLOCK: {
            bool success = true;
            for(usize i = 0; i < AS_LIST_NODE(*tree)->nodes.used; ++i) {
                ASTNode **n = (ASTNode **)(AS_LIST_NODE(*tree)->nodes.data + i);
                success = !success ? false : replace_all_ids_with_objs(v, n);
            }
            return success;
        }
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
        case ND_CALL:
        case ND_ASSIGN: {
            bool lhs_result = replace_all_ids_with_objs(v, &(AS_BINARY_NODE(*tree)->lhs));
            bool rhs_result = replace_all_ids_with_objs(v, &(AS_BINARY_NODE(*tree)->rhs));
            return (!lhs_result || !rhs_result) ? false : true;
        }
        case ND_PROPERTY_ACCESS:
            if(!replace_all_ids_with_objs(v, &(AS_BINARY_NODE(*tree)->lhs))) {
                return false;
            }
            // NOTE: We can't replace the identifier in rhs because we don't know the type of the struct lhs is an instance of.
            //       The identifier is replaced in validate_variable() after the type is known.
            break;
        case ND_IF: {
            bool cond_result = replace_all_ids_with_objs(v, &(AS_CONDITIONAL_NODE(*tree)->condition));
            bool body_result = replace_all_ids_with_objs(v, &(AS_CONDITIONAL_NODE(*tree)->body));
            bool else_result = replace_all_ids_with_objs(v, &(AS_CONDITIONAL_NODE(*tree)->else_));
            return (!cond_result || !body_result || !else_result) ? false : true;
        }
        case ND_WHILE_LOOP:
            // NOTE: ND_FOR_LOOP and ND_FOR_ITERATOR_LOOP should be handled separately
            //       because they use the rest of the clauses (initializer, increment).
            if(!replace_all_ids_with_objs(v, &(AS_LOOP_NODE(*tree)->condition))) {
                return false;
            }
            if(!replace_all_ids_with_objs(v, &(AS_LOOP_NODE(*tree)->body))) {
                return false;
            }
            break;
        case ND_IDENTIFIER: {
            ASTObj *obj = find_variable(v, AS_IDENTIFIER_NODE(*tree)->identifier);
            if(!obj) {
                obj = find_function(v, AS_IDENTIFIER_NODE(*tree)->identifier);
                if(!obj) {
                    error(v, (*tree)->location, "Symbol '%s' doesn't exist.", AS_IDENTIFIER_NODE(*tree)->identifier);
                    return false;
                }
            }
            ASTNode *node = astNewObjNode(obj->type == OBJ_VAR ? ND_VARIABLE : ND_FUNCTION, (*tree)->location, obj);
            astNodeFree(*tree);
            *tree = node;
            break;
        }
        // unary nodes
        case ND_RETURN:
            if(AS_UNARY_NODE(*tree)->operand == NULL) {
                break;
            }
            // fallthrough
        case ND_NEGATE:
            return replace_all_ids_with_objs(v, &(AS_UNARY_NODE(*tree)->operand));
        // ignored nodes (no object/object already exists).
        case ND_NUMBER_LITERAL:
        case ND_VARIABLE:
        case ND_VAR_DECL:
            break;
        default:
            UNREACHABLE();
    }
    return true;
}

static void validate_function(Validator *v, ASTObj *fn) {
    VERIFY(fn->type == OBJ_FN);

    if(stringEqual(fn->name, "main")) {
        v->found_main = true;
        // TODO: validate main()
        //       - Return type
        //       - Parameters
    }

    v->current_function = fn;

    for(usize i = 0; i < fn->as.fn.parameters.used; ++i) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        tableSet(&v->locals_already_declared_in_current_function, (void *)param->name, NULL);
    }

    // Manually iterate over the body because validate_ast() enters the block node's scope,
    // so it cannot used here because function scope isn't represented with a ScopeID.
    // Another reason is that we need the address of the pointer to the memory of the nodes
    // so we can reallocate the nodes (if their type has to be changed).
    for(usize i = 0; i < fn->as.fn.body->nodes.used; ++i) {
        ASTNode **n = (ASTNode **)(fn->as.fn.body->nodes.data + i);
        if(!replace_all_ids_with_objs(v, n)) {
            // The validator expects no identifier nodes,
            // so if we failed to replace all of them we can't validate.
            continue;
        }
        validate_ast(v, *n);
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
    tableClear(&v->locals_already_declared_in_current_function, NULL, NULL);
}

static void validate_struct(Validator *v, ASTObj *s) {
    UNUSED(v);
    UNUSED(s);
    // nothing to do.
}

static void validate_object_callback(void *object, void *validator) {
    ASTObj *obj = (ASTObj *)object;
    Validator *v = (Validator *)validator;

    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            if(global_id_exists(v, obj->name)) {
                error(v, obj->location, "Symbol '%s' already exists.", obj->name);
            } else {
                add_global_id(v, obj->name);
            }
            validate_function(v, obj);
            break;
        case OBJ_STRUCT:
            validate_struct(v, obj);
            break;
        default:
            UNREACHABLE();
    }
}

static void validate_type_callback(TableItem *item, bool is_last, void *validator) {
    UNUSED(is_last);
    Type **ty = (Type **)&item->value;
    Validator *v = (Validator *)validator;

    if((*ty)->type != TY_ID) {
        return;
    }

    ASTObj *s = find_struct(v, (*ty)->name);
    if(!s) {
        error(v, (*ty)->decl_location, "Unknown type '%s'.", (*ty)->name);
    } else {
        *ty = s->data_type;
    }
}

static void validate_module_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;

    v->current_module = (ModuleID)index;
    tableMap(&m->types, validate_type_callback, validator);
    for(usize i = 0; i < m->globals.used; ++i) {
        ASTNode **g = (ASTNode **)(m->globals.data + i);
        ASTObj *var = NODE_IS(*g, ND_ASSIGN) ? AS_OBJ_NODE(AS_BINARY_NODE(*g)->lhs)->obj : AS_OBJ_NODE(*g)->obj;
        if(global_id_exists(v, var->name)) {
            error(v, var->location, "Symbol '%s' already exists.", var->name);
        } else {
            add_global_id(v, var->name);
        }
        replace_all_ids_with_objs(v, g);
        validate_variable(v, *g);
    }
    arrayMap(&m->objects, validate_object_callback, validator);
    tableClear(&v->global_ids_in_current_module, NULL, NULL);
}

// typechecker forward declarations
static void typecheck_variable_callback(void *variable_node, void *validator);

static bool typecheck_ast(Validator *v, ASTNode *n) {
    switch(n->node_type) {
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
            CHECK(typecheck_ast(v, AS_BINARY_NODE(n)->lhs));
            CHECK(typecheck_ast(v, AS_BINARY_NODE(n)->rhs));
            Type *lhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->lhs);
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->rhs);
            CHECK(check_types(v, AS_BINARY_NODE(n)->rhs->location, lhs_ty, rhs_ty));
            break;
        }
        case ND_IF:
            CHECK(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->condition));
            CHECK(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->body));
            CHECK(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->else_));
            break;
        case ND_WHILE_LOOP:
            // NOTE: ND_FOR_LOOP and ND_FOR_ITERATOR_LOOP should be handled separately
            //       because they use the rest of the clauses (initializer, increment).
            CHECK(typecheck_ast(v, AS_LOOP_NODE(n)->condition));
            CHECK(typecheck_ast(v, AS_LOOP_NODE(n)->body));
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
        case ND_ASSIGN: // No need to typecheck rhs as typecheck_variable_callback() already does that.
        case ND_VAR_DECL:
        case ND_VARIABLE: {
            bool old_had_error = v->had_error;
            typecheck_variable_callback((void *)n, (void *)v);
            if(old_had_error != v->had_error) {
                return false;
            }
            break;
        }
        case ND_NEGATE:
            return typecheck_ast(v, AS_UNARY_NODE(n)->operand);
        case ND_RETURN: {
            if(AS_UNARY_NODE(n)->operand == NULL) {
                break;
            }
            CHECK(typecheck_ast(v, AS_UNARY_NODE(n)->operand));
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
            CHECK(check_types(v, AS_UNARY_NODE(n)->operand->location,
                              v->current_function->as.fn.return_type,
                              operand_ty));
            break;
        }
        case ND_CALL: {
            ASTObj *callee = AS_OBJ_NODE(AS_BINARY_NODE(n)->lhs)->obj;
            VERIFY(callee->data_type->type == TY_FN);
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
        case ND_PROPERTY_ACCESS:
            break;
        case ND_ARGS:
            // see note in validate_ast() for the reason this node is an error here.
            // fallthrough
        case ND_IDENTIFIER:
            // see note in validate_ast() for the reason this node is an error here.
            // fallthrough
        default:
            UNREACHABLE();
    }
    return true;
}

static void typecheck_variable_callback(void *variable_node, void *validator) {
    ASTNode *var = AS_NODE(variable_node);
    Validator *v = (Validator *)validator;

    switch(var->node_type) {
        case ND_VARIABLE: // TODO: This isn't needed here anymore.
        case ND_VAR_DECL: // Is this neccesary? the validator already checks that.
            if(AS_OBJ_NODE(var)->obj->data_type == NULL) {
                error(v, var->location, "Variable '%s' has no type (Hint: consider adding an explicit type).", AS_OBJ_NODE(var)->obj->name);
                return;
            }
            break;
        case ND_ASSIGN: {
            if(!typecheck_ast(v, AS_BINARY_NODE(var)->rhs)) {
                return;
            }
            ASTNode *var_node = NULL;
            if(NODE_IS(AS_BINARY_NODE(var)->lhs, ND_PROPERTY_ACCESS)) {
                // FIXME: This doesn't support nested property access (e.g. a.b.c).
                var_node = AS_BINARY_NODE(AS_BINARY_NODE(var)->lhs)->rhs; // rhs - we wan't the field.
            } else {
                var_node = AS_BINARY_NODE(var)->lhs;
            }
            VERIFY(NODE_IS(var_node, ND_VARIABLE) || NODE_IS(var_node, ND_VAR_DECL));
            Type *lhs_ty = AS_OBJ_NODE(var_node)->obj->data_type;
            Type *rhs_ty = get_expr_type(v, AS_BINARY_NODE(var)->rhs);
            // allow assigning number literals to u32 variables.
            if(IS_NUMERIC(lhs_ty) && IS_NUMERIC(rhs_ty) &&
                IS_UNSIGNED(lhs_ty) && IS_SIGNED(rhs_ty)
                && NODE_IS(AS_BINARY_NODE(var)->rhs, ND_NUMBER_LITERAL)) {
                    break;
            }
            check_types(v, AS_BINARY_NODE(var)->rhs->location, lhs_ty, rhs_ty);
            break;
        }
        default:
            UNREACHABLE();
    }
}

static void typecheck_function(Validator *v, ASTObj *fn) {
    VERIFY(fn->type == OBJ_FN);

    v->current_function = fn;

    for(usize i = 0; i < fn->as.fn.parameters.used; ++i) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        VERIFY(param->type == OBJ_VAR);
        if(param->data_type == NULL) {
            error(v, param->location, "Parameter '%s' has no type!", param->name);
        }
    }
    if(v->had_error) {
        return;
    }

    // See comment in validate_function() for an explanation of why
    // it isn't possible to call typecheck_ast() on the body directly.
    for(usize i = 0; i < fn->as.fn.body->nodes.used; ++i) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &fn->as.fn.body->nodes, i);
        typecheck_ast(v, n);
    }

    v->current_function = NULL;
}

static void typecheck_struct(Validator *v, ASTObj *s) {
    for(usize i = 0; i < s->as.structure.fields.used; ++i) {
        ASTObj *member = ARRAY_GET_AS(ASTObj *, &s->as.structure.fields, i);
        if(!member->data_type) {
            error(v, member->location, "Member '%s' in struct '%s' has no type.", member->name, s->name);
        }
    }
}

static void typecheck_object_callback(void *object, void *validator) {
    ASTObj *obj = (ASTObj *)object;
    Validator *v = (Validator *)validator;

    switch(obj->type) {
        case OBJ_FN:
            typecheck_function(v, obj);
            break;
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_STRUCT:
            typecheck_struct(v, obj);
            break;
        default:
            UNREACHABLE();
    }
}

static void typecheck_module_callback(void *module, usize index, void *validator) {
    ASTModule *m = (ASTModule *)module;
    Validator *v = (Validator *)validator;

    v->current_module = (ModuleID)index;
    arrayMap(&m->globals, typecheck_variable_callback, validator);
    arrayMap(&m->objects, typecheck_object_callback, validator);
}


#undef CHECK

bool validatorValidate(Validator *v, ASTProgram *prog) {
    v->program = prog;

    // Validating pass - finish building the AST.
    arrayMapIndex(&v->program->modules, validate_module_callback, (void *)v);
    if(!v->found_main) {
        Error *err;
        NEW0(err);
        errorInit(err, ERR_ERROR, false, locationNew(0, 0, 0), "No entry point (hint: Consider adding a 'main' function).");
        v->had_error = true;
        compilerAddError(v->compiler, err);
    }

    if(!v->had_error) {
        // Typechecking pass - typecheck the AST.
        arrayMapIndex(&v->program->modules, typecheck_module_callback, (void *)v);
    }


    v->program = NULL;
    return !v->had_error;
}
