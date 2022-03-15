#include <stdio.h>
#include "common.h"
#include "Token.h"
#include "Scanner.h"
#include "Parser.h"
#include "codegen.h"

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
		case ND_EQ:
			value = value == interpret(node->right);
			break;
		case ND_NE:
			value = value != interpret(node->right);
			break;
		case ND_GT:
			value = value > interpret(node->right);
			break;
		case ND_GE:
			value = value >= interpret(node->right);
			break;
		case ND_LT:
			value = value < interpret(node->right);
			break;
		case ND_LE:
			value = value <= interpret(node->right);
			break;
		case ND_BIT_OR:
			value = value | interpret(node->right);
			break;
		case ND_XOR:
			value = value ^ interpret(node->right);
			break;
		case ND_BIT_AND:
			value = value & interpret(node->right);
			break;
		default:
			UNREACHABLE();
	}
	return value;
}

static void test_parser(char *source) {
	Parser p;
    initParser(&p, "Test", source);
    ASTProg prog = parse(&p);
    freeParser(&p);
	ASTNode *n = prog.expr;

    if(n == NULL) {
        fputs("Parsing failed!\n", stderr);
    } else {
        printf("Result: %d\n", interpret(n));
    }

    freeAST(n);
}

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "\x1b[1mUSAGE:\x1b[0m %s [expr]\n", argv[0]);
        return 1;
    }
	Parser p;
	CodeGenerator cg;
	
	// initialize the parser (and scanner (lexer))
	initParser(&p, "Test", argv[1]);
	// step 1 + 2: scan (lex) the source code, and parse it into an AST
	ASTProg prog = parse(&p);
	if(p.had_error) {
		fputs("Parsing failed!\n", stderr);
		freeParser(&p);
		return 1;
	}
	
	// initialize the code generator
	initCodegen(&cg, prog, stdout);
	// step 3: walk the AST and generate assembly
	codegen(&cg);

	// free all resources
	freeCodegen(&cg);
	freeASTProg(&prog);
	freeParser(&p);

    return 0;
}