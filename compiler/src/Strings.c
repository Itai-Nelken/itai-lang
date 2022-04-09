#include <string.h> // memcpy()
#include <assert.h>
#include "memory.h"
#include "Strings.h"

// the first 4 bytes of the strings are used for storing their length
// l = length
// d = data
// ___________________________________
// |      |      |      |      |      |
// |  l   |  l   |  l   |  l   |  d   |
// |______|______|______|______|______|

static_assert(sizeof(int) == 4);

char *newString(int length) {
    assert(length > 0);
    // + 4 for the length bytes
    char *s = CALLOC(length + 4, sizeof(char));
    
    // destructure the length int into it's 4 bytes
    s[0] = (length >> 24) & 0xff;
    s[1] = (length >> 16) & 0xff;
    s[2] = (length >> 8) & 0xff;
    s[3] = length & 0xff;

    return s + 4;
}

void freeString(char *s) {
    char *str = s - 4;
    FREE(str);
}

int stringLength(char *s) {
    // restructure the first 4 bytes into the length int
    char bytes[4] = {s[-1], s[-2], s[-3], s[-4]};
    return *((int *)bytes);
}

char *stringResize(char *s, int newLength) {
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
