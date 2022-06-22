#include <string.h> // memcmp
#include <assert.h>
#include "Token.h" // Location
#include "Types.h"

Type newType(TypeType type, Location loc) {
    return (Type){
        .type = type,
        .location = loc
    };
}

// return TY_NONE if not a builtin type
Type parseBuiltinTypeFromToken(Token tok) {
    switch (tok.type) {
        case TK_I8: return newType(TY_I8, tok.location);
        case TK_I16: return newType(TY_I16, tok.location);
        case TK_I32: return newType(TY_I32, tok.location);
        case TK_I64: return newType(TY_I64, tok.location);
        case TK_I128: return newType(TY_I128, tok.location);
        case TK_U8: return newType(TY_U8, tok.location);
        case TK_U16: return newType(TY_U16, tok.location);
        case TK_U32: return newType(TY_U32, tok.location);
        case TK_U64: return newType(TY_U64, tok.location);
        case TK_U128: return newType(TY_U128, tok.location);
        case TK_F32: return newType(TY_F32, tok.location);
        case TK_F64: return newType(TY_F64, tok.location);
        case TK_ISIZE: return newType(TY_ISIZE, tok.location);
        case TK_USIZE: return newType(TY_USIZE, tok.location);
        case TK_CHAR: return newType(TY_CHAR, tok.location);
        case TK_STR: return newType(TY_STR, tok.location);
        case TK_BOOL: return newType(TY_BOOL, tok.location);
        default:
            break;
    }
    return newType(TY_NONE, tok.location);
}
