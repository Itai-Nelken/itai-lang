#ifndef STRINGS_H
#define STRINGS_H

#include <stddef.h> // size_t
#include <stdarg.h>
#include <stdbool.h>

// NOTES
// =====
// The 'String' type is equivalent to a C string ('char *'), and therefore can
// be used with the regular C string functions.
// In this API, the String type is used when a string created by this library is expected,
// and 'char *' is used when both types can be used.
// 'const char *' is used when ONLY C strings are expected.

typedef char *String;

/***
 * Create a new empty string 'length' long
 * NOTE: strings returned by stringNew() can ONLY be freed with stringFree()
 *
 * @param length The length of the new string.
 * @return A pointer to a new heap allocated string.
 ***/
String stringNew(size_t length);

/***
 * Free a string created by stringNew() or stringCopy();
 *
 * @param s A string created with stringNew() or stringCopy().
 ***/
void stringFree(String s);

/***
 * Check if a string was created by stringNew(), stringCopy(), or stringDuplicate().
 *
 * @param s A string.
 * @return true if valid, false if not.
 ***/
bool stringIsValid(String s);

/***
 * Return the length of a string allocated with stringNew() or stringCopy().
 * 
 * @param s A string allocated by stringNew() or stringCopy().
 * @return The length of the string.
 ***/
size_t stringLength(String s);

/***
 * Resize a string allocated by stringNew() or stringCopy().
 * NOTE: if 'newSize' is smaller than the current length, data will be lost!
 *
 * @param s A string allocated by stringNew() or stringCopy().
 * @param new_capacity The new length. can't be 0.
 * @return A new string of length 'newLength'.
 ***/
String stringResize(String s, size_t new_capacity);

/***
 * Copy 'length' characters from 's' into a new string.
 *
 * @param s A string to copy.
 * @param length How much characters to copy.
 * @return A new copy of 'length' characters of 's'.
 ***/
String stringNCopy(const char *s, size_t length);

/***
 * Copy a string into a new string.
 *
 * @param s A string to copy.
 * @return A new copy of the string.
 ***/
String stringCopy(const char *s);

/***
 * Duplicate a string allocated with stringNew() or stringCopy().
 *
 * @param s A string allocated by stringNew() or stringCopy().
 * @return A new copy of the string.
 ***/
String stringDuplicate(String s);

/***
 * Check if 2 strings are equal.
 * The strings can be regular C strings as well.
 *
 * @param s1 A string
 * @param s2 Another string
 * @return true if equal, false if not
 ***/
bool stringEqual(char *s1, char *s2);

/***
 * Format a string with the arguments in 'ap' and return it.
 *
 * @param format The format string.
 * @param ap The vaargs.
 * @return A new String containing the formatted format string.
 ***/
String stringVFormat(const char *format, va_list ap);

/***
 * Format a string (using printf-like format specifiers) and return it.
 *
 * @param format The format string.
 * @return A new String containing the formatted format string.
 ***/
String stringFormat(const char *format, ...);

/***
 * Append format to dest (printf-like formatting supported)
 * NOTE: the string might be reallocated.
 *
 * @param dest the destination string.
 * @param format the string to append (printf-like format specifiers supported).
 ***/
void stringAppend(String *dest, const char *format, ...);

#endif // STRINGS_H
