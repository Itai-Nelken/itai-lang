#ifndef ERRORS_H
#define ERRORS_H

#ifndef __GNUC__
#define __attribute__(x)
#endif

#include <stdarg.h>
#include "Token.h"

typedef enum error_type {
    ERR_WARNING,
    ERR_ERROR
} ErrorType;

typedef struct error {
    ErrorType type;
    Location loc;
    char *message;
} Error;

/***
 * Create and initialize a new Error.
 *
 * @param type The type of the error.
 * @param loc The location the error belongs to.
 * @param message The error. The user has to menage the memory for it if it isn't a string literal.
 * @return A new Error initialized with the data with the arguments provided.
 ***/
Error newError(ErrorType type, Location loc, char *message);

void printErrorStr(ErrorType type, Location loc, const char *message);
int vprintErrorF(ErrorType type, Location loc, const char *format, va_list ap);
int printErrorF(ErrorType type, Location loc, const char *format, ...) __attribute__((format(printf, 3, 4)));
void printError(Error err);

#endif // ERRORS_H
