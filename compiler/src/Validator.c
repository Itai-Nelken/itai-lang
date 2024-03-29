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
    v->current_allocator = NULL;
    v->current_function = NULL;
    v->current_scope = NULL;
    v->found_main = false;
    v->had_error = false;
    tableInit(&v->declared_global_ids, NULL, NULL);
    tableInit(&v->visible_locals_in_current_function, NULL, NULL);
}

void validatorFree(Validator *v) {
    v->compiler = NULL;
    v->program = NULL;
    v->current_module = 0;
    v->current_allocator = NULL;
    v->current_function = NULL;
    v->current_scope = NULL;
    v->found_main = false;
    v->had_error = false;
    tableFree(&v->declared_global_ids);
    tableFree(&v->visible_locals_in_current_function);
}

// Note: [msg] is NOT freed.
static void add_error(Validator *v, Location loc, ErrorType type, String msg) {
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

    add_error(v, loc, ERR_ERROR, msg);
    stringFree(msg);
}

static void hint(Validator *v, Location loc, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String msg = stringVFormat(format, ap);
    va_end(ap);

    add_error(v, loc, ERR_HINT, msg);
    stringFree(msg);
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
    if(a == NULL || b == NULL || !typeEqual(a, b)) {
        error(v, loc, "Type mismatch: expected '%s' but got '%s'.", type_name(a), type_name(b));
        return false;
    }
    return true;
}

static inline bool is_callable(ASTObj *obj) {
    return obj->data_type->type == TY_FN;
}

static Type *get_expr_type(Validator *v, ASTNode *expr);
// Note: validate the node before providing it to this function.
static inline bool is_variable(ASTNode *n) {
    // Notes: Update get_variable_node() if condition is changed.
    //        The operands of ND_DEREF & ND_ADDROF must be variables,
    //        so any nodes peovided MUST be validated with validate_ast()
    //        first so no ND_DEREFs & ND_ADDROFs with non-variable operands
    //        are passed to this function.
    return NODE_IS(n, ND_VARIABLE) || NODE_IS(n, ND_DEREF) || NODE_IS(n, ND_ADDROF);
}
static inline ASTNode *get_variable_node(ASTNode *n) {
    switch(n->node_type) {
        case ND_VARIABLE:
            return n;
        case ND_DEREF:
        case ND_ADDROF:
            VERIFY(NODE_IS(AS_UNARY_NODE(n)->operand, ND_VARIABLE));
            return AS_UNARY_NODE(n)->operand;
        default:
            UNREACHABLE();
    }
}

static inline void enter_scope(Validator *v, ScopeID scope) {
    v->current_scope = astModuleGetScope(astProgramGetModule(v->program, v->current_module), scope);
}

static inline void leave_scope(Validator *v) {
    v->current_scope = astModuleGetScope(astProgramGetModule(v->program, v->current_module), v->current_scope->parent);
}

static ASTObj *find_global_var(Validator *v, ASTString name) {
    ASTModule *current_module = astProgramGetModule(v->program, v->current_module);
    for(usize i = 0; i < current_module->globals.used; ++i) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &current_module->globals, i);
        ASTObj *var = AS_OBJ_NODE(g->node_type == ND_ASSIGN ? AS_BINARY_NODE(g)->lhs : g)->obj;
        if(var->name == name) {
            return var;
        }
    }
    return NULL;
}

static ASTObj *find_local_var(Validator *v, ASTString name) {
    VERIFY(v->current_function); // Local variables don't exist outside of a function.
    ASTModule *current_module = astProgramGetModule(v->program, v->current_module);
    Scope *scope = v->current_scope;
    while(scope != NULL) {
        TableItem *i = tableGet(&scope->variables, (void *)name);
        if(i) {
            return (ASTObj *)i->value;
        }
        // FIXME: Allow astModuleGetScope() to return NULL in this case to remove this hacky if statement.
        if(scope->parent.module == EMPTY_SCOPE_ID().module && scope->parent.index == EMPTY_SCOPE_ID().index) {
            break;
        }
        scope = astModuleGetScope(current_module, scope->parent);
    }
    return NULL;
}

