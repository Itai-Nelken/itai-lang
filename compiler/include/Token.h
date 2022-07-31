#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>
#include "common.h"
#include "Compiler.h"

typedef struct location {
    u64 start, end;
    FileID file;
} Location;

/***
 * Create a new Location.
 *
 * @param start The start point in a file.
 * @param end The end point in a file.
 * @param file The file 'start' and 'end' point to.
 * @return A new Location.
 ***/
Location locationNew(u64 start, u64 end, FileID file);

/***
 * Merge to Locations into a new Location.
 * NOTE: it is a checked runtime error:
 *     1) To provide Locations with different FileID's
 *     2) If a.start is larger than b.end
 *
 * @param a The first Location.
 * @param b The second Location.
 * @return A New Location starting at a.start and ending at b.end.
 ***/
Location locationMerge(Location a, Location b);

// Update tokenPrint(), tokenTypeString() and token_type_name() in Token.c when adding new token types.
typedef enum token_type {
    // One character tokens
    TK_LPAREN, TK_RPAREN,
    TK_PLUS, TK_MINUS,
    TK_STAR, TK_SLASH,

    // Literals
    TK_NUMBER,

    // Other
    TK_GARBAGE,
    TK_EOF
} TokenType;

// Update print_number_constant() in Token.c when adding new types.
typedef enum number_constant_type {
    NUM_I64
}  NumberConstantType;

typedef struct number_constant {
    NumberConstantType type;
    union {
        i64 int64;
    } as;
} NumberConstant;

typedef struct token {
    TokenType type;
    Location location;
    union {
        NumberConstant number_constant;
    } as;
} Token;

/***
 * Make a new i64 NumberConstant.
 *
 * @param value The value.
 * @return A NumberConstant with the value.
 ***/
NumberConstant numberConstantNewInt64(i64 value);

/***
 * Make a new base Token.
 *
 @param type A TokenType.
 @param location The Location of the token.
 @return A new base Token.
 ***/
Token tokenNew(TokenType type, Location location);

/***
 * Make a new number constant Token.
 *
 * @param value A NumberConstant with the value.
 * @return A new number constant Token.
 ***/
Token tokenNewNumberConstant(Location location, NumberConstant value);

/***
 * Print a number constant.
 *
 * @param to The stream to print to.
 * @param value The number constant to print.
 ***/
void printNumberConstant(FILE *to, NumberConstant value);

/***
 * Print a Location.
 *
 * @param to The stream to print to.
 * @param loc The Location to print.
 ***/
void printLocation(FILE *to, Location loc);

/***
 * Print a Token.
 *
 * @param to The stream to print to.
 * @param t The Token to print.
 ***/
void tokenPrint(FILE *to, Token *t);

/***
 * Return the equivalent of a TokenType as a string.
 *
 * @param type A TokenType.
 * @return The equivalent of the TokenType as a string.
 ***/
const char *tokenTypeString(TokenType type);

#endif // TOKEN_H
