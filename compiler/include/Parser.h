#ifndef PARSER_H
#define PARSER_H

#include "Token.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Symbols.h"
#include "ast.h"

typedef struct parser {
    Compiler *compiler;
    Scanner *scanner;
    SymbolTable *current_symbol_table;
    Token previous_token, current_token;
} Parser;

/***
 * Initialize a Parser.
 *
 * @param p The Parser to initialize.
 * @param c A pointer to an initialized Compiler.
 ***/
void parserInit(Parser *p, Compiler *c);

/***
 * Free a Parser.
 *
 * @param p The Parser to free.
 ***/
void parserFree(Parser *p);

/***
 * Parse an AST from the tokens from 's'.
 *
 * @param p An initialized Parser.
 * @param s An initialized Scanner.
 * @param prog A pointer to an initialized AST program.
 * @return true on success, false on failure.
 ***/
bool parserParse(Parser *p, Scanner *s, ASTProgram *prog);

#endif // PARSER_H
