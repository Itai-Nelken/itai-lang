#include <string.h> // memcpy()
#include <stdbool.h>
#include <assert.h>
#include "memory.h"
#include "Strings.h"

// The first 4 bytes of the strings are used for storing the length.
// The next byte is used to store a magic number so the string functions
// can know if the string is a valid one, and the rest of the bytes are used for the actual string.
// l = length
// m = magic
// d = data
// __________________________________________
// |      |      |      |      |      |      |
// |  l   |  l   |  l   |  l   |  m   |  d   |
// |______|______|______|______|______|______|

static_assert(sizeof(int) == 4);

char *newString(int length) {
    assert(length > 0);
    // + 4 for the length bytes
    char *s = CALLOC(length + 5, sizeof(char));

    // destructure the length int into it's 4 bytes
    s[0] = (length >> 24) & 0xff;
    s[1] = (length >> 16) & 0xff;
    s[2] = (length >> 8) & 0xff;
    s[3] = length & 0xff;

    // set the magic number
    s[4] = 255; // out of the range of the regular ascii table

    return s + 5;
}

void freeString(char *s) {
    assert(stringIsValid(s));
    char *str = s - 5;
    FREE(str);
}

bool stringIsValid(char *s) {
    return s[-1] == 255;
}

int stringLength(char *s) {
    assert(stringIsValid(s));
    // restructure the first 4 bytes into the length int
    char bytes[4] = {s[-2], s[-3], s[-4], s[-5]};
    return *((int *)bytes);
}

char *stringResize(char *s, int newLength) {
    assert(stringIsValid(s));
    assert(newLength > 0);
    int oldLength = stringLength(s);
    char *str = s - 4;

    str = REALLOC(str, sizeof(char) * newLength + 4);
    memset(str + 4 + oldLength, '\0', newLength - oldLength);
    return str + 4;
}

char *stringNCopy(const char *s, int length) {
    char *str = newString(length + 1);
    memcpy(str, s, length);
    str[length] = '\0';
    return str;
}

char *stringCopy(const char *s) {
    return stringNCopy(s, strlen(s));
}

char *stringDuplicate(char *s) {
    assert(stringIsValid(s));
    int length = stringLength(s);
    char *str = newString(length+1);
    memcpy(str, s, length);
    str[length] = '\0';
    return str;
}

bool stringEqual(char *s1, char *s2) {
    int length1, length2;
    if(!stringIsValid(s1)) {
        length1 = strlen(s1);
    } else {
        length1 = stringLength(s1);
    }
    if(!stringIsValid(s2)) {
        length2 = strlen(s2);
    } else {
        length2 = stringLength(s2);
    }
    assert(stringIsValid(s2));
    if(length1 != length2) {
        return false;
    }
    return memcmp(s1, s2, length1) == 0;
}
