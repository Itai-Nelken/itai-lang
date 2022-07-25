#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>
#include "common.h"
#include "Compiler.h"

typedef struct location {
    u64 start, end;
    FileID file;
} Location;

Location locationNew(u64 start, u64 end, FileID file);

// Update token_name() and token_type_name() in Token.c when adding new token types.
typedef enum token_type {
    // One character tokens
    TK_LPAREN, TK_RPAREN,
    TK_PLUS, TK_MINUS,
    TK_STAR, TK_SLASH,

    // Literals
    TK_NUMBER,

    // Other
    TK_EOF
} TokenType;

// Update print_number_constant() in Token.c when adding new types.
typedef enum number_constant_type {
    NUM_I64
}  NumberConstantType;

typedef struct token {
    TokenType type;
    Location location;
} Token;

typedef struct number_constant {
    NumberConstantType type;
    union {
        i64 int64;
    } as;
} NumberConstant;

typedef struct number_constant_token {
    Token header;
    NumberConstant value;
} NumberConstantToken;

#define AS_TOKEN(token) ((Token *)(token))
#define AS_NUMBER_CONSTANT_TOKEN(token) ((NumberConstantToken *)(token))

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
 * Make a new operator Token.
 *
 * @param type A TokenType.
 * @param location The Location of the token.
 * @return A new operator Token.
 ***/
Token *tokenNewOperator(TokenType type, Location location);

/***
 * Make a new number constant Token.
 *
 * @param value A NumberConstant with the value.
 * @return A new number constant Token.
 ***/
Token *tokenNewNumberConstant(Location location, NumberConstant value);

/***
 * Free a Token.
 *
 * @param t The Token to free.
 ***/
void tokenFree(Token *t);

/***
 * Print a Token.
 *
 * @param to The stream to print to.
 * @param t The Token to print.
 ***/
void tokenPrint(FILE *to, Token *t);

#endif // TOKEN_H
