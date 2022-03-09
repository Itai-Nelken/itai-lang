#include "ast.h"
#include "Token.h"
#include "Scanner.h"
#include "Parser.h"

void initParser(Parser *p, const char *filename, char *source) {
    p->current_expr = NULL;
    initScanner(&p->scanner, filename, source);
}

void freeParser(Parser *p) {
    freeScanner(&p->scanner);
    if(p->current_expr != NULL) {
        freeAST(p->current_expr);
    }
}

ASTNode *parse(Parser *p) {
    
}
