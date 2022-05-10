#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "Token.h"
#include "Scanner.h"
#include "ast.h"

typedef struct parser {
    Scanner *scanner;
    Token current_token, previous_token;
    bool had_error, panic_mode;
} Parser;

/***
 * 
 ***/
void initParser(Parser *p, Scanner *s);

/***
 * 
 **/
void freeParser(Parser *p);

/***
 * 
 ***/
bool parserParse(Parser *p, ASTProg *prog);

#endif // PARSER_H
