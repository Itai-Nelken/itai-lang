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

// FIXME: find a better way to represent an empty location.
#define EMPTY_LOCATION (locationNew((u64)-1, (u64)-1, (u64)-1))

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

// Update tokenPrint(), tokenTypeString(), token_type_name() in Token.c
// and the parse table in Parser.c when adding new token types.
typedef enum token_type {
    // One character tokens
    TK_LPAREN, TK_RPAREN,
    TK_LBRACKET, TK_RBRACKET,
    TK_LBRACE, TK_RBRACE,
    TK_PLUS,
    TK_STAR, TK_SLASH,
    TK_SEMICOLON, TK_COLON,
    TK_COMMA, TK_DOT,
    TK_HASH,

    // one or two character tokens
    TK_AMPERSAND, TK_AND, // and is &&
    TK_PIPE, TK_OR, // |, ||
    TK_MINUS, TK_ARROW,
    TK_EQUAL, TK_EQUAL_EQUAL,
    TK_BANG, TK_BANG_EQUAL,
    TK_LESS, TK_LESS_EQUAL,
    TK_GREATER, TK_GREATER_EQUAL,
    TK_SCOPE_RESOLUTION, // :: (as in Module::Type.)

    // Literals
    TK_NUMBER_LITERAL,
    TK_STRING_LITERAL,
    TK_TRUE, TK_FALSE,

    // keywords
    TK_IF, TK_ELSE,
    TK_WHILE,
    TK_FN, TK_RETURN,
    TK_VAR,
    TK_STRUCT,
    TK_EXTERN,
    TK_DEFER,
    TK_EXPECT,
    TK_IMPORT,

    // primitive types
    TK_VOID, TK_I32, TK_U32, TK_STR, TK_BOOL,

    // identifier
    TK_IDENTIFIER,

    // Other
    TK_FILE_CHANGED, // emitted when the source file is changed. lexeme is name of new file,
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
