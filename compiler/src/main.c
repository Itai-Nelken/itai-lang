#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"
#include "ast.h"

int main(void) {
    Compiler c;
    Scanner s;
    Parser p;
    ASTProgram prog;
    compilerInit(&c);
    scannerInit(&s, &c);
    parserInit(&p, &c);
    astInitProgram(&prog);

    compilerAddFile(&c, "../build/test.ilc");

    bool result = parserParse(&p, &s, &prog);
    if(compilerHadError(&c)) {
        compilerPrintErrors(&c);
    } else if(result){
        astPrintProgram(stdout, &prog);
        fputc('\n', stdout);
    } else {
        fputs("\x1b[1;31mError:\x1b[0m Parser failed but didn't report any errors!\n", stderr);
    }

    astFreeProgram(&prog);
    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return 0;
}