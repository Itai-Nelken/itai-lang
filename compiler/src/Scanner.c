#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Token.h"
#include "Scanner.h"

void initScanner(Scanner *s, const char *filename, char *source) {
    s->filename = strdup(filename);
    s->current = s->start = s->source = source;
    s->current_line = s->source;
    s->line = 1;
}

void freeScanner(Scanner *s) {
    free(s->filename);
    s->start = s->current = s->source = NULL;
    s->filename = NULL;
    s->line = 1;
}

static int calculate_line_length(Scanner *s) {
    char *p = s->current_line;
    for(; *p != '\0' && *p != '\n'; p++);
    return (int)(p - s->current_line);
}

static Token makeToken(Scanner *s, TokenType type) {
    Coordinate loc = {
        .file = s->filename,
        .containing_line = s->current_line,
        .line_length = calculate_line_length(s),
        .line = s->line,
        .at = s->current - s->start
    };
    return newToken(type, loc, s->start, s->current - s->start);
}

static Token errorToken(Scanner *s, const char *message) {
    Coordinate loc = {
        .file = s->filename,
        .line = s->line,
        .containing_line = s->current_line,
        .line_length = calculate_line_length(s),
        .at = s->current - s->start
    };
    return newToken(TK_ERROR, loc, (char *)message, strlen(message));
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isNumber(char c) {
    //    for decimal, hex, binary | for octal
    return (c >= '0' && c <= '9') || c == 'O';
}

static bool isAscii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isAlpha(char c) {
    return isAscii(c) || c == '_';
}

static bool isAtEnd(Scanner *s) {
    return *s->current == '\0';
}

static char advance(Scanner *s) {
    s->current++;
    return s->current[-1];
}

static char peek(Scanner *s) {
    return *s->current;
}

static char peekNext(Scanner *s) {
    if(isAtEnd(s)) {
        return '\0';
    }
    return s->current[1];
}

static bool match(Scanner *s, char expected) {
    if(isAtEnd(s) || peek(s) != expected) {
        return false;
    }
    advance(s);
    return true;
}

static Token scan_hex(Scanner *s) {
    // consume the 'x'
    advance(s);
    if(!isAscii(peek(s)) && !isDigit(peek(s))) {
        return errorToken(s, "Invalid hexadecimal number literal");
    }
    while(isDigit(peek(s)) || isAscii(peek(s))) {
        advance(s);
    }
    return makeToken(s, TK_NUMLIT);
}

static Token scan_binary(Scanner *s) {
    // consume the 'b'
    advance(s);
    if(peek(s) != '0' && peek(s) != '1') {
        return errorToken(s, "Invalid binary number literal");
    }
    while(peek(s) == '0' || peek(s) == '1' || peek(s) == '_') {
        advance(s);
    }
    return makeToken(s, TK_NUMLIT);
}

static void scan_octal(Scanner *s) {
    while((peek(s) >= '0' && peek(s) <= '7') || peek(s) == '_') {
        advance(s);
    }
}

static void scan_decimal(Scanner *s) {
    while(isDigit(peek(s)) || peek(s) == '_') {
        advance(s);
    }

    // look for fractional part.
    if(peek(s) == '.' && isDigit(peekNext(s))) {
        // consume the dot ('.').
        advance(s);
        while(isDigit(peek(s)) || peek(s) == '_') {
            advance(s);
        }
    }
}

static Token number(Scanner *s) {
    switch(peek(s)) {
        case 'x': return scan_hex(s); break;
        case 'b': return scan_binary(s); break;
        default:
            if(s->current[-1] == 'O') {
                scan_octal(s);
            } else {
                scan_decimal(s);
            }
            break;
    }
    return makeToken(s, TK_NUMLIT);
}

static Token string(Scanner *s) {
    while(peek(s) != '"' && !isAtEnd(s)) {
        if(peek(s) == '\n') s->line++;
        advance(s);
    }
    if(isAtEnd(s)) {
        return errorToken(s, "Unterminated string literal");
    }

    // the closing '"'
    advance(s);
    return makeToken(s, TK_STRLIT);
}

static TokenType checkKeyword(Scanner *s, int start, const char *rest, TokenType type) {
    int length = strlen(rest);
    if(s->current - s->start == start + length && memcmp(s->start + start, rest, length) == 0) {
        return type;
    }
    return TK_IDENTIFIER;
}

static TokenType identifierType(Scanner *s) {
    switch(s->start[0]) {
        case 'a': return ((s->current - s->start > 1) && s->start[1] == 's') ? TK_AS : TK_IDENTIFIER;
        case 'c':
            if(checkKeyword(s, 1, "onst", TK_CONST) == TK_IDENTIFIER) {
                return checkKeyword(s, 1, "har", TK_CHAR);
            }
            return TK_CONST;
        case 'e':
            if(s->current - s->start > 1) {
                switch(s->start[1]) {
                    case 'n': return checkKeyword(s, 2, "um", TK_ENUM);
                    case 'l': return checkKeyword(s, 2, "se", TK_ELSE);
                    case 'x': return checkKeyword(s, 2, "port", TK_EXPORT);
                    default:
                        break;
                }
            }
            break;
        case 'f':
            if(s->current - s->start > 1) {
                switch(s->start[1]) {
                    case 'n': return TK_FN;
                    case 'o': return checkKeyword(s, 2, "r", TK_FOR);
                    // types
                    case '3': return checkKeyword(s, 2, "2", TK_F32);
                    case '6': return checkKeyword(s, 2, "4", TK_F64);
                    default:
                        break;
                }
            }
            break;
        case 'i':
            if(s->current - s->start > 1) {
                switch(s->start[1]) {
                    case 'f': return TK_IF;
                    case 'm': return checkKeyword(s, 2, "port", TK_IMPORT);
                    // types
                    case 's': return checkKeyword(s, 2, "ize", TK_ISIZE);
                    case '8': return TK_I8;
                    case '1':
                        if(checkKeyword(s, 2, "28", TK_I128) == TK_IDENTIFIER) {
                            return TK_I16;
                        }
                        return TK_I128;
                    case '3': return checkKeyword(s, 2, "2", TK_I32);
                    case '6': return checkKeyword(s, 2, "4", TK_I64);
                    default:
                        break;
                }
            }
            break;
        case 'm': return checkKeyword(s, 1, "odule", TK_MODULE);
        case 'n': return checkKeyword(s, 1, "ull", TK_NULL);
        case 'r': return checkKeyword(s, 1, "eturn", TK_RETURN);
        case 's':
            if(s->current - s->start > 1) {
                switch(s->start[1]) {
                    case 't':
                        if(s->current - s->start > 2) {
                            switch(s->start[2]) {
                                case 'a': return checkKeyword(s, 3, "tic", TK_STATIC);
                                case 'r':
                                    if(checkKeyword(s, 3, "uct", TK_STRUCT) == TK_IDENTIFIER) {
                                        return TK_STR;
                                    }
                                    return TK_STRUCT;
                                default:
                                    break;
                            }
                        }
                        break;
                    case 'w': return checkKeyword(s, 2, "itch", TK_SWITCH);
                    default:
                        break;
                }
            }
            break;
        case 't':
            if(checkKeyword(s, 1, "ypeof", TK_TYPEOF) == TK_IDENTIFIER) {
                return checkKeyword(s, 1, "ype", TK_TYPE);
            }
            return TK_TYPEOF;
        case 'u':
            if(s->current - s->start > 1) {
                switch(s->start[1]) {
                    case 's':
                        if(checkKeyword(s, 2, "ing", TK_USING) == TK_IDENTIFIER) {
                            return TK_USIZE;
                        }
                        return TK_USING;
                    case '8': return TK_U8;
                    case '1':
                        if(checkKeyword(s, 2, "28", TK_U128) == TK_IDENTIFIER) {
                            return TK_U16;
                        }
                        return TK_U128;
                    case '3': return checkKeyword(s, 2, "2", TK_U32);
                    case '6': return checkKeyword(s, 2, "4", TK_U64);
                    default:
                        break;
                }
            }
            break;
        case 'v': return checkKeyword(s, 1, "ar", TK_VAR);
        case 'w': return checkKeyword(s, 1, "hile", TK_WHILE);
        default:
            break;
    }
    return TK_IDENTIFIER;
}

static Token identifier(Scanner *s) {
    while(isAlpha(peek(s)) || isDigit(peek(s))) {
        advance(s);
    }
    return makeToken(s, identifierType(s));
}

static void skipWhitespace(Scanner *s) {
    for(;;) {
        char c = peek(s);
        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance(s);
                break;
            case '\n':
                s->line++;
                advance(s);
                s->current_line = s->current;
                break;
            case '/':
                if(peekNext(s) == '/') {
                    // a comment goes until end of line
                    while(peek(s) != '\n' && !isAtEnd(s)) advance(s);
                } else if(peekNext(s) == '*') {
                    // multiline comment
                    int depth = 0; // to allow "nested" comments
                    while(!isAtEnd(s)) {
                        if(peek(s) == '\n') {
                            s->line++;
                            // was = 's->current_line+1', why???
                            s->current_line = s->current;
                        }
                        if(peek(s) == '/' && peekNext(s) == '*') {
                            depth++;
                        }
                        if(peek(s) == '*' && peekNext(s) == '/') {
                            if(depth > 1) {
                                depth--;
                            } else {
                                // skip the closing '*/'
                                advance(s); advance(s);
                                break;
                            }
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

Token nextToken(Scanner *s) {
    skipWhitespace(s);
    s->start = s->current;
    if(isAtEnd(s)) {
        return makeToken(s, TK_EOF);
    }

    char c = advance(s);
    // number literals
    if(isNumber(c)) {
        return number(s);
    }
    // keywords, types, and identifiers
    if(isAlpha(c)) {
        return identifier(s);
    }
    switch(c) {
        // one character tokens
        case '(': return makeToken(s, TK_LPAREN);
        case ')': return makeToken(s, TK_RPAREN);
        case '[': return makeToken(s, TK_LBRACKET);
        case ']': return makeToken(s, TK_RBRACKET);
        case '{': return makeToken(s, TK_LBRACE);
        case '}': return makeToken(s, TK_RBRACE);
        case ',': return makeToken(s, TK_COMMA);
        case ';': return makeToken(s, TK_SEMICOLON);
        case ':': return makeToken(s, TK_COLON);
        case '~': return makeToken(s, TK_TILDE);

        // one or two character tokens
        case '-':
            if(match(s, '=')) {
                return makeToken(s, TK_MINUS_EQUAL);
            } else if(match(s, '>')) {
                return makeToken(s, TK_ARROW);
            } else {
                return makeToken(s, TK_MINUS);
            }
        case '+':
            if(match(s, '=')) {
                return makeToken(s, TK_PLUS_EQUAL);
            } else {
                return makeToken(s, TK_PLUS);
            }
        case '/': return makeToken(s, match(s, '=') ? TK_SLASH_EQUAL : TK_SLASH);
        case '*': return makeToken(s, match(s, '=') ? TK_STAR_EQUAL : TK_STAR);
        case '!': return makeToken(s, match(s, '=') ? TK_BANG_EQUAL : TK_BANG);
        case '=': return makeToken(s, match(s, '=') ? TK_EQUAL_EQUAL : TK_EQUAL);
        case '%': return makeToken(s, match(s, '=') ? TK_PERCENT_EQUAL : TK_PERCENT);
        case '^': return makeToken(s, match(s, '=') ? TK_XOR_EQUAL : TK_XOR);
        case '|': return makeToken(s, match(s, '=') ? TK_PIPE_EQUAL : TK_PIPE);
        case '&': return makeToken(s, match(s, '=') ? TK_AMPERSAND_EQUAL : TK_AMPERSAND);

        // one, two, or three character tokens
        case '>':
            if(peek(s) == '>' && peekNext(s) == '=') {
                advance(s); advance(s);
                return makeToken(s, TK_RSHIFT_EQUAL);
            }
            if(match(s, '=')) {
                return makeToken(s, TK_GREATER_EQUAL);
            } else if(match(s, '>')) {
                return makeToken(s, TK_RSHIFT);
            }
            return makeToken(s, TK_GREATER);
        case '<':
            if(peek(s) == '<' && peekNext(s) == '=') {
                advance(s); advance(s);
                return makeToken(s, TK_LSHIFT_EQUAL);
            }
            if(match(s, '=')) {
                return makeToken(s, TK_LESS_EQUAL);
            } else if(match(s, '<')) {
                return makeToken(s, TK_LSHIFT);
            }
            return makeToken(s, TK_LESS);
        case '.':
            if(peek(s) == '.' && peekNext(s) == '.') {
                advance(s); advance(s);
		return makeToken(s, TK_ELIPSIS);
            }
            return makeToken(s, TK_DOT);

        // literals (other than number which is at the top)
        case '"': return string(s);
        case '\'':
            if(peekNext(s) != '\'') {
                return errorToken(s, "Unterminated character literal");
            }
            advance(s); advance(s);
            return makeToken(s, TK_CHARLIT);
        default:
            break;
    }
    return errorToken(s, "Unexpected character");
}
