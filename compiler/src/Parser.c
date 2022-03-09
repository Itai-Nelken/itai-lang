#include <stdio.h>
#include "common.h"
#include "ast.h"
#include "Token.h"
#include "Scanner.h"
#include "Parser.h"

void initParser(Parser *p, const char *filename, char *source) {
    p->current_expr = NULL;
    p->has_error = false;
    initScanner(&p->scanner, filename, source);
}

void freeParser(Parser *p) {
    freeScanner(&p->scanner);
}

// TODO: print line containing offending charater(s) with a '^~~' pointing at the character(s)
static void error(Parser *p, Token tok, const char *message) {
    p->has_error = true;
    fprintf(stderr, "\x1b[1m%s:%d:%d: ", tok.location.file, tok.location.line, tok.location.at);
    fprintf(stderr, "\x1b[31merror:\x1b[0m %s\n", message);
}

static void errorToken(Parser *p, Token tok) {
    // FIXME: this assumes tok.lexeme is a nul terminated string
    error(p, tok, tok.lexeme);
}

static Token advance(Parser *p) {
    Token tok;
    while((tok = nextToken(&p->scanner)).type == TK_ERROR) {
        errorToken(p, tok);
    }
    p->previous_token = p->current_token;
    p->current_token = tok;
    return tok;
}

static Token peek(Parser *p) {
    return p->current_token;
}

static Token previous(Parser *p) {
    return p->previous_token;
}

static void consume(Parser *p, TokenType expected, const char *message) {
    if(peek(p).type != expected) {
        error(p, peek(p), message);
        return;
    }
    advance(p);
}

typedef enum precedence {
    PREC_NONE    = 0,
    PREC_TERM    = 1, // infix + - (lowest)
    PREC_FACTOR  = 2, // infix * /
    PREC_PRIMARY = 3  // highest
} Precedence;

typedef void (*ParseFn)(Parser *p);

typedef struct parse_rule {
    ParseFn prefix, infix;
    Precedence precedence;
} ParseRule;

// prototypes
static void parse_number(Parser *p);
static void parse_binary(Parser *p);
static void parse_grouping(Parser *p);
static void parsePrecedence(Parser *p, Precedence prec);
static ASTNode *expression(Parser *p);

static ParseRule rules[TK__COUNT] = {
    [TK_LPAREN] = {parse_grouping, NULL, PREC_NONE},
    [TK_RPAREN] = {NULL, NULL, PREC_NONE},
    [TK_MINUS] = {NULL, parse_binary, PREC_TERM},
    [TK_PLUS] = {NULL, parse_binary, PREC_TERM},
    [TK_SLASH] = {NULL, parse_binary, PREC_FACTOR},
    [TK_STAR] = {NULL, parse_binary, PREC_FACTOR},
    [TK_NUMLIT] = {parse_number, NULL, PREC_NONE},
    [TK_EOF] = {NULL, NULL, PREC_NONE}
};

static ParseRule *getRule(TokenType type) {
    return &rules[type];
}

// FIXME: duplicate with scanner one
static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// FIXME: doesn't check if the literal is empty
static void parse_number(Parser *p) {
    Token tok = previous(p);
    if((tok.lexeme[1] != 'b' || tok.lexeme[1] != 'x') && isDigit(tok.lexeme[0])) { // decimal
        p->current_expr = newNumberNode((int)strtol(tok.lexeme, NULL, 10));
        return;
    } else if(tok.lexeme[0] == 'O') { // octal
        p->current_expr = newNumberNode((int)strtol(tok.lexeme, NULL, 8));
        return;
    }
    switch(tok.lexeme[1]) {
        case 'x':
            p->current_expr = newNumberNode((int)strtol(tok.lexeme, NULL, 16));
            break;
        case 'b':
            p->current_expr = newNumberNode((int)strtol(tok.lexeme, NULL, 2));
            break;
        default:
            UNREACHABLE();
    }
}

static void parse_grouping(Parser *p) {
    p->current_expr = expression(p);
    consume(p, TK_RPAREN, "expected \x1b[1m')'\x1b[0m after expression");
}

static void parse_binary(Parser *p) {
    TokenType operatorType = previous(p).type;
    ParseRule *rule = getRule(operatorType);

    ASTNode *left = p->current_expr;
    parsePrecedence(p, rule->precedence + 1);
    switch(operatorType) {
        case TK_PLUS:
            p->current_expr = newNode(ND_ADD, left, p->current_expr);
            break;
        case TK_MINUS:
            p->current_expr = newNode(ND_SUB, left, p->current_expr);
            break;
        case TK_STAR:
            p->current_expr = newNode(ND_MUL, left, p->current_expr);
            break;
        case TK_SLASH:
            if(p->current_expr->literal.int32 == 0) {
                error(p, previous(p), "division by 0");
                return;
            }
            p->current_expr = newNode(ND_DIV, left, p->current_expr);
            break;
        default:
            UNREACHABLE();
    }
}

static void parsePrecedence(Parser *p, Precedence prec) {
    advance(p);
    ParseFn prefix = getRule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, previous(p), "expected an expression");
        return;
    }
    prefix(p);
    while(prec <= getRule(peek(p).type)->precedence) {
        advance(p);
        ParseFn infix = getRule(previous(p).type)->infix;
        infix(p);
    }
}

static ASTNode *expression(Parser *p) {
    // the lowest precedence
    parsePrecedence(p, PREC_TERM);
    return p->current_expr;
}

ASTNode *parse(Parser *p) {
    advance(p);
    ASTNode *expr = expression(p);
    consume(p, TK_EOF, "expected end of input");
    if(p->has_error) {
        freeAST(expr);
        expr = NULL;
    }
    return expr;
}
