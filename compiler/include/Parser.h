#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "Token.h"
#include "Ast.h"
#include "Scanner.h"

typedef struct parser {
    // input/output/errors
    Compiler *compiler;
    Scanner *scanner;
    ASTProgram *program;

    // state
    Token previous_token, current_token;
    bool had_error;
} Parser;

/***
 * Initialize a parser.
 *
 * @param p The parser to initialize.
 * @param s A Scanner.
 ***/
void parserInit(Parser *p, Scanner *s, Compiler *c);

/***
 * Free a parser.
 *
 * @param p The parser to free.
 ***/
void parserFree(Parser *p);

/***
 * Parse.
 *
 * @param p A Parser.
 * @param prog An ASTProgram.
 * @return true on success or false on failure.
 ***/
bool parserParse(Parser *p, ASTProgram *prog);

#endif // PARSER_H
