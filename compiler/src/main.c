#include <stdio.h>
#include "common.h"
#include "Token.h"
#include "Scanner.h"
#include "ast.h"
#include "Parser.h"
#include "Codegen.h"

// NOTE: ASTProgs have to be freed before the parser
//       as the parser also frees the scanner which makes
//       the Locations in the AST nodes invalid which may cause weird things to happen.

static void pretty_print(ASTProg *prog) {
	printf("\x1b[1mglobals:\x1b[0m\n=======\n");
	for(size_t i = 0; i < prog->globals.used; ++i) {
		ASTNode *g = ARRAY_GET_AS(ASTNode *, &prog->globals, i);
		ASTIdentifierNode *id_node = g->type == ND_EXPR_STMT ? AS_IDENTIFIER_NODE(AS_BINARY_NODE(AS_UNARY_NODE(g)->child)->left) : AS_IDENTIFIER_NODE(g);
		printf("(id: %d) name: '%s', type: {", id_node->id, GET_SYMBOL_AS(ASTIdentifier *, &prog->identifiers, id_node->id)->text);
		printType(id_node->type);
		puts("}");
	}

	printf("\x1b[1mfunctions:\x1b[0m\n=========\n");
	for(size_t i = 0; i < prog->functions.used; ++i) {
		ASTFunction *fn = ARRAY_GET_AS(ASTFunction *, &prog->functions, i);
		ASTIdentifier *name = GET_SYMBOL_AS(ASTIdentifier *, &prog->identifiers, fn->name->id);
		printf("\x1b[1;32m%s\x1b[0m:\n", name->text);
		printf("name id: %d\n", fn->name->id);
		printf("return type: {");
		printType(fn->return_type);
		puts("}");

		printf("locals:\n");
		for(size_t i = 0; i < fn->locals.used; ++i) {
			ASTNode *l = ARRAY_GET_AS(ASTNode *, &fn->locals, i);
			ASTIdentifierNode *id_node = l->type == ND_ASSIGN ? AS_IDENTIFIER_NODE(AS_BINARY_NODE(l)->left) : AS_IDENTIFIER_NODE(l);
			printf("(id: %d) name: '%s', type: {", id_node->id, GET_SYMBOL_AS(ASTIdentifier *, &prog->identifiers, id_node->id)->text);
			printType(id_node->type);
			puts("}");
		}

		printf("body:\n");
		printAST(AS_NODE(fn->body));
		putchar('\n');
	}

end:
	freeASTProg(&prog);
	freeParser(&p);
	freeScanner(&s);
    return 0;
}
