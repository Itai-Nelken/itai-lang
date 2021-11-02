#include <stdio.h>

#include "scanner.h"

int main(void) {
	Scanner s = newScanner("();123");
	Token t=scanToken(&s);
	while(t.type != TOKEN_EOF) {
		// print t.length first characters from t.start
		printf("%.*s\n", t.length, t.start);
		t=scanToken(&s);
	}
	scannerDestroy(&s);

	return 0;
}
