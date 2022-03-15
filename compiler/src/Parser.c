#include <stdio.h>
#include "common.h"
#include "ast.h"
#include "Token.h"
#include "Scanner.h"
#include "Parser.h"

void initParser(Parser *p, const char *filename, char *source) {
    p->current_expr = NULL;
    p->had_error = false;
    p->panic_mode = false;
    initScanner(&p->scanner, filename, source);
}

void freeParser(Parser *p) {
    freeScanner(&p->scanner);
}

// print the location of a token in the following format:
// <line number> | <line>
static void printLocation(Token tok) {
    fprintf(stderr, "\t%d |  ", tok.location.line);
    fprintf(stderr, "%.*s\n", tok.location.line_length, tok.location.containing_line);
}

static void indentLine(Token tok) {
    int line = tok.location.line;
    short indent = 0;
    // FIXME: only works with up to 9999 lines of code
    if(line < 10) {
        indent = 1;
    } else if(line < 100) {
        indent = 2;
    } else if(line < 1000) {
        indent = 3;
    } else if(line < 10000) {
        indent = 3;
    } else {
        UNREACHABLE();
    }
    fprintf(stderr, "%*s", indent, "");
}

static void error(Parser *p, Token tok, const char *message) {
    // suppress any errors that may be caused by previous errors
    if(p->panic_mode) {
        return;
    }
    p->had_error = true;
    p->panic_mode = true;
    fprintf(stderr, "\x1b[1m%s:%d:%d: ", tok.location.file, tok.location.line, tok.location.at + 1);
    fprintf(stderr, "\x1b[31merror:\x1b[0m\n");
    printLocation(tok);
    fprintf(stderr, "\t");
    indentLine(tok);
    fprintf(stderr, " | %*s", tok.location.line_length - tok.location.at - tok.length, "");
    fprintf(stderr, "\x1b[1;35m^");
    // tok.length - 1 because the first character is used by the '^'
    for(int i = 0; i < tok.length - 1; ++i) {
        fputc('~', stderr);
    }
    fprintf(stderr, " \x1b[0;1m%s\x1b[0m\n", message);
}

static void warning(Parser *p, Token tok, const char *message) {
    fprintf(stderr, "\x1b[1m%s:%d:%d: ", tok.location.file, tok.location.line, tok.location.at + 1);
    fprintf(stderr, "\x1b[35mwarning:\x1b[0m\n");
    printLocation(tok);
    fprintf(stderr, "\t");
    indentLine(tok);
    fprintf(stderr, " | %*s", tok.location.line_length - tok.location.at - tok.length, "");
    fprintf(stderr, "\x1b[1;35m^");
    // tok.length - 1 because the first character is used by the '^'
    for(int i = 0; i < tok.length - 1; ++i) {
        fputc('~', stderr);
    }
    fprintf(stderr, " \x1b[0;1m%s\x1b[0m\n", message);
}

