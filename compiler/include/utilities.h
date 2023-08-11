#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdbool.h>
#include <stdint.h>

// A slice into a string.
// NOTE: The string IS NOT owned by the slice.
//       The lifetime of the data is handled by the creator of the slice.
typedef struct string_slice {
    char *data;
    uint32_t length;
} StringSlice;

/***
 * Create a new StringSlice.
 *
 * @param str The string data.
 * @param len The length of the string.
 * @return A new StringSlice.
 ***/
#define STRING_SLICE(str, len)n ((StringSlice){.data = (str), .length = (len)})

/***
 * Create a new StringSlice from a String.
 *
 * @param string The String to create a slice from.
 * @return A new StringSlice.
 ***/
#define STRING_SLICE_FROM_STRING(string) STRING_SLICE((string), stringLength((string)))

void assertFail(const char *assertion, const char *file, const int line, const char *func);

#endif // UTILITIES_H
