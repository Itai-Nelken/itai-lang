#ifndef SCANNER_H
#define SCANNER_H

#include "Token.h"

typedef struct scanner {
    char *filename;
    int line;

    char *source;
    char *start;
    char *current;
} Scanner;

/***
 * Initialize a scanner
 * @param s A scanner to initialize.
 * @param filename The name of the file being scanned.
 * @param source The source code to scan. it won't be changed by the scanner.
 ***/
void initScanner(Scanner *s, const char *filename, char *source);

/***
 * Free a scanners resources.
 * @param s An initialized scanner.
 ***/
void freeScanner(Scanner *s);

/***
 * Get the next token.
 * @param s An initialized scanner;
 ***/
Token nextToken(Scanner *s);

#endif // SCANNER_H
