#include "common.h"
#include "memory.h"
#include "Error.h"
#include "Token.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"

#include <stdio.h>
static void print_token_callback(void *token, void *cl) {
    UNUSED(cl);
    tokenPrint(stdout, (Token *)token);
    fputc('\n', stdout); // newline
}

static void free_token_callback(void *token, void *cl) {
    UNUSED(cl);
    tokenFree((Token *)token);
}

int main(void) {
    Compiler c;
    Scanner s;
    Parser p;
    compilerInit(&c);
    scannerInit(&s, &c);
    parserInit(&p, &c);

    compilerAddFile(&c, "../build/test.ilc");
    Array tokens = scannerScan(&s);
    if(compilerHadError(&c)) {
        compilerPrintErrors(&c);
        goto end;
    }
    arrayMap(&tokens, print_token_callback, NULL);
    fputc('\n', stdout);

    ASTNode *result = parserParse(&p, &tokens);
    if(compilerHadError(&c)) {
        compilerPrintErrors(&c);
    } else {
        astPrint(stdout, result);
        fputc('\n', stdout);
        astFree(result);
    }

end:
    arrayMap(&tokens, free_token_callback, NULL);
    arrayFree(&tokens);
    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return 0;
}