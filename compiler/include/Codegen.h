#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "Ast.h"

bool codegenGenerate(FILE *output, ASTProgram *prog);

#endif // CODEGEN_H
