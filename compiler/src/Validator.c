#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Symbols.h"
#include "Compiler.h"
#include "ast.h"
#include "Error.h"
#include "Validator.h"

typedef struct validator_state {
    ModuleID current_module;
    bool had_error;
    Compiler *compiler;
    ASTProgram *program;
} ValidatorState;

// if a valid 'String' is provided as 'message', it will be freed.
static void error(ValidatorState *state, Location location, char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, true, location, message);
    if(stringIsValid(message)) {
        stringFree(message);
    }
    compilerAddError(state->compiler, err);
    state->had_error = true;
}

static ASTObj *find_global_in_current_module(ValidatorState *state, SymbolID name) {
    ASTModule *module = astProgramGetModule(state->program, state->current_module);
    for(usize i = 0; i < module->objects.used; ++i) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &module->objects, i);
        if(obj->type == OBJ_GLOBAL && obj->name->id == name) {
            return obj;
        }
    }
    return NULL;
}

static ASTObj *find_function_in_current_module(ValidatorState *state, SymbolID name) {
    ASTModule *module = astProgramGetModule(state->program, state->current_module);
    for(usize i = 0; i < module->objects.used; ++i) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &module->objects, i);
        if(obj->type == OBJ_FUNCTION && obj->name->id == name) {
            return obj;
        }
    }
    return NULL;
}

static inline String get_identifier(ValidatorState *s, SymbolID id) {
    String identifier = symbolTableGetIdentifier(&s->program->symbols, id);
    if(!identifier) {
        return "<invalid identifier>";
    }
    return identifier;
}

static const char *get_typename(ValidatorState *state, SymbolID ty) {
    DataType *type = symbolTableGetType(&state->program->symbols, ty);
    if(!type) {
        return "<invalid type>";
    }
    return get_identifier(state, type->name);
}

static SymbolID get_type_from_node(ValidatorState *state, ASTNode *node) {
    switch(node->type) {
        case ND_NUMBER:
            return astProgramGetPrimitiveType(state->program, TY_I32);
        case ND_VAR:
            return AS_OBJ_NODE(node)->obj->data_type;
        case ND_CHECKED_CALL:
            return AS_FUNCTION_OBJ(AS_OBJ_NODE(AS_UNARY_NODE(node)->operand)->obj)->return_type;
        case ND_CALL: /* fallthrough */
        default:
            break;
    }
    return EMPTY_SYMBOL_ID;
}

static SymbolID *unpack_lvalue_type_in_assignment(ASTNode *node) {
    ASTBinaryNode *assignment = AS_BINARY_NODE(node);
    ASTBinaryNode *lvalue = AS_BINARY_NODE(assignment->left);
    ASTObjNode *var = AS_OBJ_NODE(lvalue->right);
    return &var->obj->data_type;
}

static bool validate_call(ValidatorState *state, ASTNode *call) {
    VERIFY(call->type == ND_CALL);
    ASTUnaryNode *n = AS_UNARY_NODE(call);
    SymbolID fn_name = AS_IDENTIFIER_NODE(n->operand)->id->id;
    ASTObj *fn = find_function_in_current_module(state, fn_name);
    if(!fn) {
        error(state, n->header.location, stringFormat("Function '%s' doesn't exist!", get_identifier(state, fn_name)));
        return false;
    }
    n->header.type = ND_CHECKED_CALL;
    astFree(n->operand);
    n->operand = astNewObjNode(fn->location, fn);
    return true;
}

static void infer_types_in_assignment(ValidatorState *state, ASTNode *assignment) {
    VERIFY(assignment->type == ND_ASSIGN);
    if(!AS_BINARY_NODE(assignment)->right) {
        return;
    }
    // If the operand is a call, replace it with a CHECKED_CALL (which has the function object stored).
    if(AS_BINARY_NODE(assignment)->right->type == ND_CALL && !validate_call(state, AS_BINARY_NODE(assignment)->right)) {
        return;
    }

    SymbolID rvalue_type = get_type_from_node(state, AS_BINARY_NODE(assignment)->right);
    SymbolID *lvalue_type = unpack_lvalue_type_in_assignment(assignment);
    // The type of the variable will not be changed if it already exists
    // so type mismatches can be detected.
    if(*lvalue_type == EMPTY_SYMBOL_ID) {
        *lvalue_type = rvalue_type;
    }
}

static void set_global_obj_in_assignment(ValidatorState *state, ASTNode *node) {
    VERIFY(node->type == ND_ASSIGN);
    ASTBinaryNode *var_node = AS_BINARY_NODE(AS_BINARY_NODE(node)->left);
    ASTObj **obj = &AS_OBJ_NODE(var_node->right)->obj;
    if(*obj) {
        return;
    }
    ASTObj *global = find_global_in_current_module(state, AS_IDENTIFIER_NODE(var_node->left)->id->id);
    if(!global) {
        error(state, var_node->header.location, stringFormat("Variable '%s' doesn't exist in this scope!", get_identifier(state, AS_IDENTIFIER_NODE(var_node->left)->id->id)));
    }
    *obj = global;
}

