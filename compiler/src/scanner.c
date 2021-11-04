#include <stdlib.h>
#include <string.h>
#include "tokenType.h"
#include "token.h"
#include "scanner.h"

Scanner newScanner(const char *source) {
    Scanner scanner;
    scannerInit(&scanner, source);

    return scanner;
}

void scannerInit(Scanner *s, const char *source) {
    s->line=1;
    s->_source=malloc(strlen(source)+1);
    strcpy(s->_source, source);
    s->_source[strlen(source)]='\0';
    s->start=s->current=s->_source;
    s->_inited=true;
}

void scannerDestroy(Scanner *s) {
    free(s->_source);
    s->start=s->current=NULL;
    s->_inited=false;
}

/******
 * check if at end of source string.
 * 
 * @param s An initialized scanner.
 * 
 * @return true if at end or false if not.
 ******/
static bool isAtEnd(Scanner *s) {
    return *(s->current) == '\0';
}

/******
 * Create a new token.
 * 
 * @param s An initialized scanner.
 * @param type the token type.
 * 
 * @return The new token.
 ******/
static Token makeToken(Scanner *s, TokenType type) {
    Token t;
    t.type=type;
    t.start=s->start;
    t.length=(int)(s->current - s->start);
    t.line=s->line;
    return t;
}

/******
 * Create an error token. call ONLY with a string literal as 'message' or expect bugs.
 * 
 * @param s An initialized scanner.
 * @param message A string literal (ONLY) with the error message.
 * 
 * @return The new error Token
 ******/
static Token errorToken(Scanner *s, const char *message) {
    Token t;
    t.type=TOKEN_ERROR;
    // this is ok as long as we call this function ONLY with a string literal as parameter 'message'
    t.start=message;
    t.length=(int)strlen(message);
    t.line=s->line;
    return t;
}

static inline char advance(Scanner *s) {
    return *(s->current++);
}

static inline char peek(Scanner *s) {
    return *s->current;
}

static char peekNext(Scanner *s) {
    if(isAtEnd(s)) return '\0';
    return s->current[1]; // same as s->current+1
}

static bool match(Scanner *s, char expected) {
    if(isAtEnd(s)) return false;
    // if current character doesn't match
    if(*(s->current) != expected) return false;
    // else advance
    s->current++;
    return true;
}


static inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool isAscii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// return true if 'c' is a valid character for an identifier
static bool isAlpha(char c) {
    return isAscii(c) || c == '_';
}

// remove any newlines, spaces, carriage returns, tabs, and comments before the next character
// that isn't any of them.
static void skipWhiteSpace(Scanner *s) {
    char c;
    for(;;) {
        c=peek(s);
        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                // "eat" spaces, carriage returns and tabs
                advance(s);
                break;
            case '\n':
                // increment line and "eat" newlines
                s->line++;
                advance(s);
                break;
            case '/':
                if(peekNext(s) == '/') {
                    // a comment goes until end of line
                    while(peek(s)!='\n' && !isAtEnd(s)) advance(s);
                }
            default:
                return;
        }
    }
}

static Token number(Scanner *s) {
    while(isDigit(peek(s))) advance(s);

    /* == for fractional numbers ==
    if(peek(s)=='.' && isDigit(peekNext(s))) {
        // consume the '.'
        advance(s);
        while(isDigit(peek(s))) advance(s);
    }
    */

    return makeToken(s, TOKEN_NUMBER);
}

static Token identifier(Scanner *s) {
    while(isAlpha(peek(s)) || isDigit(peek(s))) advance(s);
    return makeToken(s, TOKEN_IDENTIFIER);
}

static Token character(Scanner *s) {
    bool isClosed=false;
    // no need to consume the opening quote as it's already consumed
    if(peekNext(s) == '\'') isClosed=true;
    if(isAscii(peek(s))) {
        // consume the character itself
        advance(s);
        // and the closing quote
        advance(s);
        return makeToken(s, TOKEN_CHARACTER);
    }
    return errorToken(s, (isClosed ? "unsupported character!" : "Unterminated character!"));
}

Token scanToken(Scanner *s) {
    skipWhiteSpace(s);
    s->start=s->current;
    if(isAtEnd(s)) return makeToken(s, TOKEN_EOF);

    char c=advance(s);
    if(isAlpha(c)) return identifier(s);
    if(isDigit(c)) return number(s);
    switch(c) {
        case '(': return makeToken(s, TOKEN_PAREN_LEFT);
        case ')': return makeToken(s, TOKEN_PAREN_RIGHT);
        case ';': return makeToken(s, TOKEN_SEMICOLON);
        case '+': return makeToken(s, TOKEN_PLUS);
        case '-': return makeToken(s, TOKEN_MINUS);
        case '*': return makeToken(s, TOKEN_STAR);
        case '/': return makeToken(s, TOKEN_SLASH);
        case '!': return makeToken(s, (match(s, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG));
        case '=': return makeToken(s, (match(s, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL));
        case '<': return makeToken(s, (match(s, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS));
        case '>': return makeToken(s, (match(s, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER));
        case '\'': return character(s);
    }

    return errorToken(s, "Unexpected character!");
}