static ASTObj *find_variable(Validator *v, ASTString name, bool *is_global) {
    ASTObj *result = NULL;
    // If we are inside a function, search for a local variable first.
    if((v->current_function) && (result = find_local_var(v, name)) != NULL) {
        if(is_global) {
            *is_global = false;
        }
        return result;
    }
    // otherwise search for a global variable.
    if(is_global) {
        *is_global = true;
    }
    return find_global_var(v, name);
}

static ASTObj *find_function(Validator *v, ASTString name) {
    ASTModule *current_module = astProgramGetModule(v->program, v->current_module);
    TableItem *item = tableGet(&current_module->module_scope->functions, (void *)name);
    if(item != NULL) {
        return (ASTObj *)item->value;
    }
    return NULL;
}

static ASTObj *find_struct(Validator *v, ASTString name) {
    ASTModule *current_module = astProgramGetModule(v->program, v->current_module);
    TableItem *item = tableGet(&current_module->module_scope->structures, (void *)name);
    if(item != NULL) {
        return (ASTObj *)item->value;
    }
    return NULL;
}

static bool global_id_exists(Validator *v, ASTObj *id, ASTObj **prev) {
    TableItem *item;
    if((item = tableGet(&v->declared_global_ids, (void *)id->name)) != NULL) {
        *prev = (ASTObj *)item->value;
        return true;
    }
    tableSet(&v->declared_global_ids, (void *)id->name, (void *)id);
    return false;
}


/* forward declarations */
static ASTNode *validate_ast(Validator *v, ASTNode *n);
static bool typecheck_ast(Validator *v, ASTNode *n);
static bool validate_type(Validator *v, Type **ty, bool allow_pointers, Location *ptr_err_loc);


static Type *ptr_type(Validator *v, Type *inner) {
    Type *ptr;
    NEW0(ptr);
    ASTString name = astProgramAddString(v->program, stringFormat("&%s", inner->name));
    typeInit(ptr, TY_PTR, name, v->current_module);
    ptr->as.ptr.inner_type = inner;

    return scopeAddType(astProgramGetModule(v->program, v->current_module)->module_scope, ptr);
}

static Type *get_expr_type(Validator *v, ASTNode *expr) {
    Type *ty = NULL;
    switch(expr->node_type) {
        case ND_NUMBER_LITERAL:
            if(AS_LITERAL_NODE(expr)->ty) {
                ty = AS_LITERAL_NODE(expr)->ty;
            } else {
                // The default number literal type is i32.
                ty = v->program->primitives.int32;
            }
            break;
        case ND_STRING_LITERAL:
            ty = v->program->primitives.str;
            break;
        case ND_VAR_DECL:
        case ND_VARIABLE:
        case ND_FUNCTION:
            ty = AS_OBJ_NODE(expr)->obj->data_type;
            break;
        case ND_PROPERTY_ACCESS:
            // The type of a property access expression is the right-most
            // node, for example: the type of the expression a.b.c is the type of c.
            VERIFY(NODE_IS(AS_BINARY_NODE(expr)->rhs, ND_VARIABLE)); // If this fails, something is wrong with the parser.
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
        case ND_DEREF:
            ty = get_expr_type(v, AS_UNARY_NODE(expr)->operand);
            if(NODE_IS(expr, ND_DEREF) && ty->type == TY_PTR) {
                ty = ty->as.ptr.inner_type;
            }
            break;
        case ND_ADDROF:
            ty = ptr_type(v, get_expr_type(v, AS_UNARY_NODE(expr)->operand));
            break;
        default:
            UNREACHABLE();
    }
    return ty;
}


/** Validator **/

// NOTE: [decl] is NOT freed.
static ASTNode *validate_variable_declaration(Validator *v, ASTNode *decl) {
    // We don't care if a variable with the same name is already visible.
    // Scopes track what variable is actually visible, this table only checks that a local variable
    // with a certain name is declared.
    // Also, the parser checks for re-declarations. so no need to re-check here.
    tableSet(&v->visible_locals_in_current_function, (void *)AS_OBJ_NODE(decl)->obj->name, (void *)AS_OBJ_NODE(decl)->obj);
    return astNewObjNode(v->current_allocator, ND_VAR_DECL, decl->location, AS_OBJ_NODE(decl)->obj);
}

