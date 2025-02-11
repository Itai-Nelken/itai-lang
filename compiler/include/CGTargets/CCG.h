#ifndef TARGET_CCG_H
#define TARGET_CCG_H

/**
 * CCG - C CodeGen
 * ===============
 **/

#include <stdio.h>
#include "Ast/Program.h"
#include "Codegen.h"

typedef struct target_ccg {
    FILE *output;
    Table declaredFunctions; // Table<ASTString, CCGFNState> - CCGFNState defined in CCG.c
} TargetCCG;

void targetCCGInit(TargetCCG *ccg, FILE *output);

void targetCCGFree(TargetCCG *ccg);

/**
 * Make and return the CGInterface for CCG.
 *
 * @param ccg A TargetCCG to use.
 * @return A CGInterface for 'ccg'.
 **/
CGInterface makeTargetCCGInterface(TargetCCG *ccg);

/**
 * Generate code using a TargetCCG.
 **/
void targetCCGGenerate(TargetCCG *ccg, ASTProgram *prog);

#endif // TARGET_CCG_H
