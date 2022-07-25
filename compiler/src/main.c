#include "common.h"
#include "memory.h"
#include "Error.h"
#include "Token.h"
#include "Compiler.h"
#include "Scanner.h"

#include <stdio.h>
static void print_token_callback(void *token, void *cl) {
    UNUSED(cl);
    tokenPrint(stdout, (Token *)token);
    puts(""); // newline
}

static void free_token_callback(void *token, void *cl) {
    UNUSED(cl);
    tokenFree((Token *)token);
}

int main(void) {
    Compiler c;
    Scanner s;
    compilerInit(&c);
    scannerInit(&s, &c);

    compilerAddFile(&c, "../build/test.ilc");
    Array tokens = scannerScan(&s);
    arrayMap(&tokens, print_token_callback, NULL);
    arrayMap(&tokens, free_token_callback, NULL);
    arrayFree(&tokens);

    if(compilerHadError(&c)) {
        compilerPrintErrors(&c);
    }

    scannerFree(&s);
    compilerFree(&c);
    return 0;
}