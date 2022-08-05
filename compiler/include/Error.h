#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdbool.h>
#include "Compiler.h"
#include "Strings.h"
#include "Token.h"

// add new types to error_type_to_string(), error_type_color() in Error.c
typedef enum error_type {
    ERR_ERROR,
} ErrorType;

typedef struct error {
    ErrorType type;
    bool has_location;
    Location location;
    String message;
} Error;

/***
 * Initialize a new Error.
 * 
 * @param err The Error to initialize.
 * @param type The ErrorType.
 * @param location The location of the error in the source files.
 * @param message The error message.
 ***/
void errorInit(Error *err, ErrorType type, bool has_location, Location location, const char *message);

/***
 * Free an Error.
 * 
 * @param err An initialized error to free.
 ***/
void errorFree(Error *err);

/***
 * Print an Error to 'to'.
 * 
 * @param err The Error to print.
 * @param compiler The Compiler to get the file contents from.
 * @param to The stream to write to.
 ***/
void errorPrint(Error *err, Compiler *compiler, FILE *to);

#endif // ERROR_H
