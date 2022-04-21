#include <stdio.h>
#include "common.h"
#include "Token.h"
#include "Scanner.h"
#include "Parser.h"
#include "Validator.h"

int main(int argc, char **argv) {
	if(argc < 2) {
        fprintf(stderr, "\x1b[1mUSAGE:\x1b[0m %s [stmt]\n", argv[0]);
        return 1;
    }

	Parser p;
	ASTProg program;
	
	// initialize the parser (and scanner (lexer))
	initParser(&p, "Test", argv[1]);
	// initialize the AST program
	initASTProg(&program);

	// step 1 + 2: scan (lex) the source code, and parse it into an AST
	if(!parse(&p, &program)) {
		fputs("Parsing failed!\n", stderr);
		freeParser(&p);
		freeASTProg(&program);
		return 1;
	}

	// step 3: make sure the AST is valid
	if(!validate(&program)) {
		fputs("Validating failed!\n", stderr);
		freeParser(&p);
		freeASTProg(&program);
		return 1;
	}
	
	// step 4: generate IR (intermidiate representation) bytecode from the AST

	// step 5: generate the actual assembly from the IR bytecode.


	// free all resources
	freeASTProg(&program);
	freeParser(&p);

    return 0;
}
