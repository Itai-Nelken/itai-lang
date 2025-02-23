#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Compiler.h" // FileID
#include "Token.h"

Location locationNew(u64 start, u64 end, FileID file) {
    return (Location){
        .start = start,
        .end = end,
        .file = file
    };
}

Location locationMerge(Location a, Location b) {
    VERIFY(a.file == b.file);
    VERIFY(a.start < b.end);

    return locationNew(a.start, b.end, a.file);
}

Token tokenNew(TokenType type, Location location, char *lexeme, u32 length) {
    return (Token){
        .type = type,
        .location = location,
        .lexeme = lexeme,
        .length = length
    };
}

static const char *token_type_name(TokenType type) {
    static const char *names[] = {
        [TK_LPAREN]         = "TK_LPAREN",
        [TK_RPAREN]         = "TK_RPAREN",
        [TK_LBRACKET]       = "TK_LBRACKET",
        [TK_RBRACKET]       = "TK_RBRACKET",
        [TK_LBRACE]         = "TK_LBRACE",
        [TK_RBRACE]         = "TK_RBRACE",
        [TK_PLUS]           = "TK_PLUS",
        [TK_STAR]           = "TK_STAR",
        [TK_SLASH]          = "TK_SLASH",
        [TK_SEMICOLON]      = "TK_SEMICOLON",
        [TK_COLON]          = "TK_COLON",
        [TK_COMMA]          = "TK_COMMA",
        [TK_DOT]            = "TK_DOT",
        [TK_HASH]           = "TK_HASH",
        [TK_AMPERSAND]      = "TK_AMPERSAND",
        [TK_AND]            = "TK_AND",
        [TK_PIPE]           = "TK_PIPE",
        [TK_OR]             = "TK_OR",
        [TK_MINUS]          = "TK_MINUS",
        [TK_ARROW]          = "TK_ARROW",
        [TK_EQUAL]          = "TK_EQUAL",
        [TK_EQUAL_EQUAL]    = "TK_EQUAL_EQUAL",
        [TK_BANG]           = "TK_BANG",
        [TK_BANG_EQUAL]     = "TK_BANG_EQUAL",
        [TK_LESS]           = "TK_LESS",
        [TK_LESS_EQUAL]     = "TK_LESS_EQUAL",
        [TK_GREATER]        = "TK_GREATER",
        [TK_GREATER_EQUAL]  = "TK_GREATER_EQUAL",
        [TK_NUMBER_LITERAL] = "TK_NUMBER_LITERAL",
        [TK_STRING_LITERAL] = "TK_STRING_LITERAL",
        [TK_TRUE]           = "TK_TRUE",
        [TK_FALSE]          = "TK_FALSE",
        [TK_IF]             = "TK_IF",
        [TK_ELSE]           = "TK_ELSE",
        [TK_WHILE]          = "TK_WHILE",
        [TK_FN]             = "TK_FN",
        [TK_RETURN]         = "TK_RETURN",
        [TK_VAR]            = "TK_VAR",
        [TK_STRUCT]         = "TK_STRUCT",
        [TK_EXTERN]         = "TK_EXTERN",
        [TK_DEFER]          = "TK_DEFER",
        [TK_MODULE]         = "TK_MODULE",
        [TK_EXPECT]         = "TK_EXPECT",
        [TK_VOID]           = "TK_VOID",
        [TK_I32]            = "TK_I32",
        [TK_U32]            = "TK_U32",
        [TK_STR]            = "TK_STR",
        [TK_BOOL]           = "TK_BOOL",
        [TK_IDENTIFIER]     = "TK_IDENTIFIER",
        [TK_GARBAGE]        = "TK_GARBAGE",
        [TK_EOF]            = "TK_EOF"
    };
    return names[(i32)type];
}

void locationPrint(FILE *to, Location loc, bool compact) {
    Location empty = EMPTY_LOCATION;
    if(loc.start == empty.start && loc.end == empty.end && loc.file == empty.file) {
        fputs("Location{(empty)}", to);
        return;
    }
    if(compact) {
        fprintf(to, "Location{\x1b[34m%lu\x1b[0m..\x1b[34m%lu\x1b[0m @ \x1b[34m%zu\x1b[0m}", loc.start, loc.end, loc.file);
    } else {
        fprintf(to, "Location{\x1b[1mstart:\x1b[0;34m %lu\x1b[0m, \x1b[1mend:\x1b[0;34m %lu\x1b[0m, \x1b[1mfile:\x1b[0;34m %zu\x1b[0m}", loc.start, loc.end, loc.file);
    }
}

void tokenPrint(FILE *to, Token *t) {
    VERIFY(t);
    fprintf(to, "Token{\x1b[1mtype:\x1b[0;33m %s\x1b[0m, \x1b[1mlocation:\x1b[0m ", token_type_name(t->type));
    locationPrint(to, t->location, false);
    fprintf(to, ", \x1b[1mlexeme:\x1b[0m '%.*s'", t->length, t->lexeme);
    fprintf(to, ", \x1b[1mlength:\x1b[0;34m %u\x1b[0m", t->length);
    fprintf(to, "}");
}

const char *tokenTypeString(TokenType type) {
    static const char *strings[] = {
        [TK_LPAREN]         = "(",
        [TK_RPAREN]         = ")",
        [TK_LBRACKET]       = "[",
        [TK_RBRACKET]       = "]",
        [TK_LBRACE]         = "{",
        [TK_RBRACE]         = "}",
        [TK_PLUS]           = "+",
        [TK_STAR]           = "*",
        [TK_SLASH]          = "/",
        [TK_SEMICOLON]      = ";",
        [TK_COLON]          = ":",
        [TK_COMMA]          = ",",
        [TK_DOT]            = ".",
        [TK_HASH]           = "#",
        [TK_AMPERSAND]      = "&",
        [TK_AND]            = "&&",
        [TK_PIPE]           = "|",
        [TK_OR]             = "||",
        [TK_MINUS]          = "-",
        [TK_ARROW]          = "->",
        [TK_EQUAL]          = "=",
        [TK_EQUAL_EQUAL]    = "==",
        [TK_BANG]           = "!",
        [TK_BANG_EQUAL]     = "!=",
        [TK_LESS]           = "<",
        [TK_LESS_EQUAL]     = "<=",
        [TK_GREATER]        = ">",
        [TK_GREATER_EQUAL]  = ">=",
        [TK_NUMBER_LITERAL] = "<number literal>",
        [TK_STRING_LITERAL] = "<string literal>",
        [TK_TRUE]           = "true",
        [TK_FALSE]          = "false",
        [TK_IF]             = "if",
        [TK_ELSE]           = "else",
        [TK_WHILE]          = "while",
        [TK_FN]             = "fn",
        [TK_RETURN]         = "return",
        [TK_VAR]            = "var",
        [TK_STRUCT]         = "struct",
        [TK_EXTERN]         = "extern",
        [TK_DEFER]          = "defer",
        [TK_MODULE]         = "module",
        [TK_EXPECT]         = "expect",
        [TK_VOID]           = "void",
        [TK_I32]            = "i32",
        [TK_U32]            = "u32",
        [TK_STR]            = "str",
        [TK_BOOL]           = "bool",
        [TK_IDENTIFIER]     = "<identifier>",
        [TK_GARBAGE]        = "<garbage>",
        [TK_EOF]            = "<eof>"
    };
    return strings[(i32)type];
}
