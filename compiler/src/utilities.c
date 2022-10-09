#include <stdio.h>
#include <stdlib.h> // abort()
#include <stdbool.h>
#include "utilities.h"

inline int alignTo(int n, int align) {
    return (n + align - 1) / align * align;
}

void assertFail(const char *assertion, const char *file, const int line, const char *func) {
    fprintf(stderr, "\n============\nInternal error at %s(): %s:%d:", func, file, line);
    fprintf(stderr, " assertion '%s' failed!\n============\n", assertion);
    fflush(stderr);
    abort();
}
