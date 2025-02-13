#ifndef CODEGEN_H
#define CODEGEN_H

/**
 * This is a "temporary" C code generator (transpiler.)
 * It directly walks the AST and outputs equivalent C code.
 *
 * I say its temporary since the final goal is to have a generic front-end
 * for the code generator that either generates a bytecode, or requires
 * back-ends (targets) to provide callbacks as required to generate code.
 * Both ideas could also be potentially combined.
 * See branch 'generic_cg_backend' for a beginning of an implementation
 * of a generic front-end and a specific back-end (in this case a C transpiler).
 *
 * The reason I'm making this temporary transpiler is that I'm more interested in
 * bringing the language to a usable state right now. Eventually, as I go back
 * to my long-term goal of having an assembly code generator, I'll start working
 * on a generic back-end/bytecode, port this transpiler to it, and then start work
 * on the assembly CG.
 **/

#include <stdio.h> // FILE
#include "Ast/Program.h"

/**
 * Transpile program represented by 'prog' to C code.
 *
 * @param output The stream to output the C code to.
 * @param prog The ASTProgram to transpile from.
 **/
void codegenGenerate(FILE *output, ASTProgram *prog);


#endif // CODEGEN_H
