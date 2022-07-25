#include <stdbool.h>
#include "utilities.h"

inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

inline bool isAscii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}
