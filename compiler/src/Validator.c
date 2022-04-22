#include <stdarg.h>
#include <stddef.h> // size_t
#include <stdbool.h>
#include <assert.h>
#include "common.h"
#include "Errors.h"
#include "Strings.h"
#include "Token.h" // Location
#include "ast.h"
#include "Validator.h"

typedef struct validator_state {
    bool had_error;
    ASTProg *prog;
} ValidatorState;

static void error(ValidatorState *state, Location loc, const char *format, ...) {
    state->had_error = true;
    va_list ap;
    va_start(ap, format);
    vprintErrorF(ERR_ERROR, loc, format, ap);
    va_end(ap);
}

static bool global_exists(ValidatorState *state, char *name) {
    for(size_t i = 0; i < state->prog->globals.used; ++i) {
        // FIXME: assumes that always is ND_EXPR_STMT (true), that is valid.
        ASTNode *global = ARRAY_GET_AS(ASTNode *, &state->prog->globals, i)->left;
        char *global_name;
        if(global->type == ND_ASSIGN) {
            global_name = global->left->as.var.name;
        } else {
            assert(global->type == ND_VAR);
            global_name = global->as.var.name;
        }
        if(stringEqual(global_name, name)) {
            return true;
        }
    }
    return false;
}

static void validate_global(void *global, void *cl) {
    ASTNode *g = (ASTNode *)global;
    ValidatorState *state = (ValidatorState *)cl;

    assert(g->type == ND_EXPR_STMT);
    assert(g->left); // FIXME: validate_unary()
    g = g->left;

    if(g->type == ND_ASSIGN && (g->left == NULL || g->right == NULL)) {
        error(state, g->loc, "ND_ASSIGN with no left or right children");
        return;
    }
    ASTObjType type;
    char *name;
    if(g->type == ND_ASSIGN) {
        type = g->left->as.var.type;
        name = g->left->as.var.name;
    } else {
        assert(g->type == ND_VAR);
        type = g->as.var.type;
        name = g->as.var.name;
    }

    if(type != OBJ_GLOBAL) {
        error(state, g->loc, "global variable with type that isn't OBJ_GLOBAL");
    }

    if(!global_exists(state, name)) {
        error(state, g->loc, "Undeclared global variable '%s'", name);
    }
}

static void validate_local(void *local, void *cl) {
    ASTObj *l = (ASTObj *)local;
    ValidatorState *state = (ValidatorState *)cl;

    if(l->type != OBJ_LOCAL) {
        state->had_error = true;
    }
}

static bool validate_unary(ASTNode *n) {
    if(n->left == NULL) {
        return true;
    }
    return false;
}

static bool validate_binary(ASTNode *n) {
    if(n->left == NULL || n->right == NULL) {
        return true;
    }
    return false;
}

static void validate_ast(ASTNode *node, bool *had_error) {
    switch(node->type) {
        // check that unary nodes have a valid operand
        case ND_EXPR_STMT:
        case ND_NEG:
        case ND_PRINT:
        case ND_RETURN:
            *had_error = validate_unary(node);
            break;
        // check that binary nodes have valid operands
        case ND_ASSIGN:
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_REM:
        case ND_EQ:
        case ND_NE:
        case ND_GT:
        case ND_GE:
        case ND_LT:
        case ND_LE:
        case ND_BIT_OR:
        case ND_XOR:
        case ND_BIT_AND:
        case ND_BIT_RSHIFT:
        case ND_BIT_LSHIFT:
            *had_error = validate_binary(node);
            break;
        // everything else isn't handled yet
        default:
            break;
    }
}

static void validate_function(void *function, void *cl) {
    ASTFunction *fn = (ASTFunction *)function;
    ValidatorState *state = (ValidatorState *)cl;

    arrayMap(&fn->locals, validate_local, cl);
    validate_ast(fn->body, &state->had_error);
}

//  == TODO ==
// * check that fn main() exists
// * check that no global variable & function names clash
// * check that there are no undeclared globals used.
bool validate(ASTProg *prog) {
    ValidatorState state = {
        .had_error = false,
        .prog = prog
    };
    arrayMap(&prog->globals, validate_global, (void *)&state);
    arrayMap(&prog->functions, validate_function, (void *)&state.had_error);
    return !state.had_error;
}
