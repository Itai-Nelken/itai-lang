#include <stdio.h>
#include <assert.h>
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
    assert(a.file == b.file);
    assert(a.start < b.end);

    return locationNew(a.start, b.end, a.file);
}

NumberConstant numberConstantNewInt64(i64 value) {
    return (NumberConstant){
        .type = NUM_I64,
        .as.int64 = value
    };
}

Token tokenNew(TokenType type, Location location) {
    Token tk = {0};
    tk.type = type;
    tk.location = location;
    return tk;
}


Token tokenNewNumberConstant(Location location, NumberConstant value) {
    Token tk = tokenNew(TK_NUMBER, location);
    tk.as.number_constant = value;
    return tk;
}

static const char *token_type_name(TokenType type) {
    static const char *names[] = {
        [TK_LPAREN]      = "TK_LPAREN",
        [TK_RPAREN]      = "TK_RPAREN",
        [TK_LBRACE]      = "TK_LBRACE",
        [TK_RBRACE]      = "TK_RBRACE",
        [TK_PLUS]        = "TK_PLUS",
        [TK_STAR]        = "TK_STAR",
        [TK_SLASH]       = "TK_SLASH",
        [TK_SEMICOLON]   = "TK_SEMICOLON",
        [TK_COLON]       = "TK_COLON",
        [TK_MINUS]       = "TK_MINUS",
        [TK_ARROW]       = "TK_ARROW",
        [TK_EQUAL]       = "TK_EQUAL",
        [TK_EQUAL_EQUAL] = "TK_EQUAL_EQUAL",
        [TK_BANG]        = "TK_BANG",
        [TK_BANG_EQUAL]  = "TK_BANG_EQUAL",
        [TK_NUMBER]      = "TK_NUMBER",
        [TK_IF]          = "TK_IF",
        [TK_ELSE]        = "TK_ELSE",
        [TK_WHILE]       = "TK_WHILE",
        [TK_FN]          = "TK_FN",
        [TK_RETURN]      = "TK_RETURN",
        [TK_VAR]         = "TK_VAR",
        [TK_I32]         = "TK_I32",
        [TK_IDENTIFIER]  = "TK_IDENTIFIER",
        [TK_GARBAGE]     = "TK_GARBAGE",
        [TK_EOF]         = "TK_EOF"
    };
    return names[(i32)type];
}

void printNumberConstant(FILE *to, NumberConstant value) {
    fprintf(to, "NumberConstant{");
    switch(value.type) {
        case NUM_I64:
            fprintf(to, "\x1b[1mas.int64:\x1b[0;34m %ld\x1b[0m", value.as.int64);
            break;
        default:
            UNREACHABLE();
    }
    fprintf(to, "}");
}

void printLocation(FILE *to, Location loc) {
    fprintf(to, "Location{\x1b[1mstart:\x1b[0;34m %ld\x1b[0m, \x1b[1mend:\x1b[0;34m %ld\x1b[0m, \x1b[1mfile:\x1b[0;34m %zu\x1b[0m}", loc.start, loc.end, loc.file);
}

void tokenPrint(FILE *to, Token *t) {
    assert(t);
    fprintf(to, "Token{\x1b[1mtype:\x1b[0;33m %s\x1b[0m, \x1b[1mlocation:\x1b[0m ", token_type_name(t->type));
    printLocation(to, t->location);
    // handle special tokens
    switch(t->type) {
        case TK_NUMBER:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            printNumberConstant(to, t->as.number_constant);
            break;
        case TK_IDENTIFIER:
            fprintf(to, ", \x1b[1midentifier:\x1b[0m %.*s", t->as.identifier.length, t->as.identifier.text);
            break;
        default:
            break;
    }
    fprintf(to, "}");
}

const char *tokenTypeString(TokenType type) {
    static const char *strings[] = {
        [TK_LPAREN]      = "(",
        [TK_RPAREN]      = ")",
        [TK_LBRACE]      = "{",
        [TK_RBRACE]      = "}",
        [TK_PLUS]        = "+",
        [TK_STAR]        = "*",
        [TK_SLASH]       = "/",
        [TK_SEMICOLON]   = ";",
        [TK_COLON]       = ":",
        [TK_MINUS]       = "-",
        [TK_ARROW]       = "->",
        [TK_EQUAL]       = "=",
        [TK_EQUAL_EQUAL] = "==",
        [TK_BANG]        = "!",
        [TK_BANG_EQUAL]  = "!=",
        [TK_NUMBER]      = "<number>",
        [TK_IF]          = "if",
        [TK_ELSE]        = "else",
        [TK_WHILE]       = "while",
        [TK_FN]          = "fn",
        [TK_RETURN]      = "return",
        [TK_VAR]         = "var",
        [TK_I32]         = "i32",
        [TK_IDENTIFIER]  = "<identifier>",
        [TK_GARBAGE]     = "<garbage>",
        [TK_EOF]         = "<eof>"
    };
    return strings[(i32)type];
}