// NOTE: ownership of [n] is taken.
static ASTNode *validate_assignment(Validator *v, ASTNode *n) {
    // 1) Validate [lhs], this will replace any identifier nodes with variable nodes (and check if the variable exists).
    ASTNode *lhs = validate_ast(v, AS_BINARY_NODE(n)->lhs);
    if(!lhs) {
        return NULL;
    }
    if(!(NODE_IS(lhs, ND_VARIABLE) || NODE_IS(lhs, ND_DEREF) || NODE_IS(lhs, ND_VAR_DECL) || NODE_IS(lhs, ND_PROPERTY_ACCESS))) {
        error(v, lhs->location, "Invalid assignment target (only variables can be assigned).");
        return NULL;
    }
    // 2) Validate [rhs] - the expression whose value is being assigned to the variable.
    ASTNode *rhs = validate_ast(v, AS_BINARY_NODE(n)->rhs);
    if(!rhs) {
        return NULL;
    }
    // 3) If [lhs] is a variable declaration
    if(NODE_IS(lhs, ND_VAR_DECL)) {
        ASTObj *var = AS_OBJ_NODE(lhs)->obj;
        // a) Check that the variable isn't being assigned itself.
        if(is_variable(rhs) && AS_OBJ_NODE(get_variable_node(rhs))->obj == var) {
            error(v, rhs->location, "Variable '%s' assigned to itself in declaration.", AS_OBJ_NODE(get_variable_node(rhs))->obj->name);
            return NULL;
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
    // 5) Construct the validated assignment.
    return astNewBinaryNode(v->current_allocator, ND_ASSIGN, loc, lhs, rhs);
}

void collect_ids_for_property_access(Array *s, ASTNode *n) {
    VERIFY(NODE_IS(n, ND_PROPERTY_ACCESS)); // This function should only be called on property access nodes.

    while(NODE_IS(AS_BINARY_NODE(n)->lhs, ND_PROPERTY_ACCESS)) {
        arrayPush(s, (void *)AS_BINARY_NODE(n)->rhs);
        n = AS_BINARY_NODE(n)->lhs;
    }
    arrayPush(s, (void *)AS_BINARY_NODE(n)->rhs);
    arrayPush(s, (void *)AS_BINARY_NODE(n)->lhs);
    // [s] will now contain the identifier nodes from the property access expression.
    // for example, [s] will be [c, b, a] for the expression a.b.c
}

// NOTE: ownership of [n] is taken.
static ASTNode *validate_ast(Validator *v, ASTNode *n) {
    ASTNode *result = NULL;
    switch(n->node_type) {
        case ND_VAR_DECL:
            result = validate_variable_declaration(v, n);
            break;
        case ND_ASSIGN:
            result = validate_assignment(v, n);
            break;
        case ND_BLOCK: {
            ASTListNode *new_n = AS_LIST_NODE(astNewListNode(v->current_allocator, ND_BLOCK, n->location, AS_LIST_NODE(n)->scope, arrayLength(&AS_LIST_NODE(n)->nodes)));
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
                break;
            }
            result = AS_NODE(new_n);
            break;
        }
        // unary nodes
        case ND_ADDROF:
        case ND_DEREF:
        case ND_NEGATE: {
            ASTNode *operand = validate_ast(v, AS_UNARY_NODE(n)->operand);
            if(!operand) {
                break;
            }
            // The following check is for ND_ADDROF & ND_DEREF only.
            // It checks that the object whose address will be taken
            // is an lvalue.
            if((NODE_IS(n, ND_ADDROF) || NODE_IS(n, ND_DEREF)) && !is_variable(operand)) {
                error(v, operand->location, "Expected a variable.");
                break;
            } else if(NODE_IS(n, ND_DEREF) && AS_OBJ_NODE(operand)->obj->data_type->type != TY_PTR) {
                error(v, operand->location, "Expected variable to be of a pointer type.");
                break;
            }
            result = astNewUnaryNode(v->current_allocator, n->node_type, n->location, operand);
            break;
        }
        case ND_RETURN:
            if(v->current_function->as.fn.return_type->type != TY_VOID && AS_UNARY_NODE(n)->operand == NULL) {
                error(v, n->location, "'return' with no value in function '%s' returning '%s'.",
                      v->current_function->name, type_name(v->current_function->as.fn.return_type));
                break;
            }
            if(AS_UNARY_NODE(n)->operand) {
                ASTNode *operand = validate_ast(v, AS_UNARY_NODE(n)->operand);
                if(!operand)
                    break;
                result = astNewUnaryNode(v->current_allocator, n->node_type, n->location, operand);
                break;
            }
            result = astNewUnaryNode(v->current_allocator, n->node_type, n->location, NULL);
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
                break;
            }
            result = astNewBinaryNode(v->current_allocator, n->node_type, n->location, lhs, rhs);
            break;
        }
        case ND_IF: {
            ASTNode *condition = validate_ast(v, AS_CONDITIONAL_NODE(n)->condition);
            if(!condition)
                break;
            ASTNode *body = validate_ast(v, AS_CONDITIONAL_NODE(n)->body);
            if(!body) {
                break;
            }
            if(AS_CONDITIONAL_NODE(n)->else_) {
                ASTNode *else_ = validate_ast(v, AS_CONDITIONAL_NODE(n)->else_);
                if(!else_) {
                    break;
                }
                result = astNewConditionalNode(v->current_allocator, n->node_type, n->location, condition, body, else_);
                break;
            }

            result = astNewConditionalNode(v->current_allocator, n->node_type, n->location, condition, body, NULL);
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
                break;
            }
            result = astNewLoopNode(v->current_allocator, n->location, NULL, condition, NULL, body);
            break;
        }
        case ND_CALL: {
            ASTNode *callee_node = validate_ast(v, AS_BINARY_NODE(n)->lhs);
            if(!callee_node) {
                break;
            }
            ASTObj *callee = AS_OBJ_NODE(callee_node)->obj;
            ASTListNode *arguments = AS_LIST_NODE(AS_BINARY_NODE(n)->rhs);
            if(!is_callable(callee)) {
                error(v, n->location, "Called object '%s' of type '%s' isn't callable.", callee->name, type_name(callee->data_type));
                return false;
            }
            ASTListNode *new_args = AS_LIST_NODE(astNewListNode(v->current_allocator, ND_ARGS, arguments->header.location, arguments->scope, arrayLength(&AS_LIST_NODE(arguments)->nodes)));
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
            if(arrayLength(parameters) != arrayLength(&arguments->nodes)) {
                error(v, arguments->header.location, "Expected %zu %s but got %zu.",
                      arrayLength(parameters), arrayLength(parameters) == 1 ? "argument" : "arguments",
                      arguments->nodes.used);
                hint(v, callee->name_location, "Function '%s' defined here with %zu %s.",
                    callee->name,
                    arrayLength(parameters),
                    arrayLength(parameters) == 1 ? "parameter" : "parameters");
            }
            result = astNewBinaryNode(v->current_allocator, n->node_type, n->location, callee_node, AS_NODE(new_args));
            break;
        }
        case ND_IDENTIFIER: {
            // find the object (variable visible in current scope or function) the identifier refers to.
            bool is_global;
            ASTObj *obj = find_variable(v, AS_IDENTIFIER_NODE(n)->identifier, &is_global);
            if(!obj) {
                obj = find_function(v, AS_IDENTIFIER_NODE(n)->identifier);
                if(!obj) {
                    error(v, n->location, "Symbol '%s' doesn't exist in this scope.", AS_IDENTIFIER_NODE(n)->identifier);
                    break;
                }
            }
            if(!is_global && tableGet(&v->visible_locals_in_current_function, (void *)obj->name) == NULL) {
                error(v, n->location, "Undeclared variable '%s'.", obj->name);
                break;
            }
            result = astNewObjNode(v->current_allocator, obj->type == OBJ_VAR ? ND_VARIABLE : ND_FUNCTION, n->location, obj);
            break;
        }
        case ND_PROPERTY_ACCESS: {
            Array stack;
            arrayInit(&stack);
            // a.b.c => [c, b, a]
            collect_ids_for_property_access(&stack, n);
            ASTNode *lhs = validate_ast(v, ARRAY_POP_AS(ASTNode *, &stack)); // The variable.
            if(!lhs) {
                arrayFree(&stack);
                break;
            }
            if(!is_variable(lhs)) {
                error(v, lhs->location, "Property access can only be done on variables.");
                arrayFree(&stack);
                break;
            }
            if(AS_OBJ_NODE(lhs)->obj->data_type == NULL) {
                // This is done in the typechecker, but we need the type here, so we check.
                error(v, lhs->location, "Variable '%s' has no type.", AS_OBJ_NODE(lhs)->obj->name);
                hint(v, AS_OBJ_NODE(lhs)->obj->location, "Consider adding an explicit type here.");
                arrayFree(&stack);
                break;
            }
            while(arrayLength(&stack) > 0) {
                ASTNode *rhs = ARRAY_POP_AS(ASTNode *, &stack); // The field.
                Type *lhs_ty = get_expr_type(v, lhs);
                if(lhs_ty->type == TY_PTR && lhs_ty->as.ptr.inner_type->type == TY_STRUCT) {
                    // FIXME: this will break if pointers are allowed as struct members.
                    lhs = astNewUnaryNode(v->current_allocator, ND_DEREF, lhs->location, lhs);
                    lhs_ty = lhs_ty->as.ptr.inner_type;
                }
                if(lhs_ty->type != TY_STRUCT) {
                    error(v, lhs->location, "Field access on value of non-struct type '%s'.", type_name(lhs_ty));
                    break;
                }
                ASTObj *s = find_struct(v, lhs_ty->name);
                VERIFY(s); // The struct should exist if its type exists.
                bool found = false;
                Scope *s_scope = astModuleGetScope(astProgramGetModule(v->program, v->current_module), s->as.structure.scope);
                FOR(i, s_scope->objects) {
                    ASTObj *field = ARRAY_GET_AS(ASTObj *, &s_scope->objects, i);
                    VERIFY(field->type == OBJ_VAR);
                    if(field->name == AS_IDENTIFIER_NODE(rhs)->identifier) {
                        ASTNode *new_property_node = astNewObjNode(v->current_allocator, ND_VARIABLE, field->location, field);
                        if(result) {
                            result = astNewBinaryNode(v->current_allocator, ND_PROPERTY_ACCESS, locationMerge(result->location, rhs->location), result, new_property_node);
                        } else {
                            result = astNewBinaryNode(v->current_allocator, ND_PROPERTY_ACCESS, locationMerge(lhs->location, rhs->location), lhs, new_property_node);
                        }
                        found = true;
                        lhs = new_property_node;
                        break;
                    }
                }
                if(!found) {
                    error(v, rhs->location, "Field '%s' doesn't exist in struct '%s'.", AS_IDENTIFIER_NODE(rhs)->identifier, s->name);
                    break;
                }
            }
            arrayFree(&stack);
            break;
        }
        case ND_DEFER:
            error(v, n->location, "'defer' is not allowed in this context.");
            break;
        // ignored nodes (no validating to do).
        case ND_NUMBER_LITERAL:
        case ND_STRING_LITERAL:
        case ND_VARIABLE:
        case ND_FUNCTION:
            // Return early so the node isn't freed (there is no need to create a copy of nodes we don't change).
            return n;
        case ND_ARGS: // Argument nodes should never appear outside of a call (which doesn't validate them using this function).
        default:
            UNREACHABLE();
    }
    return result;
}

