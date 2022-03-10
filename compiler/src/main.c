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

static int interpret(ASTNode *node) {
	if(node->type == ND_NUM) {
		return node->literal.int32;
	}

	int value = interpret(node->left);
	switch(node->type) {
		case ND_ADD:
			value = value + interpret(node->right);
			break;
		case ND_SUB:
			value = value - interpret(node->right);
			break;
		case ND_MUL:
			value = value * interpret(node->right);
			break;
		case ND_DIV:
			value = value / interpret(node->right);
			break;
		case ND_NEG:
			value = -value;
			break;
		default:
			UNREACHABLE();
	}
	return value;
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