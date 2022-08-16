#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Error.h"
#include "Token.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"

int main(void) {
    Compiler c;
    Scanner s;
    Parser p;
    compilerInit(&c);
    scannerInit(&s, &c);
    parserInit(&p, &c);

    compilerAddFile(&c, "../build/test.ilc");

    ASTNode *result = parserParse(&p, &s);
    if(compilerHadError(&c)) {
        compilerPrintErrors(&c);
    } else if (result){
        astPrint(stdout, result);
        fputc('\n', stdout);
        astFree(result);
    } else {
        fputs("\x1b[1;31mError:\x1b[0m Parser returned NULL but didn't report any errors!\n", stderr);
    }

    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return 0;
}