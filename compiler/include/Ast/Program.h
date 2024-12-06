#ifndef AST_PROGRAM_H
#define AST_PROGRAM_H

#include "Array.h"

/**
 * A Program represents a complete AST tree for a program including modules, scopes, objects etc.
 * Effectively, it stores an Array of Modules, and owns all of them.
 **/
typedef struct ast_program {
    Array modules; // Array<ASTModule *>;
} ASTProgram;


#endif // AST_PROGRAM_H
