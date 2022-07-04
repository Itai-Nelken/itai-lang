#include <stdio.h>
#include <string.h> // memcpy()
#include <stdarg.h>
#include <math.h> // abs()
#include <assert.h>
#include <stdbool.h>
#include "memory.h"
#include "Strings.h"

// C = capacity
// L = length
// M = magic number
// D = data
// ________________________
// |   |   |   |   |   |   | ...
// | C | L | M | D | D | D | ...
// |___|___|___|___|___|___| ...
//

static inline size_t *from_str(char *s) {
    return ((size_t *)s) - 3;
}

static inline char *to_str(size_t *ptr) {
    return (char *)(ptr + 3);
}

char *newString(size_t capacity) {
    assert(capacity > 0);
    size_t *ptr = ALLOC(sizeof(size_t) * 3 + sizeof(char) * (capacity + 1));
    memset(ptr, 0, capacity + 1);
    ptr[0] = capacity + 1; // capacity
    ptr[1] = 0; // length
    ptr[2] = 0xDEADC0DE; // magic
    return to_str(ptr);
}

void freeString(char *s) {
    assert(stringIsValid(s));
    // Remove the magic number in case the string is used again accidentally.
    from_str(s)[2] = 0;
    FREE(from_str(s));
}

bool stringIsValid(char *s) {
    return from_str(s)[2] == 0xDEADC0DE;
}

size_t stringLength(char *s) {
    return from_str(s)[1];
}

char *stringResize(char *s, size_t newCapacity) {
    assert(newCapacity > 0);
    size_t *ptr = from_str(s);
    size_t oldCap = ptr[0];
    ptr = REALLOC(ptr, newCapacity);
    memset(ptr + oldCap, 0, labs(((ssize_t)oldCap) - ((ssize_t)newCapacity)));
    ptr[0] = newCapacity;
    return to_str(ptr);
}

char *stringNCopy(const char *s, int length) {
    char *str = newString(length);
    memcpy(str, s, length);
    from_str(str)[1] = length;
    return str;
}

char *stringCopy(const char *s) {
    return stringNCopy(s, strlen(s));
}

char *stringDuplicate(char *s) {
    assert(stringIsValid(s));
    return stringNCopy((const char *)s, stringLength(s));
}

bool stringEqual(char *s1, char *s2) {
    size_t length1, length2;
    if(stringIsValid(s1)) {
        length1 = stringLength(s1);
    } else {
        length1 = strlen(s1);
    }
    if(stringIsValid(s2)) {
        length2 = stringLength(s2);
    } else {
        length2 = strlen(s2);
    }

    if(length1 != length2) {
        return false;
    }
    return memcmp(s1, s2, length1) == 0;
}

void stringAppend(char *dest, const char *format, ...) {
    assert(stringIsValid(dest));
    va_list ap;

    // Get the total length of the formatted string.
    // see man 3 printf.
    va_start(ap, format);
    int needed_length = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    char *buffer = newString(needed_length + 1);
    va_start(ap, format);
    vsnprintf(buffer, needed_length + 1, format, ap);
    va_end(ap);

    // from_str(dest)[0] == capacity
    if((size_t)(needed_length + 1) > from_str(dest)[0]) {
        stringResize(dest, needed_length + 1);
    }
    strncat(dest, buffer, needed_length);
    // dest is filled with 0 by stringResize(), so no need to terminate the string.
    freeString(buffer);
    from_str(dest)[1] = needed_length;
}
