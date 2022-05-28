#include <stdbool.h>
#include "ast.h"
#include "Validator.h"

typedef struct validator_state {
    bool had_error;
    ASTProg *prog;
} ValidatorState;

bool validate(ASTProg *prog) {
    ValidatorState state = {
        .had_error = false,
        .prog = prog
    };

    return !state.had_error;
}
