#include <stdio.h>
#include <string.h> // memcpy(), memset()
#include <stddef.h> // offsetof
#include <stdarg.h>
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"

typedef struct __attribute__((packed)) string_header {
    size_t capacity;
    size_t length;
    size_t magic;
    char data[];
} StringHeader;

static inline StringHeader *from_str(String s) {
    //return &((StringHeader *)s)[-1];
    return (StringHeader *)(s - offsetof(StringHeader, data));
}

static inline String to_str(StringHeader *h) {
    return h->data;
}

// TODO: remove this function.
static bool is_valid(String s) {
    return from_str(s)->magic == 0xDEADC0DE;
}

String stringNew(size_t capacity) {
    StringHeader *h = ALLOC(sizeof(*h) + sizeof(char) * (capacity + 1));
    memset(h, 0, sizeof(*h) + sizeof(char) * (capacity + 1));
    h->capacity = capacity + 1;
    h->length = 0;
    h->magic = 0xDEADC0DE;
    return to_str(h);
}

void stringFree(String s) {
    VERIFY(is_valid(s));
    // Remove the magic number in case the string is used again accidentally.
    from_str(s)->magic = 0;
    FREE(from_str(s));
}

size_t stringLength(String s) {
    return from_str(s)->length;
}

String stringResize(String s, size_t new_capacity) {
    VERIFY(new_capacity > 0);
    StringHeader *h = from_str(s);
    size_t old_capacity = h->capacity;
    h = REALLOC(h, sizeof(*h) + sizeof(char) * new_capacity);
    memset(to_str(h) + old_capacity, 0, new_capacity - old_capacity);
    h->capacity = new_capacity;
    return to_str(h);
}

String stringNCopy(const char *s, size_t length) {
    String str = stringNew(length);
    memcpy(str, s, length);
    str[length] = '\0';
    from_str(str)->length = length;
    return str;
}

String stringCopy(const char *s) {
    return stringNCopy(s, strlen(s));
}

String stringDuplicate(String s) {
    VERIFY(is_valid(s));
    return stringNCopy((const char *)s, stringLength(s));
}

bool stringEqual(char *s1, char *s2) {
    size_t length1, length2;
    if(is_valid(s1)) {
        length1 = stringLength(s1);
    } else {
        length1 = strlen(s1);
    }
    if(is_valid(s2)) {
        length2 = stringLength(s2);
    } else {
        length2 = strlen(s2);
    }

    if(length1 != length2) {
        return false;
    }
    return memcmp(s1, s2, length1) == 0;
}

String stringVFormat(const char *format, va_list ap) {
    va_list copy;
    va_copy(copy, ap);
    // Get the total length of the formatted string.
    // see man 3 printf.
    int needed_length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    String s = stringNew(needed_length + 1);
    vsnprintf(s, needed_length + 1, format, ap);
    from_str(s)->length = needed_length;
    return s;
}

String stringFormat(const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    String s = stringVFormat(format, ap);
    va_end(ap);
    return s;
}

void stringAppend(String *dest, const char *format, ...) {
    VERIFY(is_valid(*dest));
    va_list ap;

    va_start(ap, format);
    String buffer = stringVFormat(format, ap);
    va_end(ap);

    if(stringLength(*dest) + stringLength(buffer) + 1 > from_str(*dest)->capacity) {
        *dest = stringResize(*dest, stringLength(*dest) + stringLength(buffer) + 1);
    }
    strncat(*dest, buffer, stringLength(buffer));
    // *dest is zeroed by stringNew() & stringResize(), so no need to terminate the string.
    from_str(*dest)->length += stringLength(buffer);
    stringFree(buffer);
}
