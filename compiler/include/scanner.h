#ifndef SCANNER_H
#define SCANNER_H
#include "common.h"
#include "token.h"

typedef struct scanner {
    bool _inited;
    char *_source;
    char *start, *current;
    int line;
} Scanner;

Scanner newScanner(const char *source);
void scannerInit(Scanner *s, const char *source);
void scannerDestroy(Scanner *s);

Token scanToken(Scanner *s);

#endif // SCANNER_H
