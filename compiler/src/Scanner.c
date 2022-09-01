#include <string.h> // memcmp
#include <assert.h>
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "utilities.h"
#include "Compiler.h"
#include "Error.h"
#include "Array.h"
#include "Strings.h"
#include "Token.h"
#include "Scanner.h"

void scannerInit(Scanner *s, Compiler *c) {
    s->compiler = c;
    s->source = NULL;
    s->start = s->current = 0;
    s->failed_to_set_source = false;
}

void scannerFree(Scanner *s) {
    s->compiler = NULL;
    s->source = NULL;
    s->start = s->current = 0;
}

static inline Location make_location(Scanner *s) {
    return locationNew(s->start, s->current, compilerGetCurrentFileID(s->compiler));
}

// Takes ownership 'message'
static void add_error(Scanner *s, bool has_location, String message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, has_location, has_location ? make_location(s) : locationNew(0, 0, 0), message);
    stringFree(message);
    compilerAddError(s->compiler, err);
}

static inline Token make_simple_token(Scanner *s, TokenType type) {
    return tokenNew(type, locationNew(s->start, s->current, compilerGetCurrentFileID(s->compiler)));
}

static inline bool is_end(Scanner *s) {
    return s->source[s->current] == '\0';
}

static inline char advance(Scanner *s) {
    return s->source[s->current++];
}

static inline char peek(Scanner *s) {
    return s->source[s->current];
}

static inline char peek_next(Scanner *s) {
    if(s->current + 1 > stringLength(s->source)) {
        return '\0';
    }
    return s->source[s->current + 1];
}

static bool match(Scanner *s, char c) {
    if(peek(s) != c) {
        return false;
    }
    advance(s);
    return true;
}

static void skip_whitespace(Scanner *s) {
    for(;;) {
        switch(peek(s)) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                advance(s);
                break;
            case '/':
                if(peek_next(s) == '/') {
                    // comment, skip until end of line.
                    while(!is_end(s) && peek(s) != '\n') {
                        advance(s);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token scan_number_constant(Scanner *s) {
    // TODO: add hex, octal, and binary literals.
    while(!is_end(s) && (isDigit(peek(s)) || peek(s) == '_')) {
        advance(s);
    }
    i64 value = strtol(&s->source[s->start], NULL, 10);
    return tokenNewNumberConstant(locationNew(s->start, s->current, compilerGetCurrentFileID(s->compiler)), numberConstantNewInt64(value));
}

static TokenType scan_keyword_or_identifier_type(Scanner *s) {
    while(!is_end(s) && (isAscii(peek(s)) || isDigit(peek(s)) || peek(s) == '_')) {
        advance(s);
    }
    char *lexeme = s->source + s->start;
    u32 length = (u32)(s->current - s->start);

    TokenType result;
    switch(*lexeme) {
        case 'i':
            switch(length) {
                case 2:
                    result =  memcmp(lexeme, "if", 2) == 0 ? TK_IF : TK_IDENTIFIER;
                    break;
                case 3:
                    result =  memcmp(lexeme, "i32", 3) == 0 ? TK_I32 : TK_IDENTIFIER;
                    break;
                default:
                    result = TK_IDENTIFIER;
                    break;
            }
            break;
        case 'e':
            result = (length == 4 && memcmp(lexeme, "else", 4) == 0) ? TK_ELSE : TK_IDENTIFIER;
            break;
        case 'w':
            result = (length == 5 && memcmp(lexeme, "while", 5) == 0) ? TK_WHILE : TK_IDENTIFIER;
            break;
        case 'f':
            result = (length == 2 && memcmp(lexeme, "fn", 2) == 0) ? TK_FN : TK_IDENTIFIER;
            break;
        case 'r':
            result = (length == 6 && memcmp(lexeme, "return", 6) == 0) ? TK_RETURN : TK_IDENTIFIER;
            break;
        default:
            result = TK_IDENTIFIER;
            break;
    }
    return result;
}

Token scan_token(Scanner *s) {
    skip_whitespace(s);
    s->start = s->current;
    if(is_end(s)) {
        return make_simple_token(s, TK_EOF);
    }

    char c = advance(s);
    if(isDigit(c)) {
        return scan_number_constant(s);
    }
    if(isAscii(c) || c == '_') {
        Token tk = make_simple_token(s, scan_keyword_or_identifier_type(s));
        tk.as.identifier.text = s->source + s->start;
        tk.as.identifier.length = (u32)(s->current - s->start);
        return tk;
    }

    switch(c) {
        case '(': return make_simple_token(s, TK_LPAREN);
        case ')': return make_simple_token(s, TK_RPAREN);
        case '{': return make_simple_token(s, TK_LBRACE);
        case '}': return make_simple_token(s, TK_RBRACE);
        case '+': return make_simple_token(s, TK_PLUS);
        case '*': return make_simple_token(s, TK_STAR);
        case '/': return make_simple_token(s, TK_SLASH);
        case ';': return make_simple_token(s, TK_SEMICOLON);
        case '-': return make_simple_token(s, match(s, '>') ? TK_ARROW : TK_MINUS);
        case '=': return make_simple_token(s, match(s, '=') ? TK_EQUAL_EQUAL : TK_EQUAL);
        case '!': return make_simple_token(s, match(s, '=') ? TK_BANG_EQUAL : TK_BANG);
        default:
            break;
    }
    add_error(s, true, stringFormat("Unknown character '%c'!", c));
    return make_simple_token(s, TK_GARBAGE);
}

static bool set_source(Scanner *s, FileID file) {
    File *f = compilerGetFile(s->compiler, file);
    assert(f);
    String contents = fileRead(f);
    if(contents == NULL) {
        add_error(s, false, stringFormat("Failed to read file '%s'!", compilerGetFile(s->compiler, compilerGetCurrentFileID(s->compiler))->path));
        return false;
    }
    s->source = contents;
    return true;
}

Token scannerNextToken(Scanner *s) {
    if(s->failed_to_set_source) {
        // if we failed to set the source, we can't do anything.
        // the error was already reported, so we will simplt return
        // TK_EOF so the parser thinks we are at the end of the input
        // and will exit so errors can be printed.
        return make_simple_token(s, TK_EOF);
    }
    if(!s->source) {
        if(!set_source(s, compilerNextFile(s->compiler))) {
            s->failed_to_set_source = true;
            // if we can't set the source, assume we are at the end
            // of the input.
            return make_simple_token(s, TK_EOF);
        }
    }
    Token tk = scan_token(s);
    if(tk.type == TK_EOF && compilerHasNextFile(s->compiler)) {
        set_source(s, compilerNextFile(s->compiler));
        tk = scan_token(s);
    }
    return tk;
}
