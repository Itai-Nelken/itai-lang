#include <stdbool.h>
#include "utilities.h"

inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

inline bool isAscii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
