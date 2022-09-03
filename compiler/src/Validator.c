#include <stdbool.h>
#include "common.h"
#include "Array.h"
#include "Strings.h"
#include "Symbols.h"
#include "Compiler.h"
#include "ast.h"
#include "Validator.h"

typedef struct validator_state {
    ModuleID current_module;
    bool had_error;
    Compiler *compiler;
    ASTProgram *program;
} ValidatorState;

static inline String get_identifier(ValidatorState *s, SymbolID id) {
    // TODO: handle NULL returned.
    return symbolTableGetIdentifier(&s->program->symbols, id);
}

static void infer_types_in_node(ASTNode *node, ValidatorState *state) {
    UNUSED(node);
    UNUSED(state);
}

static void validate_ast(ASTNode *node, ValidatorState *state) {
    UNUSED(node);
    UNUSED(state);
}

static void typecheck_ast(ASTNode *node, ValidatorState *state) {
    UNUSED(node);
    UNUSED(state);
}

static void validate_function(ASTFunctionObj *fn, ValidatorState *state) {
    if(state->current_module == state->program->root_module && stringEqual(get_identifier(state, fn->header.name->id), "main")) {
        state->program->entry_point = fn;
    }
    for(usize i = 0; i < fn->body->body.used; ++i) {
        ASTNode *node = ARRAY_GET_AS(ASTNode *, &fn->body->body, i);
        infer_types_in_node(node, state);
        validate_ast(node, state);
        typecheck_ast(node, state);
    }
}

static void validate_object_callback(void *object, void *state) {
    ASTObj *obj = AS_OBJ(object);
    switch(obj->type) {
        case OBJ_FUNCTION:
            validate_function(AS_FUNCTION_OBJ(obj), (ValidatorState *)state);
            break;
        default:
            UNREACHABLE();
    }
}

static void validate_module_callback(void *module, void *state) {
    ASTModule *mod = (ASTModule *)module;

    arrayMap(&mod->objects, validate_object_callback, state);
    ((ValidatorState *)state)->current_module++; // assumes that the objects are stored in an Array (so sequencially).
}


bool validateAndTypecheckProgram(Compiler *compiler, ASTProgram *prog) {
    // For every module in the program, do:
    // - For every object in the module, do:
    //   1) If object is a global, infer types.
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
