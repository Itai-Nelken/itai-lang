#include <stdio.h>
#include <stdlib.h> // abort()
#include "utilities.h"

void assertFail(const char *assertion, const char *file, const int line, const char *func) {
    fflush(stdout);
    fprintf(stderr, "\n============\nInternal error at %s(): %s:%d:", func, file, line);
    fprintf(stderr, " assertion '%s' failed!\n============\n", assertion);
    fflush(stderr);
    abort();
}
