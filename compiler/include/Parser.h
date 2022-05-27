#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "Token.h"
#include "Scanner.h"
#include "ast.h"
#include "Symbols.h"

#define MAX_DECLS_IN_BLOCK 100
#define MAX_SCOPE_DEPTH 10

typedef struct scope {
    Array ids; // Array<int>
    struct scope *previous;
} Scope;

typedef struct parser {
    ASTProg *prog;
    SymTable *current_id_table;
    Scanner *scanner;
    Token current_token, previous_token;
    bool had_error, panic_mode;
    Scope *scopes;
    int scope_depth;
    ASTFunction *current_fn;
} Parser;

/***
 * Initialize a parser.
 * It is a checked runtime error to pass NULL as any of the arguments.
 *
 * @param p An uninitialized parser to initialize.
 * @param s A pointer to an initialized scanner to use for lexing.
 * @param prog A pointer to an initialized ASTProg to store the AST in.
 ***/
void initParser(Parser *p, Scanner *s, ASTProg *prog);

/***
 * Free a parsers resources.
 *
 * @param p A parser to free.
 **/
void freeParser(Parser *p);

/***
 * Parse all tokens in provided by the scanner passed to initParser()
 * and store the AST in the ASTProg provided to initParser().
 *
 * @param p An initialized parser.
 *
 * @return true on success, false on error.
 ***/
bool parserParse(Parser *p);

#endif // PARSER_H
