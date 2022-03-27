#include <stdio.h>
#include "common.h"
#include "Token.h"
#include "Scanner.h"
#include "Parser.h"
#include "codegen.h"

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "\x1b[1mUSAGE:\x1b[0m %s [expr]\n", argv[0]);
        return 1;
    }

	Parser p;
	CodeGenerator cg;
	ASTProg program;
	
	// initialize the parser (and scanner (lexer))
	initParser(&p, "Test", argv[1]);
	// initialize the AST program
	initASTProg(&program);
	// step 1 + 2: scan (lex) the source code, and parse it into an AST
	if(!parse(&p, &program)) {
		fputs("Parsing failed!\n", stderr);
		freeParser(&p);
		return 1;
	}
	
	// initialize the code generator
	initCodegen(&cg, &program, stdout);
	// step 3: walk the AST and generate assembly
	codegen(&cg);

	// free all resources
	freeCodegen(&cg);
	freeASTProg(&program);
	freeParser(&p);

    return 0;
}