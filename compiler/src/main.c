#include <stdio.h>
#include "common.h"
#include "Token.h"
#include "Scanner.h"
#include "ast.h"
#include "Parser.h"
#include "Validator.h"

// NOTE: ASTProgs have to be freed before the parser
//       as the parser also frees the scanner which makes
//       the Locations in the AST nodes invalid which may cause weird things to happen.

int main(int argc, char **argv) {
	if(argc < 2) {
        fprintf(stderr, "\x1b[1mUSAGE:\x1b[0m %s [code]\n", argv[0]);
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
		goto end;
	}
	
	if(!validate(&prog)) {
		fputs("Validating failed!\n", stderr);
		goto end;
	}

	printf("\x1b[1mglobals:\x1b[0m\n=======\n");
	for(size_t i = 0; i < prog.globals.used; ++i) {
		ASTNode *g = ARRAY_GET_AS(ASTNode *, &prog.globals, i);
		int id = (g->type == ND_ASSIGN ? AS_IDENTIFIER_NODE(AS_BINARY_NODE(g)->left) : AS_IDENTIFIER_NODE(g))->id;
		printf("* '%s'\n", GET_SYMBOL_AS(ASTIdentifier, &prog.identifiers, id)->text);
	}

	printf("\x1b[1mfunctions:\x1b[0m\n=========\n");
	for(size_t i = 0; i < prog.functions.used; ++i) {
		ASTFunction *fn = ARRAY_GET_AS(ASTFunction *, &prog.functions, i);
		ASTIdentifier *name = GET_SYMBOL_AS(ASTIdentifier, &prog.identifiers, fn->name->id);
		printf("\x1b[32m%s\x1b[0m:\n", name->text);

		printf("locals:\n");
		for(size_t i = 0; i < fn->locals.used; ++i) {
			ASTNode *l = ARRAY_GET_AS(ASTNode *, &fn->locals, i);
			int id = (l->type == ND_ASSIGN ? AS_IDENTIFIER_NODE(AS_BINARY_NODE(l)->left) : AS_IDENTIFIER_NODE(l))->id;
			printf("* '%s'\n", GET_SYMBOL_AS(ASTIdentifier, &fn->identifiers, id)->text);
		}

		printf("body:\n");
		printAST(AS_NODE(fn->body));
		putchar('\n');
	}

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
