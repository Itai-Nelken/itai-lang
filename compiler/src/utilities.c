#include <stdio.h>
#include <stdlib.h> // abort()
#include <stdbool.h>
#include "utilities.h"

inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

inline bool isAscii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline int alignTo(int n, int align) {
    return (n + align - 1) / align * align;
}

void assertFail(const char *assertion, const char *file, const int line, const char *func) {
    fprintf(stderr, "\n============\nInternal error at %s(): %s:%d:", func, file, line);
    fprintf(stderr, " assertion '%s' failed!\n============\n", assertion);
    fflush(stderr);
    abort();
}
