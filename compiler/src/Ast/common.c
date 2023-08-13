#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Token.h" // Location
#include "Table.h"
#include "Strings.h"
#include "Ast/ast_common.h"


/* callbacks */

static void free_string_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    stringFree((String)item->value);
}

static void print_string_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    String s = (String)item->key;
    fprintf(to, "\"%s\"", s);
    if(!is_last) {
        fputs(", ", to);
    }
}


/* ASTString */

void astStringPrint(FILE *to, ASTString *str) {
    fputs("ASTString{", to);
    locationPrint(to, str->location, true);
    fprintf(to, ", '%s'}", str->data);
}


/* ASTStringTable */

void astStringTableInit(ASTStringTable *st) {
    tableInit(&st->strings, NULL, NULL);
}

void astStringTableFree(ASTStringTable *st) {
    tableMap(&st->strings, free_string_callback, NULL);
    tableFree(&st->strings);
}

ASTInternedString astStringTableAddString(ASTStringTable *st, char *str) {
    String string = stringCopy(str);

    TableItem *existing_str = tableGet(&st->strings, (void *)string);
    if(existing_str != NULL) {
        stringFree(string);
        return (ASTInternedString)existing_str->value;
    }
    tableSet(&st->strings, (void *)string, (void *)string);
    return (ASTInternedString)string;
}

void astStringTablePrint(FILE *to, ASTStringTable *st) {
    fputs("ASTStringTable{", to);
    tableMap(&st->strings, print_string_table_callback, (void *)to);
    fputs("}", to);
}


/* Value */

void valuePrint(FILE *to, Value value) {
    fprintf(to, "Value{\x1b[1mvalue:\x1b[0m ");
    switch(value.type) {
        case VAL_NUMBER:
            fprintf(to, "\x1b[34m%lu\x1b[0m", value.as.number);
            break;
        case VAL_STRING:
            astStringPrint(to, &value.as.string);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


/* ScopeID */

void scopeIDPrint(FILE *to, ScopeID scope_id, bool compact) {
    if(compact) {
        fprintf(to, "ScopeID{\x1b[1mm:\x1b[0;34m%zu\x1b[0m,\x1b[1mi:\x1b[0;34m%zu\x1b[0m}", scope_id.module, scope_id.index);
    } else {
        fprintf(to, "ScopeID{\x1b[1mmodule:\x1b[0;34m %zu\x1b[0m, \x1b[1mindex:\x1b[0;34m %zu\x1b[0m}", scope_id.module, scope_id.index);
    }
}


/* ControlFlow */

ControlFlow controlFlowUpdate(ControlFlow old, ControlFlow new) {
    if(old == new) {
        return old;
    }
    if(old == CF_MAY_RETURN || new == CF_MAY_RETURN) {
        return CF_MAY_RETURN;
    }
    if((old == CF_NEVER_RETURNS && new == CF_ALWAYS_RETURNS) ||
       (old == CF_ALWAYS_RETURNS && new == CF_NEVER_RETURNS)) {
        return CF_MAY_RETURN;
    }
    UNREACHABLE();
}

void controlFlowPrint(FILE *to, ControlFlow cf) {
    fprintf(to, "ControlFlow{\x1b[1;34m%s\x1b[0m}", ((const char *[__CF_STATE_COUNT]){
        [CF_NONE]           = "NONE",
        [CF_NEVER_RETURNS]  = "NEVER_RETURNS",
        [CF_MAY_RETURN]     = "MAY_RETURN",
        [CF_ALWAYS_RETURNS] = "ALWAYS_RETURNS",
    }[cf]));
}


/* Attribute */

Attribute *attributeNew(AttributeType type, Location loc) {
    Attribute *attr;
    NEW0(attr);
    attr->type = type;
    attr->location = loc;
    return attr;
}

void attributeFree(Attribute *a) {
    FREE(a);
}

static const char *attribute_type_name(AttributeType type) {
    static const char *types[] = {
        [ATTR_SOURCE] = "ATTR_SOURCE",
    };
    return types[(u32)type];
}

void attributePrint(FILE *to, Attribute *a) {
    fprintf(to, "Attribute{\x1b[1mtype: \x1b[0;35m%s\x1b[0m, \x1b[1mlocation:\x1b[0m ", attribute_type_name(a->type));
    locationPrint(to, a->location, true);
    switch(a->type) {
        case ATTR_SOURCE:
            fputs(", ", to);
            astStringPrint(to, &a->as.source);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

const char *attributeTypeString(AttributeType type) {
    static const char *types[] = {
        [ATTR_SOURCE] = "source",
    };
    return types[(u32)type];
}


/* ASTObj - common */

const char *__ast_obj_type_name(ASTObjType type) {
    static const char *names[] = {
        [OBJ_VAR]       = "OBJ_VAR",
        [OBJ_FN]        = "OBJ_FN",
        [OBJ_STRUCT]    = "OBJ_STRUCT",
        [OBJ_EXTERN_FN] = "OBJ_EXTERN_FN"
    };
    return names[type];
}
