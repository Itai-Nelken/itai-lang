#include <stdarg.h>
#include "common.h"
#include "Table.h"
#include "Strings.h"
#include "Ast/StringTable.h"

/* Helper functions */

static void free_string_callback(void *string, void *cl) {
    UNUSED(cl);
    stringFree((String)string);
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

void stringTableInit(StringTable *st) {
    tableInit(&st->strings, NULL, NULL);
}

void stringTableFree(StringTable *st) {
    tableMap(&st->strings, free_string_callback, NULL);
    tableFree(&st->strings);
}

ASTString stringTableString(StringTable *st, const char *str) {
    String str = stringDuplicate(str);
    return add_string(&st->strings, str);
}

ASTString stringTableFormat(StringTable *st, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String str = stringVFormat(format, ap);
    va_end(ap);

    return add_string(&st->strings, str);
}
