#ifndef TYPES_H
#define TYPES_H

#include "Token.h" // Location

typedef enum type_type {
    TY_I8,
    TY_I16,
    TY_I32,
    TY_I64,
    TY_I128,
    TY_U8,
    TY_U16,
    TY_U32,
    TY_U64,
    TY_U128,
    TY_F32,
    TY_F64,
    TY_ISIZE,
    TY_USIZE,
    TY_CHAR,
    TY_STR,
    TY_BOOL,
    TY_CUSTOM,
    TY_NONE,
    TY_COUNT
} TypeType;

typedef struct type {
    TypeType type;
    int id;
    Location location; 
} Type;

Type newType(TypeType type, int id, Location loc);
Type newPrimitiveType(TypeType type, Location loc);
Type newEmptyType(Location loc);
Type parseBuiltinTypeFromToken(Token tok);
void printType(Type ty);

#endif // TYPES_H
