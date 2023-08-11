#ifndef TYPE_COMMON_H
#define TYPE_COMMON_H

#include <stdbool.h>

typedef enum type_type {
    TY_VOID,
    TY_I32, TY_U32,
    TY_STR,
    TY_PTR,
    TY_FN,
    TY_STRUCT,
    TY_ID, // An identifier type is a place holder for types the parser can't resolve.
    TY_COUNT
} TypeType;

static inline const char *type_type_name(TypeType type) {
    static const char *names[] = {
        [TY_VOID]   = "TY_VOID",
        [TY_I32]    = "TY_I32",
        [TY_U32]    = "TY_U32",
        [TY_STR]    = "TY_STR",
        [TY_PTR]    = "TY_PTR",
        [TY_FN]     = "TY_FN",
        [TY_STRUCT] = "TY_STRUCT",
        [TY_ID]     = "TY_ID"
    };
    return names[type];
}

static inline bool typeIsNumeric(TypeType type) {
    switch(type) {
        case TY_I32:
        case TY_U32:
            return true;
        default:
            break;
    }
    return false;
}

static inline bool typeIsSigned(TypeType type) {
    VERIFY(typeIsNumeric(type));
    switch(type) {
        case TY_I32:
            return true;
        default:
            break;
    }
    return false;
}

static inline bool typeIsUnsigned(TypeType type) {
    VERIFY(typeIsNumeric(type));
    switch(type) {
        case TY_U32:
            return true;
        default:
            break;
    }
    return false;
}

static inline bool typeIsPrimitive(TypeType type) {
    switch(type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
            return true;
        default:
            break;
    }
    return false;
}

static inline bool typeIsFunction(TypeType type) {
    return type == TY_FN;
}

/** Utility macros that accept both types of Type (ParsedType & CheckedType) **/

#define IS_NUMERIC(ty) typeIsNumeric((ty)->type)

#define IS_UNSIGNED(ty) typeIsUnsigned((ty)->type)

#define IS_SIGNED(ty) typeIsSigned((ty)->type)

#define IS_PRIMITIVE(ty) typeIsPrimitive((ty)->type)

#define IS_FUNCTION(ty) typeIsFunction((ty)->type)

#endif // TYPE_COMMON_H