// Note: [ptr_err_loc] MUST be valid if [allow_pointers] is false.
static bool validate_type(Validator *v, Type **ty, bool allow_pointers, Location *ptr_err_loc) {
    // *ty might be NULL if the type wasn't inferred yet.
    if(*ty == NULL) {
        return true;
    }
    if((*ty)->type == TY_ID) {
        ASTObj *s = find_struct(v, (*ty)->name);
        if(s) {
            *ty = s->data_type;
        } else {
            error(v, (*ty)->decl_location, "Unknown type '%s'.", (*ty)->name);
            return false;
        }
    } else if((*ty)->type == TY_PTR) {
        if(!allow_pointers) {
            VERIFY(ptr_err_loc);
            error(v, *ptr_err_loc, "Pointer types are not allowed in this context.");
            return false;
        }
        validate_type(v, &(*ty)->as.ptr.inner_type, true, NULL);
    }
    return true;
}

static bool typecheck_ast(Validator *v, ASTNode *n);
static bool validate_function(Validator *v, ASTObj *fn) {
    if(stringEqual(fn->name, "main")) {
        v->found_main = true;
        // TODO: validate main():
        //       - return type.
        //       - parameters.
    }

    v->current_function = fn;

    Scope *fn_scope = astModuleGetScope(astProgramGetModule(v->program, v->current_module), fn->as.fn.body->scope);
    bool had_error = false;
    FOR(i, fn_scope->objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &fn_scope->objects, i);
        had_error = had_error ? true : !validate_type(v, &obj->data_type, false, &obj->location); // FIXME: use l.type_location
    }
    VERIFY(arrayLength(&fn->as.fn.parameters) == arrayLength(&fn->data_type->as.fn.parameter_types));
    for(usize i = 0; i < arrayLength(&fn->as.fn.parameters); ++i) {
        // Validate the type in the parameter variable object.
        ASTObj *p = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        had_error = had_error ? true : !validate_type(v, &p->data_type, true, NULL);
        tableSet(&v->visible_locals_in_current_function, (void *)p->name, (void *)p);

        // Validate the type in the function data type.
        Type **ty = (Type **)(fn->data_type->as.fn.parameter_types.data + i);
        had_error = had_error ? true : !validate_type(v, ty, true, NULL);
    }

    // If validating types failed, don't validate the body to stop cascading errors.
    if(had_error) {
        return false;
    }

    // FIXME: Use fn.type_location.
    TRY(validate_type(v, &fn->as.fn.return_type, false, &fn->location));

    ASTListNode *new_body = AS_LIST_NODE(astNewListNode(v->current_allocator,
                                                        fn->as.fn.body->header.node_type,
                                                        fn->as.fn.body->header.location,
                                                        fn->as.fn.body->scope,
                                                        arrayLength(&fn->as.fn.body->nodes)));
    new_body->control_flow = fn->as.fn.body->control_flow;
    enter_scope(v, new_body->scope);
    FOR(i, fn->as.fn.body->nodes) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &fn->as.fn.body->nodes, i);
        if(NODE_IS(n, ND_DEFER)) {
            ASTNode *operand = validate_ast(v, AS_UNARY_NODE(n)->operand);
            if(operand) {
                // defers are typechecked here (so the validator is in the correct state).
                if(!typecheck_ast(v, operand)) {
                    v->had_error = true; // should be set if there was an error, but just in case set it again.
                    continue;
                }
                arrayPush(&fn->as.fn.defers, (void *)operand);
                // 'defer's aren't pushed back to the body as they are saved in the 'fn.defers' array
                // and should be codegened separately at the function epilogue.
            }
        } else {
            ASTNode *new_n = validate_ast(v, n);
            // It doesn't matter if [new_n] is NULL as it means there was an error
            // and typechecking won't proceed.
            arrayPush(&new_body->nodes, (void *)new_n);
        }
    }
    leave_scope(v);
    fn->as.fn.body = new_body;
    arrayReverse(&fn->as.fn.defers); // Reverse [defers] so they are generated in the correct order.

    if(fn->as.fn.return_type->type != TY_VOID) {
        if(fn->as.fn.body->control_flow == CF_NEVER_RETURNS) {
            // The location created points to the closing '}' of the function body.
            error(v, locationNew(fn->as.fn.body->header.location.end - 1,
                                 fn->as.fn.body->header.location.end,
                                fn->as.fn.body->header.location.file),
                  "Control reaches end of non-void function '%s'.", fn->name);
        }
    }

    tableClear(&v->visible_locals_in_current_function, NULL, NULL);
    v->current_function = NULL;
    return true;
}

