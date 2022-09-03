#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdbool.h>
#include "Compiler.h"
#include "ast.h"

bool validateAndTypecheckProgram(Compiler *compiler, ASTProgram *prog);

#endif // VALIDATOR_H
