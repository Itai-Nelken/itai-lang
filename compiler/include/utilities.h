#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdbool.h>

bool isDigit(char c);
bool isAscii(char c);
int alignTo(int n, int value);

void assertFail(const char *assertion, const char *file, const int line, const char *func);

#endif // UTILITIES_H