static bool validate_struct(Validator *v, ASTObj *s) {
    Scope *scope = astModuleGetScope(astProgramGetModule(v->program, v->current_module), s->as.structure.scope);
    FOR(i, scope->objects) {
        ASTObj *field = ARRAY_GET_AS(ASTObj *, &scope->objects, i);
        VERIFY(field->type == OBJ_VAR);
        // Pointers are not allowed as struct members.
        // Note: if this is changed, auto-deref in property access will break.
        //       see note in validate_ast():ND_PROPERTY_ACCESS:while(array.len() > 0):if(ty is ptr && ty.inner is struct).
        if(!validate_type(v, &field->data_type, false, &field->location)) // FIXME: use field.type_location
            continue;
        arrayInsert(&s->data_type->as.structure.field_types, i, (void *)field->data_type);
        // Note: The check for recursive structs is in typecheck_struct().
    }
    return true;
}

static bool validate_extern_fn(Validator *v, ASTObj *fn) {
    if(fn->as.extern_fn.source_attr == NULL) {
        error(v, fn->name_location, "Extern function '%s' has no 'source' attribute.", fn->name);
        return false;
    }
    if(fn->as.extern_fn.source_attr->type != ATTR_SOURCE) {
        error(v, fn->as.extern_fn.source_attr->location, "Invalid attribute '%s' for extern function.", attributeTypeString(fn->as.extern_fn.source_attr->type));
        return false;
    }
    return true;
}

