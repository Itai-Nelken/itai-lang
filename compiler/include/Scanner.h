#ifndef SCANNER_H
#define SCANNER_H

#include "common.h"
#include "Compiler.h"
#include "Array.h"
#include "Strings.h"

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
 * Scan All the files in the Compiler provided to scannerInit().
 * 
 * @param s An initialized Scanner.
 * @return Array<Token *> containing all the tokens.
 ***/
Array scannerScan(Scanner *s);

#endif // SCANNER_H
