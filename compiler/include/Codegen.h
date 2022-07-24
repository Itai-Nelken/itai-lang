#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "ast.h"
#include "Strings.h"

// 256 is a random number that has no meaning at all.
// it just seemed like a good initial size.
#define CODEGEN_BUFFER_INITIAL_SIZE 256

typedef struct code_generator {
    ASTProg *prog;
    char *output_buffer;
    size_t output_buffer_size;
    FILE *output;
} Codegenerator;

void initCodegen(Codegenerator *cg, ASTProg *prog);

void freeCodegen(Codegenerator *cg);

bool codegen(Codegenerator *cg);

#endif // CODEGEN_H