static bool validate_object(Validator *v, ASTObj *obj) {
    if(obj->type == OBJ_FN || obj->type == OBJ_EXTERN_FN) {
        ASTObj *prev_decl = NULL;
        if(global_id_exists(v, obj, &prev_decl)) {
            error(v, obj->name_location, "Symbol '%s' already exists.", obj->name);
            hint(v, prev_decl->name_location, "Previous declaration was here.");
        }
    }

    switch(obj->type) {
        case OBJ_VAR:
            return validate_type(v, &obj->data_type, false, &obj->location); // FIXME: use obj.type_location
        case OBJ_FN: {
            return validate_function(v, obj);
        }
        case OBJ_STRUCT:
            return validate_struct(v, obj);
        case OBJ_EXTERN_FN: {
            return validate_extern_fn(v, obj);
        }
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
            break;
        case ND_ASSIGN:
            VERIFY(NODE_IS(AS_BINARY_NODE(g)->lhs, ND_VAR_DECL)); // If this fails, something is wrong with the parser.
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
    v->current_allocator = &m->ast_allocator.alloc;
    v->current_scope = m->module_scope;

    FOR(i, m->globals) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &m->globals, i);
        ASTObj *var = NODE_IS(g, ND_ASSIGN)
                       ? AS_OBJ_NODE(AS_BINARY_NODE(g)->lhs)->obj
                       : AS_OBJ_NODE(g)->obj;
        ASTObj *prev_decl = NULL;
        if(global_id_exists(v, var, &prev_decl)) {
            error(v, var->location, "Global symbol '%s' redefined.", var->name);
            hint(v, prev_decl->location, "Previous declaration was here.");
        }
        ASTNode *new_g = validate_global_variable(v, g);
        if(new_g) {
            arrayInsert(&m->globals, i, (void *)new_g);
        }
    }

    // objects
    FOR(i, m->module_scope->objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &m->module_scope->objects, i);
        validate_object(v, obj); // Note: no need to handle errors here as we want to validate all objects always.
    }
    v->current_allocator = NULL;
}

