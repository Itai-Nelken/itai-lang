#ifndef STRING_TABLE_H
#define STRING_TABLE_H

#include <stdio.h>
#include "Strings.h"
#include "Table.h"

/**
 * A StringTable holds a single copy of each string put in it.
 * Attempting to add a string that already exists will return the already existing string.
 * StringTable owns all its strings. The only way to add a string is to create it with one of the provided methods.
 *
 * StringTable operates on ASTStrings which are used only to differentiate interned strings (strings in the table).
 * An ASTString is an alias to a String.
 * @see Strings.h#String
 **/

typedef String ASTString;

typedef struct string_table {
    Table strings; // Table<ASTString, void>
} StringTable;


/**
 * Pretty print a string table.
 *
 * @param to The stream to print to.
 * @param st The StringTable to print.
 **/
void stringTablePrint(FILE *to, StringTable *st);

/**
 * Initialize a StringTable.
 *
 * @param st The StringTable to initialize.
 **/
void stringTableInit(StringTable *st);

/**
 * Free a StringTable (frees all the strings it owns).
 *
 * @param st The StringTable to free.
 **/
void stringTableFree(StringTable *st);

/**
 * Add a string to a StringTable if it doesn't exist yet.
 *
 * @param st The StringTable to add the string to.
 * @param str The string to add (must be nul terminated).
 * @return The interned string.
 **/
ASTString stringTableString(StringTable *st, char *str);

/**
 * Add a formatted string to a StringTable.
 *
 * @param st The StringTable to add the string to.
 * @param format The format string (printf-like format specifiers supported).
 **/
ASTString stringTableFormat(StringTable *st, const char *format, ...);

#endif // STRING_TABLE_H
