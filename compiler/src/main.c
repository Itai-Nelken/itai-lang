#include "memory.h"
#include "Compiler.h"
#include "Error.h"

int main(void) {
    Compiler c;
    compilerInit(&c);
    FileID file = compilerAddFile(&c, "../build/test.ilc");
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, locationNew(13, 16, file), "test error");
    compilerAddError(&c, err);

    compilerPrintErrors(&c);

    compilerFree(&c);
    return 0;
}