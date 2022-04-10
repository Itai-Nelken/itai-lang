#include <stdio.h>
#include <stddef.h> // size_t
#include <stdarg.h>
#include <assert.h>
#include "common.h"
#include "Strings.h"
#include "Token.h"
#include "Errors.h"

static void indentLine(Token *tok) {
    int line = tok->location.line;
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

static void printTokenLocation(Token *tok) {
    fprintf(stderr, "\t%d | ", tok->location.line);
    fprintf(stderr, "%.*s\n", tok->location.line_length, tok->location.containing_line);
    fputs("\t", stderr);
    indentLine(tok);
}

static const char *errortypes[] = {"\x1b[35mwarning", "\x1b[31merror"};

void printError(ErrorType type, Token tok, const char *message) {
    fprintf(stderr, "\x1b[1m%s:%d:%d: ", tok.location.file, tok.location.line, tok.location.at + 1);
    fprintf(stderr, "%s:\x1b[0m\n", errortypes[type]);
    printTokenLocation(&tok);
    fprintf(stderr, " | %*s", tok.location.at, "");
    fprintf(stderr, "\x1b[1;35m^");
    // tok.length - 1 because the first character is used by the '^'
    for(int i = 0; i < tok.length - 1; ++i) {
        fputc('~', stderr);
    }
    fprintf(stderr, " \x1b[0;1m%s\x1b[0m\n", message);
}

int printErrorF(ErrorType type, Token tok, const char *format, ...) {
    char *buffer = NULL;
    int length = 0;
    va_list ap;

    // determine how much space is needed for the final string
    // copied from 'man 3 printf'
    va_start(ap, format);
    length = vsnprintf(buffer, 0, format, ap);
    va_end(ap);
    assert(length > 0);

    buffer = newString(length + 1); // +1 for the nul character ('\0')
    va_start(ap, format);
    length = vsnprintf(buffer, (size_t)length + 1, format, ap);
    va_end(ap);

    printError(type, tok, buffer);

    freeString(buffer);
    return length;
}
