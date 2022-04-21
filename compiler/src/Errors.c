#include <stdio.h>
#include <stddef.h> // size_t
#include <stdarg.h>
#include <assert.h>
#include "common.h"
#include "Strings.h"
#include "Token.h"
#include "Errors.h"

static void indentLine(Location *const loc) {
    int line = loc->line;
    short indent = 0;
    // FIXME: only works with up to 9999 lines of code
    if(line < 10) {
        indent = 1;
    } else if(line < 100) {
        indent = 2;
    } else if(line < 1000) {
        indent = 3;
    } else if(line < 10000) {
        indent = 3;
    } else {
        UNREACHABLE();
    }
    fprintf(stderr, "%*s", indent, "");
}

static void printLocation(Location *const loc) {
    fprintf(stderr, "\t%d | ", loc->line);
    fprintf(stderr, "%.*s\n", loc->line_length, loc->containing_line);
    fputs("\t", stderr);
    indentLine(loc);
}

static const char *errortypes[] = {"\x1b[35mwarning", "\x1b[31merror"};

void printError(ErrorType type, Location loc, const char *message) {
    fprintf(stderr, "\x1b[1m%s:%d:%d: ", loc.file, loc.line, loc.at + 1);
    fprintf(stderr, "%s:\x1b[0m\n", errortypes[type]);
    printLocation(&loc);
    fprintf(stderr, " | %*s", loc.at, "");
    fprintf(stderr, "\x1b[1;35m^");
    // tok.length - 1 because the first character is used by the '^'
    for(int i = 0; i < loc.length - 1; ++i) {
        fputc('~', stderr);
    }
    fprintf(stderr, " \x1b[0;1m%s\x1b[0m\n", message);
}

int vprintErrorF(ErrorType type, Location loc, const char *format, va_list ap) {
    char *buffer = NULL;
    int length = 0;
    va_list copy;

    va_copy(copy, ap);

    // determine how much space is needed for the final string
    // copied from 'man 3 printf'
    length = vsnprintf(buffer, 0, format, ap);
    assert(length > 0);

    buffer = newString(length + 1); // +1 for the nul character ('\0')
    length = vsnprintf(buffer, (size_t)length + 1, format, copy);
    va_end(copy);

    printError(type, loc, buffer);
    freeString(buffer);
    return length;
}

int printErrorF(ErrorType type, Location loc, const char *format, ...) {
    int length = 0;
    va_list ap;

    // determine how much space is needed for the final string
    // copied from 'man 3 printf'
    va_start(ap, format);
    vprintErrorF(type, loc, format, ap);
    va_end(ap);

    return length;
}