/** Typechecker **/

static bool typecheck_assignment(Validator *v, ASTNode *n) {
    TRY(typecheck_ast(v, AS_BINARY_NODE(n)->lhs));
    TRY(typecheck_ast(v, AS_BINARY_NODE(n)->rhs));
    Type *lhs_ty = get_expr_type(v, AS_BINARY_NODE(n)->lhs);
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
    if(AS_OBJ_NODE(decl)->obj->data_type == NULL) {
        error(v, decl->location, "Variable '%s' has no type.", AS_OBJ_NODE(decl)->obj->name);
        hint(v, AS_OBJ_NODE(decl)->obj->location, "Consider adding an explicit type here.");
        return false;
    } else if(AS_OBJ_NODE(decl)->obj->data_type->type == TY_VOID) {
        error(v, decl->location, "A variable cannot have the type 'void'.");
        return false;
    }
    return true;
}

static bool typecheck_ast(Validator *v, ASTNode *n) {
    switch(n->node_type) {
        case ND_VAR_DECL:
            TRY(typecheck_variable_declaration(v, n));
            break;
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
            if(IS_NUMERIC(lhs_ty) && IS_NUMERIC(rhs_ty)
               && IS_UNSIGNED(lhs_ty) &&
               NODE_IS(AS_BINARY_NODE(n)->rhs, ND_NUMBER_LITERAL) && IS_SIGNED(rhs_ty)) {
                break;
            }
            TRY(check_types(v, AS_BINARY_NODE(n)->rhs->location, lhs_ty, rhs_ty));
            break;
        }
        case ND_IF:
            TRY(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->condition));
            TRY(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->body));
            if(AS_CONDITIONAL_NODE(n)->else_) {
                TRY(typecheck_ast(v, AS_CONDITIONAL_NODE(n)->else_));
            }
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
                failed = !typecheck_ast(v, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(n)->nodes, i));
            }
            leave_scope(v);
            // failed == false -> success (return true).
            // failed == true -> failure (return false).
            return !failed;
        }
        case ND_ADDROF:
        case ND_DEREF:
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
        case ND_STRING_LITERAL:
        case ND_FUNCTION:
        case ND_VARIABLE:
        case ND_PROPERTY_ACCESS:
            break;
        case ND_ARGS: // see note in validate_ast() for the reason this node is an error here.
        case ND_IDENTIFIER: // see note in validate_ast() for the reason this node is an error here.
        case ND_DEFER: // 'defer's should be removed from the body by validate_function().
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

    enter_scope(v, fn->as.fn.body->scope);
    FOR(i, fn->as.fn.body->nodes) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &fn->as.fn.body->nodes, i);
        if(!typecheck_ast(v, n)) {
            had_error = true;
        }
    }
    leave_scope(v);
    // fn.defers is typechecked in validate_function().

    v->current_function = NULL;
    return !had_error;
}

