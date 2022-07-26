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
static void add_error(Scanner *s, String message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, make_location(s), message);
    stringFree(message);
    compilerAddError(s->compiler, err);
}

static inline Token *make_operator_token(Scanner *s, TokenType type) {
    return tokenNewOperator(type, locationNew(s->start, s->current, compilerGetCurrentFileID(s->compiler)));
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

static Token *scan_number_constant(Scanner *s) {
    // TODO: add hex, octal, and binary literals.
    while(!is_end(s) && (isDigit(peek(s)) || peek(s) == '_')) {
        advance(s);
    }
    i64 value = strtol(&s->source[s->start], NULL, 10);
    return tokenNewNumberConstant(locationNew(s->start, s->current, compilerGetCurrentFileID(s->compiler)), numberConstantNewInt64(value));
}

static Token *scan_token(Scanner *s) {
    skip_whitespace(s);
    s->start = s->current;
    if(is_end(s)) {
        return make_operator_token(s, TK_EOF);
    }

    char c = advance(s);
    if(isDigit(c)) {
        return scan_number_constant(s);
    }

    switch(c) {
        case '(': return make_operator_token(s, TK_LPAREN);
        case ')': return make_operator_token(s, TK_RPAREN);
        case '+': return make_operator_token(s, TK_PLUS);
        case '-': return make_operator_token(s, TK_MINUS);
        case '*': return make_operator_token(s, TK_STAR);
        case '/': return make_operator_token(s, TK_SLASH);
        default:
            break;
    }
    add_error(s, stringFormat("Unknown character '%c'!", c));
    return NULL;
}

static void scan_file(Scanner *s, Array *tokens) {
    String contents = fileRead(compilerGetFile(s->compiler, compilerGetCurrentFileID(s->compiler)));
    if(contents == NULL) {
        add_error(s, stringFormat("Failed to read file '%s'!", compilerGetFile(s->compiler, compilerGetCurrentFileID(s->compiler))->path));
        return;
    }

    s->source = contents;
    Token *t = NULL;
    for(t = scan_token(s); !is_end(s); t = scan_token(s)) {
        if(!t) {
            continue;
        }
        if(t->type == TK_EOF) {
            tokenFree(t);
            break;
        }
        arrayPush(tokens, (void *)t);
    }
    s->source = NULL;
}

Array scannerScan(Scanner *s) {
    Array tokens; // Array<Token *>
    arrayInit(&tokens);
    while(compilerHasNextFile(s->compiler)) {
        compilerNextFile(s->compiler);
        scan_file(s, &tokens);
    }
    // push the EOF token as the last token.
    Token *t;
    NEW0(t);
    *t = tokenNew(TK_EOF, make_location(s));
    arrayPush(&tokens, (void *)t);
    return tokens;
}
