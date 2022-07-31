#ifndef SCANNER_H
#define SCANNER_H

#include "common.h"
#include "Compiler.h"
#include "Strings.h"
#include "Token.h"

typedef struct scanner {
    Compiler *compiler;
    String source;
    usize start, current;
} Scanner;

/***
 * Initialize a Scanner.
 * 
 * @param s A Scanner to initialize.
 * @param c An initialized Compiler.
 ***/
void scannerInit(Scanner *s, Compiler *c);

/***
 * Free a Scanner.
 * 
 * @param s A Scanner to free.
 ***/
void scannerFree(Scanner *s);

/***
 * Scan the next token in the current file, and change to the next file when needed.
 * 
 * @param s An initialized Scanner.
 * @return The next token.
 ***/
Token scannerNextToken(Scanner *s);

#endif // SCANNER_H
