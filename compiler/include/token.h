#ifndef TOKEN_H
#define TOKEN_H

#include "tokenType.h"

typedef struct token {
    TokenType type;
    Type data_type;
    const char *start;
    int length;
    int line;
} Token;

#endif // TOKEN_H
