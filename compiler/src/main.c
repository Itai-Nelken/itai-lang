#include <stdio.h>
#include "Scanner.h"
#include "Token.h"

int main(int argc, char **argv) {
    if(argc < 2) return 1;
    Scanner s;
    initScanner(&s, "Test", argv[1]);
    Token t;
    while((t = nextToken(&s)).type != TK_EOF) {
        printToken(t);
    }
    freeScanner(&s);
    return 0;
}