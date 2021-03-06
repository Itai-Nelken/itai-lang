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

enum slots {
    CAPACITY   = 0,
    LENGTH     = 1,
    MAGIC      = 2,
    SLOT_COUNT = 3
};

static inline size_t *from_str(char *s) {
    return ((size_t *)s) - SLOT_COUNT;
}

static inline char *to_str(size_t *ptr) {
    return (char *)(ptr + SLOT_COUNT);
}

char *newString(size_t capacity) {
    assert(capacity > 0);
    size_t *ptr = ALLOC(sizeof(size_t) * SLOT_COUNT + sizeof(char) * (capacity + 1));
    memset(ptr, 0, capacity + 1);
    ptr[CAPACITY] = capacity + 1; // capacity
    ptr[LENGTH] = 0; // length
    ptr[MAGIC] = 0xDEADC0DE; // magic
    return to_str(ptr);
}

void freeString(char *s) {
    assert(stringIsValid(s));
    // Remove the magic number in case the string is used again accidentally.
    from_str(s)[MAGIC] = 0;
    FREE(from_str(s));
}

bool stringIsValid(char *s) {
    return from_str(s)[MAGIC] == 0xDEADC0DE;
}

size_t stringLength(char *s) {
    return from_str(s)[LENGTH];
}

char *stringResize(char *s, size_t newCapacity) {
    assert(newCapacity > 0);
    size_t *ptr = from_str(s);
    size_t oldCap = ptr[CAPACITY];
    ptr = REALLOC(ptr, newCapacity);
    memset(ptr + oldCap, 0, labs(((ssize_t)oldCap) - ((ssize_t)newCapacity)));
    ptr[CAPACITY] = newCapacity;
    return to_str(ptr);
}

char *stringNCopy(const char *s, int length) {
    char *str = newString(length);
    memcpy(str, s, length);
    from_str(str)[LENGTH] = length;
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

    if((size_t)(needed_length + 1) > from_str(dest)[CAPACITY]) {
        stringResize(dest, stringLength(dest) + needed_length + 1);
    }
    strncat(dest, buffer, needed_length);
    // dest is zeroed by stringNew() & stringResize(), so no need to terminate the string.
    freeString(buffer);
    from_str(dest)[LENGTH] += needed_length;
}
