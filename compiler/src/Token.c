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

NumberConstant numberConstantNewInt64(i64 value) {
    return (NumberConstant){
        .type = NUM_I64,
        .as.int64 = value
    };
}

Token tokenNew(TokenType type, Location location) {
    return (Token){
        .type = type,
        .location = location
    };
}

Token *tokenNewOperator(TokenType type, Location location) {
    Token *t;
    NEW0(t);
    *t = tokenNew(type, location);
    return t;
}

Token *tokenNewNumberConstant(Location location, NumberConstant value) {
    NumberConstantToken *t;
    NEW0(t);
    t->header = tokenNew(TK_NUMBER, location);
    t->value = value;
    return AS_TOKEN(t);
}

void tokenFree(Token *t) {
    assert(t);
    // handle special tokens.
    switch(t->type) {
        default:
            break;
    }
    FREE(t);
}

static const char *token_type_name(TokenType type) {
    static const char *names[] = {
        [TK_LPAREN] = "TK_LPAREN",
        [TK_RPAREN] = "TK_RPAREN",
        [TK_PLUS]   = "TK_PLUS",
        [TK_MINUS]  = "TK_MINUS",
        [TK_STAR]   = "TK_STAR",
        [TK_SLASH]  = "TK_SLASH",
        [TK_NUMBER] = "TK_NUMBER",
        [TK_EOF]    = "TK_EOF"
    };
    return names[(i32)type];
}

static const char *token_name(TokenType type) {
    switch(type) {
        case TK_NUMBER:
            return "NumberConstantToken";
        default:
            break;
    }
    return "Token";
}

static void print_location(FILE *to, Location loc) {
    fprintf(to, "Location{\x1b[1mstart:\x1b[0;34m %ld\x1b[0m, \x1b[1mend:\x1b[0;34m %ld\x1b[0m, \x1b[1mfile:\x1b[0;34m %zu\x1b[0m}", loc.start, loc.end, loc.file);
}

static void print_number_constant(FILE *to, NumberConstant value) {
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

void tokenPrint(FILE *to, Token *t) {
    assert(t);
    fprintf(to, "%s{\x1b[1mtype:\x1b[0;33m %s\x1b[0m, \x1b[1mlocation:\x1b[0m ", token_name(t->type), token_type_name(t->type));
    print_location(to, t->location);
    // handle special tokens
    switch(t->type) {
        case TK_NUMBER:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            print_number_constant(to, AS_NUMBER_CONSTANT_TOKEN(t)->value);
        default:
            break;
    }
    fprintf(to, "}");
}
