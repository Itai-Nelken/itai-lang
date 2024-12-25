#include <stdarg.h>
#include <string.h>
#include "common.h"
#include "Table.h"
#include "Strings.h"
#include "Ast/StringTable.h"

/* Helper functions */

static void free_string_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    stringFree((String)item->key);
}

static void print_string_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    fprintf(to, "\"%s\"", (char *)item->key);
    if(!is_last) {
        fputs(", ", to);
    }
}

// Note: takes ownership of [str].
static ASTString add_string(Table *strings, String str) {
    TableItem *item;
    if((item = tableGet(strings, (void *)str)) != NULL) {
        stringFree(str);
        return (ASTString)item->key;
    }
    tableSet(strings, (void *)str, NULL);
    return (ASTString)str; // cast is uneccesary, only used to signify that from now on the string is an ASTString.
}


/* StringTable functions */

void stringTablePrint(FILE *to, StringTable *st) {
    if(!st) {
        fputs("(null)", to);
        return;
    }

    fputs("StringTable{\x1b[1mstrings:\x1b[0m [", to);
    tableMap(&st->strings, print_string_callback, (void *)to);
    fputs("]}", to);
}

void stringTableInit(StringTable *st) {
    tableInit(&st->strings, NULL, NULL);
}

void stringTableFree(StringTable *st) {
    tableMap(&st->strings, free_string_callback, NULL);
    tableFree(&st->strings);
}

ASTString stringTableString(StringTable *st, char *str) {
    String s = stringDuplicate(str);
    return add_string(&st->strings, s);
}

ASTString stringTableFormat(StringTable *st, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String str = stringVFormat(format, ap);
    va_end(ap);

    return add_string(&st->strings, str);
}
