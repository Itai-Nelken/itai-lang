#ifndef C_CODEGEN_H
#define C_CODEGEN_H

#include <stdbool.h>
#include "ast.h"

bool c_codegen(ASTProgram *prog);

#endif // C_CODEGEN_H
