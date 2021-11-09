#include <stdio.h>

#include "scanner.h"
#include "tokenType.h"

static const char *t2s(TokenType type) {
	switch (type) {
		case TOKEN_ERROR:
			return "TOKEN_ERROR";
		case TOKEN_PAREN_LEFT:
			return "TOKEN_PAREN_LEFT";
		case TOKEN_PAREN_RIGHT:
			return "TOKEN_PAREN_RIGHT";
		case TOKEN_BRACE_LEFT:
			return "TOKEN_BRACE_LEFT";
		case TOKEN_BRACE_RIGHT:
			return "TOKEN_BRACE_RIGHT";
		case TOKEN_SEMICOLON:
			return "TOKEN_SEMICOLON";
		case TOKEN_COMMA:
			return "TOKEN_COMMA";
		case TOKEN_NUMBER:
			return "TOKEN_NUMBER";
		case TOKEN_CHARACTER:
			return "TOKEN_CHARACTER";
		case TOKEN_IDENTIFIER:
			return "TOKEN_IDENTIFIER";
		case TOKEN_PLUS:
			return "TOKEN_PLUS";
		case TOKEN_MINUS:
			return "TOKEN_MINUS";
		case TOKEN_STAR:
			return "TOKEN_STAR";
		case TOKEN_SLASH:
			return "TOKEN_SLASH";
		case TOKEN_BANG:
			return "TOKEN_BANG";
		case TOKEN_BANG_EQUAL:
			return "TOKEN_BANG_EQUAL";
		case TOKEN_EQUAL:
			return "TOKEN_EQUAL";
		case TOKEN_EQUAL_EQUAL:
			return "TOKEN_EQUAL_EQUAL";
		case TOKEN_LESS:
			return "TOKEN_LESS";
		case TOKEN_LESS_EQUAL:
			return "TOKEN_LESS_EQUAL";
		case TOKEN_GREATER:
			return "TOKEN_GREATER";
		case TOKEN_GREATER_EQUAL:
			return "TOKEN_GREATER_EQUAL";
		case TOKEN_VAR:
			return "TOKEN_VAR";
		case TOKEN_WHILE:
			return "TOKEN_WHILE";
		case TOKEN_IF:
			return "TOKEN_IF";
		case TOKEN_ELSE:
			return "TOKEN_ELSE";
		case TOKEN_FALSE:
			return "TOKEN_FALSE";
		case TOKEN_TRUE:
			return "TOKEN_TRUE";
		case TOKEN_FN:
			return "TOKEN_FN";
		case TOKEN_RETURN:
			return "TOKEN_RETURN";
		case TOKEN_TYPE_INT:
			return "TOKEN_TYPE_INT";
		case TOKEN_TYPE_INT_PTR:
			return "TOKEN_TYPE_INT_PTR";
		case TOKEN_TYPE_CHAR:
			return "TOKEN_TYPE_CHAR";
		case TOKEN_TYPE_CHAR_PTR:
			return "TOKEN_TYPE_CHAR_PTR";
		case TOKEN_TYPE_BOOL:
			return "TOKEN_TYPE_BOOL";
		case TOKEN_TYPE_BOOL_PTR:
			return "TOKEN_TYPE_BOOL_PTR";
		default:
			break;
	}
	return "UNKNOWN";
}

int main(int argc, char **argv) {
	Scanner s = newScanner((argc>1 ? argv[1] : "var int a = 123;"));
	Token t;
	while((t=scanToken(&s)).type != TOKEN_EOF) {
		if(t.type == TOKEN_ERROR) {
			printf("%s : line %d, %.*s\n", t2s(t.type), t.line, t.length, t.start);
		} else {
			// print t.length first characters from t.start
			printf("%s : %.*s\n", t2s(t.type), t.length, t.start);
		}
	}
	puts("TOKEN_EOF");
	scannerDestroy(&s);

	return 0;
}
