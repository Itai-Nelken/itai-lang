#include <string.h> // memcmp
#include "common.h"
#include "memory.h"
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

// Note: ownership of 'message' is taken.
static void add_error(Scanner *s, bool has_location, String message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, has_location, has_location ? make_location(s) : EMPTY_LOCATION, message);
    stringFree(message);
    compilerAddError(s->compiler, err);
}

static inline Token make_token(Scanner *s, TokenType type) {
    return tokenNew(type, locationNew(s->start, s->current, compilerGetCurrentFileID(s->compiler)), s->source + s->start, (u32)(s->current - s->start));
}

static inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool isAscii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
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
                } else if(peek_next(s) == '*') {
                    // multiline comments, skip until '*/' (handling nested multiline comments).
                    u8 depth = 1;
                    advance(s); // consume the first '/'.
                    while(!is_end(s) && depth > 0) {
                        if(peek(s) == '/' && peek_next(s) == '*') {
                            advance(s);
                            depth++;
                        } else if(peek(s) == '*' && peek_next(s) == '/') {
                            advance(s);
                            depth--;
                        }
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

static void scan_number_literal(Scanner *s) {
    // TODO: add hex, octal, and binary literals.
    while(!is_end(s) && (isDigit(peek(s)) || peek(s) == '_')) {
        advance(s);
    }
}

static void scan_string_literal(Scanner *s) {
    while(!is_end(s) && peek(s) != '"') {
        // Allow escaped double-quotes (e.g. "value=\"%s\"")
        if(peek(s) == '\\' && peek_next(s) == '"') {
            advance(s);
        }
        advance(s);
    }
    advance(s); // consume the closing '"'.
}

static TokenType scan_keyword_or_identifier_type(Scanner *s) {
    while(!is_end(s) && (isAscii(peek(s)) || isDigit(peek(s)) || peek(s) == '_')) {
        advance(s);
    }
    char *lexeme = s->source + s->start;
    u32 length = (u32)(s->current - s->start);
    #define CHECK(expected, expected_length, expected_type) (length == (expected_length) && memcmp(lexeme, (expected), (expected_length)) == 0 ? (expected_type) : TK_IDENTIFIER)

    TokenType result;
    switch(*lexeme) {
        case 'b':
            result = CHECK("bool", 4, TK_BOOL);
            break;
        case 'd':
            result = CHECK("defer", 5, TK_DEFER);
            break;
        case 'e':
            switch(length) {
                case 4:
                    result = CHECK("else", 4, TK_ELSE);
                    break;
                case 6:
                    // 'exTern'
                    // 'exPect'
                    //    ^- index 2.
                    switch(lexeme[2]) {
                        case 't':
                            result = CHECK("extern", 6, TK_EXTERN);
                            break;
                        case 'p':
                            result = CHECK("expect", 6, TK_EXPECT);
                            break;
                    }
                    break;
                default:
                    result = TK_IDENTIFIER;
                    break;
            }
            break;
        case 'f':
            switch(length) {
                case 2:
                    result = CHECK("fn", 2, TK_FN);
                    break;
                case 5:
                    result = CHECK("false", 5, TK_FALSE);
                    break;
                default:
                    result = TK_IDENTIFIER;
                    break;
            }
            break;
        case 'i':
            switch(length) {
                case 2:
                    result =  CHECK("if", 2, TK_IF);
                    break;
                case 3:
                    result =  CHECK("i32", 3, TK_I32);
                    break;
                case 6:
                    result = CHECK("import", 6, TK_IMPORT);
                    break;
                default:
                    result = TK_IDENTIFIER;
                    break;
            }
            break;
        case 'r':
            result = CHECK("return", 6, TK_RETURN);
            break;
        case 's':
            switch(length) {
                case 6:
                    result = CHECK("struct", 6, TK_STRUCT);
                    break;
                case 3:
                    result = CHECK("str", 3, TK_STR);
                    break;
                default:
                    result = TK_IDENTIFIER;
                    break;
            }
            break;
        case 't':
            result = CHECK("true", 4, TK_TRUE);
            break;
        case 'u':
            result = CHECK("u32", 3, TK_U32);
            break;
        case 'v':
            switch(length) {
                case 3:
                    result = CHECK("var", 3, TK_VAR);
                    break;
                case 4:
                    result = CHECK("void", 4, TK_VOID);
                    break;
                default:
                    result = TK_IDENTIFIER;
                    break;
            }
            break;
        case 'w':
            result = CHECK("while", 5, TK_WHILE);
            break;
        default:
            result = TK_IDENTIFIER;
            break;
    }
    #undef CHECK
    return result;
}

Token scan_token(Scanner *s) {
    skip_whitespace(s);
    s->start = s->current;
    if(is_end(s)) {
        return make_token(s, TK_EOF);
    }

    char c = advance(s);
    if(isDigit(c)) {
        scan_number_literal(s);
        return make_token(s, TK_NUMBER_LITERAL);
    }
    if(isAscii(c) || c == '_') {
        Token tk = make_token(s, scan_keyword_or_identifier_type(s));
        return tk;
    }

    switch(c) {
        case '(': return make_token(s, TK_LPAREN);
        case ')': return make_token(s, TK_RPAREN);
        case '[': return make_token(s, TK_LBRACKET);
        case ']': return make_token(s, TK_RBRACKET);
        case '{': return make_token(s, TK_LBRACE);
        case '}': return make_token(s, TK_RBRACE);
        case '+': return make_token(s, TK_PLUS);
        case '*': return make_token(s, TK_STAR);
        case '/': return make_token(s, TK_SLASH);
        case ';': return make_token(s, TK_SEMICOLON);
        case ':': return make_token(s, match(s, ':') ? TK_SCOPE_RESOLUTION : TK_COLON);
        case ',': return make_token(s, TK_COMMA);
        case '.': return make_token(s, TK_DOT);
        case '#': return make_token(s, TK_HASH);
        case '&': return make_token(s, match(s, '&') ? TK_AND : TK_AMPERSAND);
        case '-': return make_token(s, match(s, '>') ? TK_ARROW : TK_MINUS);
        case '=': return make_token(s, match(s, '=') ? TK_EQUAL_EQUAL : TK_EQUAL);
        case '|': return make_token(s, match(s, '|') ? TK_OR : TK_PIPE);
        case '!': return make_token(s, match(s, '=') ? TK_BANG_EQUAL : TK_BANG);
        case '<': return make_token(s, match(s, '=') ? TK_LESS_EQUAL : TK_LESS);
        case '>': return make_token(s, match(s, '=') ? TK_GREATER_EQUAL : TK_GREATER);
        case '"':
            scan_string_literal(s);
            return make_token(s, TK_STRING_LITERAL);
        default:
            break;
    }
    add_error(s, true, stringFormat("Unknown character '%c'!", c));
    return make_token(s, TK_GARBAGE);
}

static bool set_source(Scanner *s, FileID file) {
    File *f = compilerGetFile(s->compiler, file);
    VERIFY(f);
    String contents = fileRead(f);
    if(contents == NULL) {
        add_error(s, false, stringFormat("Failed to read file '%s'!", compilerGetFile(s->compiler, compilerGetCurrentFileID(s->compiler))->path));
        return false;
    }
    s->source = contents;
    s->start = s->current = 0;
    return true;
}

Token scannerNextToken(Scanner *s) {
    if(s->failed_to_set_source) {
        // if we failed to set the source, we can't do anything.
        // the error was already reported, so we will simplt return
        // TK_EOF so the parser thinks we are at the end of the input
        // and will exit so errors can be printed.
        return make_token(s, TK_EOF);
    }
    if(!s->source) {
        if(!set_source(s, compilerNextFile(s->compiler))) {
            s->failed_to_set_source = true;
            // if we can't set the source, assume we are at the end
            // of the input.
            return make_token(s, TK_EOF);
        }
    }
    Token tk = scan_token(s);
    if(tk.type == TK_EOF && compilerHasNextFile(s->compiler)) {
        set_source(s, compilerNextFile(s->compiler));
        File *newFile = compilerGetFile(s->compiler, compilerGetCurrentFileID(s->compiler));
        VERIFY(newFile);
        tk = tokenNew(TK_FILE_CHANGED, EMPTY_LOCATION, newFile->fileNameNoExtension, stringLength(newFile->fileNameNoExtension));
    }
    return tk;
}
