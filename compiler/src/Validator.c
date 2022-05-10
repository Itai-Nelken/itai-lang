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

//static void error(ValidatorState *state, Location loc, const char *format, ...) {
//    state->had_error = true;
//    va_list ap;
//    va_start(ap, format);
//    vprintErrorF(ERR_ERROR, loc, format, ap);
//    va_end(ap);
//}

//static bool validate_unary(ASTNode *n) {
//    if(AS_UNARY_NODE(n)->child == NULL) {
//        return false;
//    }
//    return true;
//}

//static bool validate_binary(ASTNode *node) {
//    ASTBinaryNode *n = AS_BINARY_NODE(node);
//    if(n->left == NULL || n->right == NULL) {
//        return false;
//    }
//    return true;
//}

//static void validate_ast(ASTNode *node, bool *had_error) {
//    switch(node->type) {
//        // check that unary nodes have a valid operand
//        case ND_NEG:
//            *had_error = !validate_unary(node);
//            break;
//        // check that binary nodes have valid operands
//        case ND_ADD:
//        case ND_SUB:
//        case ND_MUL:
//        case ND_DIV:
//        case ND_REM:
//        case ND_EQ:
//        case ND_NE:
//        case ND_GT:
//        case ND_GE:
//        case ND_LT:
//        case ND_LE:
//        case ND_BIT_OR:
//        case ND_XOR:
//        case ND_BIT_AND:
//        case ND_BIT_RSHIFT:
//        case ND_BIT_LSHIFT:
//            *had_error = !validate_binary(node);
//            break;
//        // everything else isn't handled yet
//        default:
//            break;
//    }
//}

//static bool global_exists(ValidatorState *state, char *name) {
//}

//static void validate_global(void *global, void *cl) {
//    // check that all globals are declared
//}

//static void validate_local(void *local, void *cl) {
//
//}

//static void validate_function(void *function, void *cl) {
//
//}

//static void validate_global_identifiers(ValidatorState *state) {
//    bool found_main = false;
//    Table found;
//    initTable(&found, NULL, NULL);
//
//    // add function names
//    // 1) get function
//    // 2) if name == "main", set found_main to true
//    // 3) if name already exists in Table found, error redefinition of function.
//    //    else add the name to Table with value 0.
//
//    if(!found_main) {
//        fprintf(stderr, "\x1b[1;31mERROR:\x1b[0;1m function main isn't defined!\x1b[0m\n");
//        state->had_error = true;
//    }
//
//    // add global variable names
//    // error on redefinition of (function | global variable) name.
//    // add names to table with value 1.
//
//    freeTable(&found);
//}

bool validate(ASTProg *prog) {
    ValidatorState state = {
        .had_error = false,
        .prog = prog
    };
    //arrayMap(&prog->globals, validate_global, (void *)&state);
    //arrayMap(&prog->functions, validate_function, (void *)&state.had_error);
    //validate_global_identifiers(&state);
    return !state.had_error;
}
