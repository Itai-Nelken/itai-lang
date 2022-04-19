#ifndef STRINGS_H
#define STRINGS_H

#include <stdbool.h>

/***
 * Create a new empty string 'length' long
 * NOTE: strings returned by stringNew() can ONLY be freed with stringFree()
 *
 * @param length The length of the new string.
 * @return A pointer to a new heap allocated string.
 ***/
char *newString(int length);

/***
 * Free a string created by stringNew() or stringCopy();
 *
 * @param s A string created with stringNew() or stringCopy().
 ***/
void freeString(char *s);

/***
 * Check if a string is a valid string.
 *
 * @param s A string.
 * @return true if valid, false if not.
 ***/
bool stringIsValid(char *s);

/***
 * Return the length of a string allocated with stringNew() or stringCopy().
 * 
 * @param s A string allocated by stringNew() or stringCopy().
 * @return The length of the string.
 ***/
int stringLength(char *s);

/***
 * Resize a string allocated by stringNew() or stringCopy().
 * NOTE: if 'newSize' is smaller than the curren't length, data will be lost!
 *
 * @param s A string allocated by stringNew() or stringCopy().
 * @param newLength The new length. can't be 0.
 * @return A new string of length 'newLength'.
 ***/
char *stringResize(char *s, int newLength);

/***
 * Copy 'length' characters from 's' into a new string.
 *
 * @param s A string to copy.
 * @param length How much characters to copy.
 * @return A new copy of 'length' characters of 's'.
 ***/
char *stringNCopy(const char *s, int length);

/***
 * Copy a string into a new string.
 *
 * @param s A string to copy.
 * @return A new copy of the string.
 ***/
char *stringCopy(const char *s);

/***
 * Duplicate a string allocated with stringNew() or stringCopy().
 *
 * @param s A string allocated by stringNew() or stringCopy().
 * @return A new copy of the string.
 ***/
char *stringDuplicate(char *s);

/***
 * Check if 2 strings are equal.
 * The strings can be regular C strings as well.
 *
 * @param s1 A string
 * @param s2 Another string
 * @return true if equal, false if not
 ***/
bool stringEqual(char *s1, char *s2);

#endif // STRINGS_H
