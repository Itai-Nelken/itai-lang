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
    } else {
        astPrint(stdout, result);
        fputc('\n', stdout);
        astFree(result);
    }

    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return 0;
}