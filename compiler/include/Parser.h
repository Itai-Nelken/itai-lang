#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "Token.h"
#include "Scanner.h"

typedef struct parser {
    Scanner scanner;
    Token current_token, previous_token;
    bool had_error;
    ASTNode *current_expr;
} Parser;

void initParser(Parser *p, const char *filename, char *source);
void freeParser(Parser *p);

ASTProg parse(Parser *p);

#endif // PARSER_H