static void validate_ast(ValidatorState *state, ASTNode *node) {
    switch(node->type) {
        case ND_EXPR_STMT:
            validate_ast(state, AS_UNARY_NODE(node)->operand);
            break;
        case ND_BLOCK:
            for(usize i = 0; i < AS_LIST_NODE(node)->body.used; ++i) {
                validate_ast(state, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(node)->body, i));
            }
            break;
        case ND_ASSIGN:
            set_global_obj_in_assignment(state, node);
            infer_types_in_assignment(state, node);
            break;
        case ND_CALL:
            validate_call(state, node);
            break;
        default:
            break;
    }
}

static void typecheck_ast(ValidatorState *state, ASTNode *node) {
    switch(node->type) {
        case ND_EXPR_STMT:
            typecheck_ast(state, AS_UNARY_NODE(node)->operand);
            break;
        case ND_BLOCK:
            for(usize i = 0; i < AS_LIST_NODE(node)->body.used; ++i) {
                typecheck_ast(state, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(node)->body, i));
            }
            break;
        case ND_ASSIGN: {
            SymbolID *lvalue_type = unpack_lvalue_type_in_assignment(node);
            SymbolID rvalue_type = get_type_from_node(state, AS_BINARY_NODE(node)->right);
            if(*lvalue_type != rvalue_type) {
                error(state, AS_BINARY_NODE(node)->right->location, stringFormat("Type mismatch: expected '%s' but got '%s'!", get_typename(state, *lvalue_type), get_typename(state, rvalue_type)));
            }
            break;
        }
        default:
            break;
    }
}

static void typecheck_variable(ValidatorState *state, ASTVariableObj *var) {
    UNUSED(state);
    if(var->header.data_type == EMPTY_SYMBOL_ID) {
        error(state, var->header.location, "Variable has no type! (hint: consider adding an explicit type).");
    }
}

static void validate_variable(ValidatorState *state, ASTVariableObj *var) {
    UNUSED(state);
    if(var->initializer && var->header.data_type == EMPTY_SYMBOL_ID) {
        var->header.data_type = get_type_from_node(state, var->initializer);
    }
}

static void validate_function(ValidatorState *state, ASTFunctionObj *fn) {
    // If function is main, set the entry point to it.
    if(state->current_module == state->program->root_module && stringEqual(get_identifier(state, fn->header.name->id), "main")) {
        state->program->entry_point = fn;
    }

    // locals
    for(usize i = 0; i < fn->locals.used; ++i) {
        ASTVariableObj *var = ARRAY_GET_AS(ASTVariableObj *, &fn->locals, i);
        validate_variable(state, var);
        typecheck_variable(state, var);
    }

    // body
    for(usize i = 0; i < fn->body->body.used; ++i) {
        ASTNode *node = ARRAY_GET_AS(ASTNode *, &fn->body->body, i);
        validate_ast(state, node);
        typecheck_ast(state, node);
    }

    // TODO: check that returned value types against function return type.
}

static void validate_object_callback(void *object, void *state) {
    ASTObj *obj = AS_OBJ(object);
    switch(obj->type) {
        case OBJ_FUNCTION:
            validate_function((ValidatorState *)state, AS_FUNCTION_OBJ(obj));
            break;
        case OBJ_GLOBAL:
            validate_variable((ValidatorState *)state, AS_VARIABLE_OBJ(obj));
            typecheck_variable((ValidatorState *)state, AS_VARIABLE_OBJ(obj));
            break;
        default:
            UNREACHABLE();
    }
}

static void validate_module_callback(void *module, void *state) {
    ASTModule *mod = (ASTModule *)module;

    arrayMap(&mod->objects, validate_object_callback, state);
    ((ValidatorState *)state)->current_module++; // assumes that the objects are stored in an Array (so sequentially).
}


bool validateAndTypecheckProgram(Compiler *compiler, ASTProgram *prog) {
    // For every module in the program, do:
    // - For every object in the module, do:
    //   1) If object is a global, do:
    //      * Infer type if neccesary.
    //      * Typecheck.
    //   2) If the object is a funtion, do:
    //      * If in the root module, check for main() and set prog.entry_point to it.
    //      * For every node in the body, do:
    //        - Infer types in neccesary.
    //        - Check that all referenced global symbols exist.
    //        - Typecheck.

    ValidatorState state = {
        .current_module = 0,
        .had_error = false,
        .compiler = compiler,
        .program = prog
    };
    arrayMap(&prog->modules, validate_module_callback, (void *)&state);
    if(!prog->entry_point) {
        Error *err;
        NEW0(err);
        errorInit(err, ERR_ERROR, false, locationNew(0, 0, 0), "No entry point! (Hint: Add a 'main' function).");
        compilerAddError(state.compiler, err);
        return false;
    }
    return !state.had_error;
}