// FIXME: assumes tok.lexeme is a nul terminated string
static void errorToken(Parser *p, Token tok) {
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
    PREC_NONE       = 0,
    PREC_EQUALITY   = 1, // infix == !=
    PREC_COMPARISON = 2, // infix > >= < <=
    PREC_TERM       = 3, // infix + - (lowest)
    PREC_FACTOR     = 4, // infix * /
    PREC_UNARY      = 5,
    PREC_PRIMARY    = 6  // highest
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
static void parse_unary(Parser *p);
static void parsePrecedence(Parser *p, Precedence prec);
static ASTNode *expression(Parser *p);

static ParseRule rules[TK__COUNT] = {
    [TK_LPAREN]          = {parse_grouping, NULL, PREC_NONE},
    [TK_RPAREN]          = {NULL, NULL, PREC_NONE},
    [TK_LBRACKET]        = {NULL, NULL, PREC_NONE},
    [TK_RBRACKET]        = {NULL, NULL, PREC_NONE},
    [TK_LBRACE]          = {NULL, NULL, PREC_NONE},
    [TK_RBRACE]          = {NULL, NULL, PREC_NONE},
    [TK_COMMA]           = {NULL, NULL, PREC_NONE},
    [TK_SEMICOLON]       = {NULL, NULL, PREC_NONE},
    [TK_COLON]           = {NULL, NULL, PREC_NONE},
    [TK_TILDE]           = {NULL, NULL, PREC_NONE},
    [TK_MINUS]           = {parse_unary, parse_binary, PREC_TERM},
    [TK_MINUS_EQUAL]     = {NULL, NULL, PREC_NONE},
    [TK_PLUS]            = {parse_unary, parse_binary, PREC_TERM},
    [TK_PLUS_EQUAL]      = {NULL, NULL, PREC_NONE},
    [TK_SLASH]           = {NULL, parse_binary, PREC_FACTOR},
    [TK_SLASH_EQUAL]     = {NULL, NULL, PREC_NONE},
    [TK_STAR]            = {NULL, parse_binary, PREC_FACTOR},
    [TK_STAR_EQUAL]      = {NULL, NULL, PREC_NONE},
    [TK_BANG]            = {NULL, NULL, PREC_NONE},
    [TK_BANG_EQUAL]      = {NULL, parse_binary, PREC_EQUALITY},
    [TK_EQUAL]           = {NULL, NULL, PREC_NONE},
    [TK_EQUAL_EQUAL]     = {NULL, parse_binary, PREC_EQUALITY},
    [TK_PERCENT]         = {NULL, NULL, PREC_NONE},
    [TK_PERCENT_EQUAL]   = {NULL, NULL, PREC_NONE},
    [TK_XOR]             = {NULL, NULL, PREC_NONE},
    [TK_XOR_EQUAL]       = {NULL, NULL, PREC_NONE},
    [TK_PIPE]            = {NULL, NULL, PREC_NONE},
    [TK_PIPE_EQUAL]      = {NULL, NULL, PREC_NONE},
    [TK_AMPERSAND]       = {NULL, NULL, PREC_NONE},
    [TK_AMPERSAND_EQUAL] = {NULL, NULL, PREC_NONE},
    [TK_GREATER]         = {NULL, parse_binary, PREC_COMPARISON},
    [TK_GREATER_EQUAL]   = {NULL, parse_binary, PREC_COMPARISON},
    [TK_RSHIFT]          = {NULL, NULL, PREC_NONE},
    [TK_RSHIFT_EQUAL]    = {NULL, NULL, PREC_NONE},
    [TK_LESS]            = {NULL, parse_binary, PREC_COMPARISON},
    [TK_LESS_EQUAL]      = {NULL, parse_binary, PREC_COMPARISON},
    [TK_LSHIFT]          = {NULL, NULL, PREC_NONE},
    [TK_LSHIFT_EQUAL]    = {NULL, NULL, PREC_NONE},
    [TK_DOT]             = {NULL, NULL, PREC_NONE},
    [TK_ELIPSIS]         = {NULL, NULL, PREC_NONE},
    [TK_STRLIT]          = {NULL, NULL, PREC_NONE},
    [TK_CHARLIT]         = {NULL, NULL, PREC_NONE},
    [TK_NUMLIT]          = {parse_number, NULL, PREC_NONE},
    [TK_IDENTIFIER]      = {NULL, NULL, PREC_NONE},
    [TK_I8]              = {NULL, NULL, PREC_NONE},
    [TK_I16]             = {NULL, NULL, PREC_NONE},
    [TK_I32]             = {NULL, NULL, PREC_NONE},
    [TK_I64]             = {NULL, NULL, PREC_NONE},
    [TK_I128]            = {NULL, NULL, PREC_NONE},
    [TK_U8]              = {NULL, NULL, PREC_NONE},
    [TK_U16]             = {NULL, NULL, PREC_NONE},
    [TK_U32]             = {NULL, NULL, PREC_NONE},
    [TK_U64]             = {NULL, NULL, PREC_NONE},
    [TK_U128]            = {NULL, NULL, PREC_NONE},
    [TK_F32]             = {NULL, NULL, PREC_NONE},
    [TK_F64]             = {NULL, NULL, PREC_NONE},
    [TK_ISIZE]           = {NULL, NULL, PREC_NONE},
    [TK_USIZE]           = {NULL, NULL, PREC_NONE},
    [TK_CHAR]            = {NULL, NULL, PREC_NONE},
    [TK_STR]             = {NULL, NULL, PREC_NONE},
    [TK_VAR]             = {NULL, NULL, PREC_NONE},
    [TK_CONST]           = {NULL, NULL, PREC_NONE},
    [TK_STATIC]          = {NULL, NULL, PREC_NONE},
    [TK_FN]              = {NULL, NULL, PREC_NONE},
    [TK_RETURN]          = {NULL, NULL, PREC_NONE},
    [TK_ENUM]            = {NULL, NULL, PREC_NONE},
    [TK_STRUCT]          = {NULL, NULL, PREC_NONE},
    [TK_IF]              = {NULL, NULL, PREC_NONE},
    [TK_ELSE]            = {NULL, NULL, PREC_NONE},
    [TK_SWITCH]          = {NULL, NULL, PREC_NONE},
    [TK_MODULE]          = {NULL, NULL, PREC_NONE},
    [TK_EXPORT]          = {NULL, NULL, PREC_NONE},
    [TK_IMPORT]          = {NULL, NULL, PREC_NONE},
    [TK_AS]              = {NULL, NULL, PREC_NONE},
    [TK_USING]           = {NULL, NULL, PREC_NONE},
    [TK_WHILE]           = {NULL, NULL, PREC_NONE},
    [TK_FOR]             = {NULL, NULL, PREC_NONE},
    [TK_TYPE]            = {NULL, NULL, PREC_NONE},
    [TK_NULL]            = {NULL, NULL, PREC_NONE},
    [TK_TYPEOF]          = {NULL, NULL, PREC_NONE},
    [TK_ERROR]           = {NULL, NULL, PREC_NONE},
    [TK_EOF]             = {NULL, NULL, PREC_NONE}
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

static void parse_unary(Parser *p) {
    TokenType operatorType = previous(p).type;
    parsePrecedence(p, PREC_UNARY);
    switch(operatorType) {
        case TK_MINUS:
            p->current_expr = newUnaryNode(ND_NEG, p->current_expr);
            break;
        case TK_PLUS:
            // nothing, leave the operand
            break;
        default:
            UNREACHABLE();
    }
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
                warning(p, peek(p), "division by 0");
                return;
            }
            p->current_expr = newNode(ND_DIV, left, p->current_expr);
            break;
        case TK_EQUAL_EQUAL:
            p->current_expr = newNode(ND_EQ, left, p->current_expr);
            break;
        case TK_BANG_EQUAL:
            p->current_expr = newNode(ND_NE, left, p->current_expr);
            break;
        case TK_GREATER:
            p->current_expr = newNode(ND_GT, left, p->current_expr);
            break;
        case TK_GREATER_EQUAL:
            p->current_expr = newNode(ND_GE, left, p->current_expr);
            break;
        case TK_LESS:
            p->current_expr = newNode(ND_LT, left, p->current_expr);
            break;
        case TK_LESS_EQUAL:
            p->current_expr = newNode(ND_LE, left, p->current_expr);
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
    // start parsing with the lowest precedence
    parsePrecedence(p, PREC_EQUALITY);
    return p->current_expr;
}

ASTProg parse(Parser *p) {
    advance(p);
    ASTNode *expr = expression(p);
    consume(p, TK_EOF, "expected end of input");
    if(p->had_error) {
        freeAST(expr);
        expr = NULL;
    }
    ASTProg prog = {
        .expr = expr
    };
    return prog;
}
