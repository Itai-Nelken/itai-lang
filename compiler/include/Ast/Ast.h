#ifndef AST_H
#define AST_H

/**
 * This is a utility header that includes all the different AST related headers.
 * It also declares a few utility macros.
 **/

#include "ExprNode.h"
#include "StmtNode.h"
#include "Object.h"
#include "Scope.h"
#include "Module.h"
#include "Program.h"
#include "Type.h"
#include "StringTable.h"


/* Utility macros */

#define NODE_AS(type, node) ((type *)node)
#define NODE_IS(node, check_type) ((node)->type == (check_type))

// TODO: Think through and draw a diagram of how I want my AST to look.
//       Things to keep in mind:
//         * Scope: separation between "namespace" and block scope? - No. they are similar enough that separating doesn't make sense (only difference is that block scope can't have structs.)
//            - In any case, holds all identifiers and their linked objects (functions, vars etc).
//         * Is separation between parsed and checked AST necessary? Future validator might do direct translation to bytecode. - No. this just complicates things.
//               Solution - have checkedAST methods (in validator?) that compute information that a checkedAST node would contain.
//            - Speaking of, consider converting AST to bytecode in validator. simplifying AST instead of complicating it makes sense.
//         * Try to keep as simple as possible!! (see bytecode idea).


#endif // AST_H
