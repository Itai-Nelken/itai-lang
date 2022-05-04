#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "ast.h"
#include "Token.h"
#include "Scanner.h"
#include "Array.h"

typedef struct scope {
    struct scope *previous;
    Array locals; // Array<char *>
    int depth;
} Scope;

typedef struct parser {
    Scanner scanner;
    Token current_token, previous_token;
    bool had_error, panic_mode;
    ASTNode *current_expr;
    ASTFunction *current_fn;

    Scope *scopes;
    int scope_depth;
} Parser;

void initParser(Parser *p, const char *filename, char *source);
void freeParser(Parser *p);

/***
 * Parse source code into an AST program
 * @param p An initialized parser
 * @param prog An initialized program.
 * @return true of parsing succeeded, false if not.
 ***/
bool parse(Parser *p, ASTProg *prog);

#endif // PARSER_H
