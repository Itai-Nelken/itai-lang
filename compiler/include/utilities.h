#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdbool.h>

int alignTo(int n, int value);

void assertFail(const char *assertion, const char *file, const int line, const char *func);

#endif // UTILITIES_H