static bool is_recursive_struct(Type *root_struct_type, Type *field_type) {
    if(field_type->type != TY_STRUCT) {
        return false;
    }
    FOR(i, field_type->as.structure.field_types) {
        Type *field = ARRAY_GET_AS(Type *, &field_type->as.structure.field_types, i);
        if(typeEqual(root_struct_type, field) || is_recursive_struct(root_struct_type, field)) {
            return true;
        }
    }
    return false;
}

static bool typecheck_struct(Validator *v, ASTObj *s) {
    bool had_error = false;
    Scope *scope = astModuleGetScope(astProgramGetModule(v->program, v->current_module), s->as.structure.scope);
    FOR(i, scope->objects) {
        ASTObj *field = ARRAY_GET_AS(ASTObj *, &scope->objects, i);
        VERIFY(field->type == OBJ_VAR);
        if(field->data_type == NULL) {
            error(v, field->location, "Field '%s' in struct '%s' has no type.", field->name, s->name);
            had_error = true;
            continue;
        } else if(field->data_type->type == TY_VOID) {
            error(v, field->location, "A field cannot have the type 'void'.");
            had_error = true;
            continue;
        }
        if(is_recursive_struct(s->data_type, field->data_type)) {
            error(v, field->location, "Struct '%s' cannot have a field that recursively contains it.", s->name);
            had_error = true;
        }
    }
    return !had_error; // true on success, false on failure.
}

static bool typecheck_object(Validator *v, ASTObj *obj) {
    switch(obj->type) {
        case OBJ_VAR:
        case OBJ_EXTERN_FN:
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
    v->current_scope = m->module_scope;

    // Global variables
    FOR(i, m->globals) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &m->globals, i);
        typecheck_global_variable(v, g);
    }

    // Objects
    FOR(i, m->module_scope->objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &m->module_scope->objects, i);
        typecheck_object(v, obj); // Note: no need to handle errors as we want to typecheck all objects always.
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
        errorInit(err, ERR_ERROR, false, EMPTY_LOCATION(), "No entry point (hint: Consider adding a 'main' function).");
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
