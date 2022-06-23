#include <string.h> // memcmp
#include <assert.h>
#include "Token.h" // Location
#include "Types.h"

Type newType(TypeType type, int id, Location loc) {
    return (Type){
        .type = type,
        .id = id,
        .location = loc
    };
}

Type newPrimitiveType(TypeType type, Location loc) {
    return newType(type, -1, loc);
}

// returns TY_NONE if not a builtin type
Type parseBuiltinTypeFromToken(Token tok) {
    switch (tok.type) {
        case TK_I8: return newPrimitiveType(TY_I8, tok.location);
        case TK_I16: return newPrimitiveType(TY_I16, tok.location);
        case TK_I32: return newPrimitiveType(TY_I32, tok.location);
        case TK_I64: return newPrimitiveType(TY_I64, tok.location);
        case TK_I128: return newPrimitiveType(TY_I128, tok.location);
        case TK_U8: return newPrimitiveType(TY_U8, tok.location);
        case TK_U16: return newPrimitiveType(TY_U16, tok.location);
        case TK_U32: return newPrimitiveType(TY_U32, tok.location);
        case TK_U64: return newPrimitiveType(TY_U64, tok.location);
        case TK_U128: return newPrimitiveType(TY_U128, tok.location);
        case TK_F32: return newPrimitiveType(TY_F32, tok.location);
        case TK_F64: return newPrimitiveType(TY_F64, tok.location);
        case TK_ISIZE: return newPrimitiveType(TY_ISIZE, tok.location);
        case TK_USIZE: return newPrimitiveType(TY_USIZE, tok.location);
        case TK_CHAR: return newPrimitiveType(TY_CHAR, tok.location);
        case TK_STR: return newPrimitiveType(TY_STR, tok.location);
        case TK_BOOL: return newPrimitiveType(TY_BOOL, tok.location);
        default:
            break;
    }
    return newPrimitiveType(TY_NONE, tok.location);
}
