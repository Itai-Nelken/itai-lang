#include <stdbool.h>
#include <assert.h>
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
        case ND_IDENTIFIER:
            // TODO: resolve variable types.
        default:
            break;
    }
    return EMPTY_SYMBOL_ID;
}

static void infer_types_in_assignment(ValidatorState *state, ASTNode *assignment) {
    assert(assignment->type == ND_ASSIGN);
    ASTIdentifierNode *lvalue = AS_IDENTIFIER_NODE(AS_BINARY_NODE(assignment)->left);
    SymbolID type_id = get_type_from_node(state, AS_BINARY_NODE(assignment)->right);
    if(type_id != EMPTY_SYMBOL_ID) {
        lvalue->data_type = type_id;
    }
}

static void validate_ast(ValidatorState *state, ASTNode *node) {
    switch(node->type) {
        case ND_ASSIGN:
            infer_types_in_assignment(state, node);
            break;
        default:
            break;
    }
}

static void typecheck_ast(ValidatorState *state, ASTNode *node) {
    if(!node) {
        return;
    }
    switch(node->type) {
        case ND_ASSIGN: {
            SymbolID lvalue_type = AS_IDENTIFIER_NODE(AS_BINARY_NODE(node)->left)->data_type;
            SymbolID rvalue_type = get_type_from_node(state, AS_BINARY_NODE(node)->right);
            if(lvalue_type != rvalue_type) {
                error(state, node->location,
                    stringFormat("Mismatched types (expected '%s' but got '%s')!",
                    get_typename(state, lvalue_type),
                    get_typename(state, rvalue_type)));
            }
            break;
        }
        default:
            break;
    }
}

static void typecheck_variable(ValidatorState *state, ASTVariableObj *var) {
    SymbolID ty = var->header.data_type;
    if(ty == EMPTY_SYMBOL_ID) {
        error(state, var->header.name->location,
            stringFormat("Variable '%s' has no type!",
            get_identifier(state, var->header.name->id)));
    } else if(var->initializer && (ty != get_type_from_node(state, var->initializer))) {
        error(state, locationMerge(var->header.location, var->initializer->location),
            stringFormat("Mismatched types (expected '%s' but got '%s')!",
            get_typename(state, var->header.data_type),
            get_typename(state, get_type_from_node(state, var->initializer))));
    }
}

static void validate_variable(ValidatorState *state, ASTVariableObj *var) {
    if(var->header.data_type == EMPTY_SYMBOL_ID && var->initializer) {
        var->header.data_type = get_type_from_node(state, var->initializer);
    }
    typecheck_variable(state, var);
}

static void validate_function(ValidatorState *state, ASTFunctionObj *fn) {
    if(state->current_module == state->program->root_module && stringEqual(get_identifier(state, fn->header.name->id), "main")) {
        state->program->entry_point = fn;
    }
    for(usize i = 0; i < fn->locals.used; ++i) {
        ASTVariableObj *var = ARRAY_GET_AS(ASTVariableObj *, &fn->locals, i);
        validate_variable(state, var);
    }
    for(usize i = 0; i < fn->body->body.used; ++i) {
        ASTNode *node = ARRAY_GET_AS(ASTNode *, &fn->body->body, i);
        validate_ast(state, node);
        typecheck_ast(state, node);
    }
}

static void validate_object_callback(void *object, void *state) {
    ASTObj *obj = AS_OBJ(object);
    switch(obj->type) {
        case OBJ_FUNCTION:
            validate_function((ValidatorState *)state, AS_FUNCTION_OBJ(obj));
            break;
        case OBJ_GLOBAL:
            validate_variable((ValidatorState *)state, AS_VARIABLE_OBJ(obj));
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
    return !state.had_error;
}
