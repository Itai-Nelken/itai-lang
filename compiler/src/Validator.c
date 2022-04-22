#include <stdio.h>
#include <stdarg.h>
#include <stddef.h> // size_t
#include <stdbool.h>
#include <assert.h>
#include "common.h"
#include "Errors.h"
#include "Strings.h"
#include "Table.h"
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

static bool validate_unary(ASTNode *n) {
    if(n->left == NULL) {
        return false;
    }
    return true;
}

static bool validate_binary(ASTNode *n) {
    if(n->left == NULL || n->right == NULL) {
        return false;
    }
    return true;
}

static void validate_ast(ASTNode *node, bool *had_error) {
    switch(node->type) {
        // check that unary nodes have a valid operand
        case ND_EXPR_STMT:
        case ND_NEG:
        case ND_PRINT:
        case ND_RETURN:
            *had_error = !validate_unary(node);
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
            *had_error = !validate_binary(node);
            break;
        // everything else isn't handled yet
        default:
            break;
    }
}

static bool global_exists(ValidatorState *state, char *name) {
    for(size_t i = 0; i < state->prog->globals.used; ++i) {
        // FIXME: assumes that the ND_EXPR_STMT is always valid.
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
    if(!validate_unary(g)) {
        error(state, g->loc, "Invalid ND_EXPR_STMT!");
        return;
    }
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

static void validate_function(void *function, void *cl) {
    ASTFunction *fn = (ASTFunction *)function;
    ValidatorState *state = (ValidatorState *)cl;

    arrayMap(&fn->locals, validate_local, cl);
    validate_ast(fn->body, &state->had_error);
}

static void validate_global_identifiers(ValidatorState *state) {
    bool found_main = false;
    Table found;
    initTable(&found, NULL, NULL);

    // add function names
    for(size_t i = 0; i < state->prog->functions.used; ++i) {
        ASTFunction *fn = ARRAY_GET_AS(ASTFunction *, &state->prog->functions, i);
        if(stringEqual(fn->name, "main")) {
            found_main = true;
        }
        if(tableGet(&found, fn->name) != NULL) {
            error(state, fn->location, "Redefinition of function '%s'", fn->name);
        } else {
            tableSet(&found, fn->name, (void *)0);
        }
    }

    if(!found_main) {
        fprintf(stderr, "\x1b[1;31mERROR:\x1b[0;1m function main isn't defined!\x1b[0m\n");
        state->had_error = true;
    }

    // add global variable names
    for(size_t i = 0; i < state->prog->globals.used; ++i) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &state->prog->globals, i)->left;
        Location loc;
        char *name;
        if(g->type == ND_ASSIGN) {
            loc = g->left->loc;
            name = g->left->as.var.name;
        } else {
            assert(g->type == ND_VAR);
            loc = g->loc;
            name = g->as.var.name;
        }
        TableItem *item = tableGet(&found, name);
        if(item != NULL) {
            error(state, loc, "Redefinition of %s '%s'", (item->value == 0 ? "function" : "global variable"), name);
        } else {
            tableSet(&found, name, (void *)1);
        }
    }
    freeTable(&found);
}

bool validate(ASTProg *prog) {
    ValidatorState state = {
        .had_error = false,
        .prog = prog
    };
    arrayMap(&prog->globals, validate_global, (void *)&state);
    arrayMap(&prog->functions, validate_function, (void *)&state.had_error);
    validate_global_identifiers(&state);
    return !state.had_error;
}
