#ifndef ERRORS_H
#define ERRORS_H

#ifndef __GNUC__
#define __attribute__(x)
#endif

#include "Token.h"

typedef enum error_type {
    ERR_WARNING,
    ERR_ERROR
} ErrorType;

void printError(ErrorType type, Location loc, const char *message);
int printErrorF(ErrorType type, Location loc, const char *format, ...) __attribute__((format(printf, 3, 4)));

#endif // ERRORS_H
