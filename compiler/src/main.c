#include <stdio.h>
#include "common.h"
#include "Token.h"
#include "Scanner.h"
#include "Parser.h"

static void test_scanner(char *source) {
    Scanner s;
    initScanner(&s, "Test", source);
    Token t;
    while((t = nextToken(&s)).type != TK_EOF) {
        printToken(t);
    }
    freeScanner(&s);
}

static int interpret(ASTNode *n) {
    if(n->type == ND_NUM) {
        return n->literal.int32;
    }

    int left = interpret(n->left);
    int right = interpret(n->right);
    switch(n->type) {
        case ND_ADD: return left + right;
        case ND_SUB: return left - right;
        case ND_MUL: return left * right;
        case ND_DIV: return left / right;
        default:
            UNREACHABLE();
    }
}

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "\x1b[1mUSAGE:\x1b[0m %s [expr]\n", argv[0]);
        return 1;
    }
    Parser p;
    initParser(&p, "Test", argv[1]);
    ASTNode *n = parse(&p);
    freeParser(&p);

    if(n == NULL) {
        fputs("Parsing failed!\n", stderr);
    } else {
        printf("Result: %d\n", interpret(n));
    }

    freeAST(n);
    return 0;
}