#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Token.h"
#include "ast.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"
#include "Validator.h"

int main(void) {
    int return_value = 0;
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
    if(!result) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Parser failed but didn't report any errors!\n", stderr);
        }
        return_value = 1;
        goto end;
    }

    result = validateAndTypecheckProgram(&c, &prog);
    if(!result) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Validator failed but didn't report any errors!\n", stderr);
        }
        return_value = 1;
        goto end;
    }

    astPrintProgram(stdout, &prog);
    fputc('\n', stdout);

end:
    astFreeProgram(&prog);
    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return return_value;
}