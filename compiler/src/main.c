#include <stdio.h>
#include "Scanner.h"
#include "Token.h"

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "\x1b[1mUSAGE:\x1b[0m %s [str]\n", argv[0]);
        return 1;
    }
    Scanner s;
    initScanner(&s, "Test", argv[1]);
    Token t;
    while((t = nextToken(&s)).type != TK_EOF) {
        printToken(t);
    }
    freeScanner(&s);
    return 0;
}