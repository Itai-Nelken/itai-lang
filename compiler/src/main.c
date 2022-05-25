#include <stdio.h>
#include "common.h"
#include "Token.h"
#include "Scanner.h"
#include "ast.h"
#include "Parser.h"
#include "Validator.h"

// NOTE: ASTProgs have to be freed before the parser
//       as the parser also frees the scanner which makes
//       the Locations in the AST nodes invalid causing segfaults.

int main(int argc, char **argv) {
	if(argc < 2) {
        fprintf(stderr, "\x1b[1mUSAGE:\x1b[0m %s [stmt]\n", argv[0]);
        return 1;
    }

	Scanner s;
	Parser p;
	ASTProg prog;
	initASTProg(&prog);
	initScanner(&s, "Test", argv[1]);
	initParser(&p, &s, &prog);
	if(!parserParse(&p)) {
		fputs("Parsing failed!\n", stderr);
		return 1;
	}
	
	if(!validate(&prog)) {
		fputs("Validating failed!\n", stderr);
		goto end;
	}

	printf("OK\n");

end:
	freeASTProg(&prog);
	freeParser(&p);
	freeScanner(&s);

	//Parser p;
	//CodeGenerator cg;
	//ASTProg program;
	
	// initialize the parser (and scanner (lexer))
	//initParser(&p, "Test", argv[1]);
	// initialize the AST program
	//initASTProg(&program);
	// step 1 + 2: scan (lex) the source code, and parse it into an AST
	// this is also the first pass of error and warning reporting
	//if(!parse(&p, &program)) {
	//	fputs("Parsing failed!\n", stderr);
	//	freeASTProg(&program);
	//	freeParser(&p);
	//	return 1;
	//}

	// step 3: validate the AST to make sure its valid
	// this is the 2nd pass
	//if(!validate(&program)) {
	//	fputs("Validating failed!\n", stderr);
	//	freeASTProg(&program);
	//	freeParser(&p);
	//	return 1;
	//}
	
	// initialize the code generator
	//initCodegen(&cg, &program, stdout);
	// step 4: walk the AST and generate assembly
	// this is also the 3rd pass
	//codegen(&cg);

	// free all resources
	//freeCodegen(&cg);
	//freeASTProg(&program);
	//freeParser(&p);

    return 0;
}
