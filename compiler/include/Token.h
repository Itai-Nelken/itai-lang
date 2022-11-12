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

#define EMPTY_LOCATION() (locationNew(0, 0, 0))

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
    TK_LBRACE, TK_RBRACE,
    TK_PLUS,
    TK_STAR, TK_SLASH,
    TK_SEMICOLON, TK_COLON,
    TK_COMMA,

    // one or two character tokens
    TK_MINUS, TK_ARROW,
    TK_EQUAL, TK_EQUAL_EQUAL,
    TK_BANG, TK_BANG_EQUAL,

    // Literals
    TK_NUMBER,

    // keywords
    TK_IF, TK_ELSE,
    TK_WHILE,
    TK_FN, TK_RETURN,
    TK_VAR,

    // primitive types
    TK_I32, TK_U32,

    // identifier
    TK_IDENTIFIER,

    // Other
    TK_GARBAGE,
    TK_EOF,
    TK_TYPE_COUNT
} TokenType;

typedef struct token {
    TokenType type;
    Location location;
    char *lexeme;
    u32 length;
} Token;

/***
 * Make a new Token.
 *
 * @param type A TokenType.
 * @param location The Location of the token.
 * @param lexeme The lexeme the token represents.
 * @param length The length of the lexeme.
 * @return A new base Token.
 ***/
Token tokenNew(TokenType type, Location location, char *lexeme, u32 length);

/***
 * Print a Location.
 *
 * @param to The stream to print to.
 * @param loc The Location to print.
 * @param compact Print the location in a compact way.
 ***/
void locationPrint(FILE *to, Location loc, bool compact);

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
