#include <stdio.h>
#include "Token.h"

Token newToken(TokenType type, Location loc, char *lexeme, int length) {
    Token t = {
        .type = type,
        .location = loc,
        .lexeme = lexeme,
        .length = length
    };
    return t;
}

Token newErrorToken(Location loc, char *lexeme, int length, const char *message) {
    Token t = newToken(TK_ERROR, loc, lexeme, length);
    t.errmsg = message;
    return t;
}

void printToken(Token t) {
    printf("Token{\x1b[1mtype:\x1b[33m %s\x1b[0m, "
           "\x1b[34;1mlocation\x1b[0m{\x1b[1mfile: \x1b[0;33m'%s'\x1b[0m, "
                "\x1b[1mcontaining_line:\x1b[0;33m'%.*s'\x1b[0m, "
                "\x1b[1mline_length:\x1b[0;33m %d\x1b[0m, "
                "\x1b[1mline: \x1b[0;36m%d\x1b[0m, "
                "\x1b[1mat: \x1b[0;36m%d\x1b[0m}, "
            "\x1b[1mlexeme:\x1b[0;33m '%.*s'\x1b[0m, "
            "\x1b[1mlength:\x1b[0;36m %d\x1b[0m}\n",
        tokenTypeToString(t.type),
        t.location.file,
        t.location.line_length, t.location.containing_line,
        t.location.line_length,
        t.location.line,
        t.location.at,
        t.length, t.lexeme,
        t.length);
}

// lut == lookup table
static const char *tokentype_str_lut[] = {
    [TK_LPAREN]          = "TK_LPAREN",
    [TK_RPAREN]          = "TK_RPAREN",
    [TK_LBRACKET]        = "TK_LBRACKET",
    [TK_RBRACKET]        = "TK_RBRACKET",
    [TK_LBRACE]          = "TK_LBRACE",
    [TK_RBRACE]          = "TK_RBRACE",
    [TK_COMMA]           = "TK_COMMA",
    [TK_SEMICOLON]       = "TK_SEMICOLON",
    [TK_COLON]           = "TK_COLON",
    [TK_TILDE]           = "TK_TILDE",

    // one or two character tokens
    [TK_MINUS]           = "TK_MINUS",
    [TK_MINUS_EQUAL]     = "TK_MINUS_EQUAL",
    [TK_ARROW]           = "TK_ARROW", // ->
    [TK_PLUS]            = "TK_PLUS",
    [TK_PLUS_EQUAL]      = "TK_PLUS_EQUAL",
    [TK_SLASH]           = "TK_SLASH",
    [TK_SLASH_EQUAL]     = "TK_SLASH_EQUAL",
    [TK_STAR]            = "TK_STAR",
    [TK_STAR_EQUAL]      = "TK_STAR_EQUAL",
    [TK_BANG]            = "TK_BANG",
    [TK_BANG_EQUAL]      = "TK_BANG_EQUAL",
    [TK_EQUAL]           = "TK_EQUAL",
    [TK_EQUAL_EQUAL]     = "TK_EQUAL_EQUAL",
    [TK_PERCENT]         = "TK_PERCENT",
    [TK_PERCENT_EQUAL]   = "TK_PERCENT_EQUAL",
    [TK_XOR]             = "TK_XOR",
    [TK_XOR_EQUAL]       = "TK_XOR_EQUAL",
    [TK_PIPE]            = "TK_PIPE",
    [TK_PIPE_EQUAL]      = "TK_PIPE_EQUAL",
    [TK_AMPERSAND]       = "TK_AMPERSAND",
    [TK_AMPERSAND_EQUAL] = "TK_AMPERSAND_EQUAL",

    // one, two, or three character tokens
    [TK_GREATER]         = "TK_GREATER",
    [TK_GREATER_EQUAL]   = "TK_GREATER_EQUAL",
    [TK_RSHIFT]          = "TK_RSHIFT",
    [TK_RSHIFT_EQUAL]    = "TK_RSHIFT_EQUAL",
    [TK_LESS]            = "TK_LESS",
    [TK_LESS_EQUAL]      = "TK_LESS_EQUAL",
    [TK_LSHIFT]          = "TK_LSHIFT",
    [TK_LSHIFT_EQUAL]    = "TK_LSHIFT_EQUAL",
    [TK_DOT]             = "TK_DOT",
    [TK_ELIPSIS]             = "TK_ELIPSIS",

    // literals
    [TK_STRLIT]          = "TK_STRLIT",
    [TK_CHARLIT]         = "TK_CHARLIT",
    [TK_NUMLIT]          = "TK_NUMLIT",
    [TK_IDENTIFIER]      = "TK_IDENTIFIER",

    // types
    [TK_I8]              = "TK_I8",
    [TK_I16]             = "TK_I16",
    [TK_I32]             = "TK_I32",
    [TK_I64]             = "TK_I64",
    [TK_I128]            = "TK_I128",
    [TK_U8]              = "TK_U8",
    [TK_U16]             = "TK_U16",
    [TK_U32]             = "TK_U32",
    [TK_U64]             = "TK_U64",
    [TK_U128]            = "TK_U128",
    [TK_F32]             = "TK_F32",
    [TK_F64]             = "TK_F64",
    [TK_ISIZE]           = "TK_ISIZE",
    [TK_USIZE]           = "TK_USIZE",
    [TK_CHAR]            = "TK_CHAR",
    [TK_STR]             = "TK_STR",

    // keywords
    [TK_VAR] = "TK_VAR",
    [TK_CONST] = "TK_CONST",
    [TK_STATIC] = "TK_STATIC",
    [TK_FN] = "TK_FN",
    [TK_RETURN] = "TK_RETURN",
    [TK_ENUM] = "TK_ENUM",
    [TK_STRUCT] = "TK_STRUCT",
    [TK_IF] = "TK_IF",
    [TK_ELSE] = "TK_ELSE",
    [TK_SWITCH] = "TK_SWITCH",
    [TK_MODULE] = "TK_MODULE",
    [TK_EXPORT] = "TK_EXPORT",
    [TK_IMPORT] = "TK_IMPORT",
    [TK_AS] = "TK_AS",
    [TK_USING] = "TK_USING",
    [TK_WHILE] = "TK_WHILE",
    [TK_FOR] = "TK_FOR",
    [TK_TYPE] = "TK_TYPE",
    [TK_NULL] = "TK_NULL",
    [TK_TYPEOF] = "TK_TYPEOF",

    // other
    [TK_ERROR] = "TK_ERROR",
    [TK_EOF] = "TK_EOF"
};

inline const char *tokenTypeToString(TokenType type) {
    if(type > TK_EOF + 1) {
        return "UNKNOWN";
    }
    return tokentype_str_lut[type];
}
