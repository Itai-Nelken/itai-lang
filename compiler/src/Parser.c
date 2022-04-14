#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "utilities.h"
#include "Strings.h"
#include "Array.h"
#include "ast.h"
#include "Token.h"
#include "Errors.h"
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

static void error(Parser *p, Token tok, const char *message) {
    // suppress any errors that may be caused by previous errors
    if(p->panic_mode) {
        return;
    }
    p->had_error = true;
    p->panic_mode = true;

    printError(ERR_ERROR, tok.location, message);
}

static inline void warning(Token tok, const char *message) {
    printError(ERR_WARNING, tok.location, message);
}

static inline void errorToken(Parser *p, Token tok) {
    error(p, tok, tok.errmsg);
    // because errors from the scanner won't cause
    // cascading errors.
    p->panic_mode = false;
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
    PREC_ASSIGNMENT = 1, // infix =
    PREC_BIT_OR     = 1,  // infix |
    PREC_BIT_XOR    = 2,  // infix ^
    PREC_BIT_AND    = 3,  // infix &
    PREC_EQUALITY   = 4, // infix == !=
    PREC_COMPARISON = 5, // infix > >= < <=
    PREC_BIT_SHIFT  = 6, // infix << >>
    PREC_TERM       = 7, // infix + -
    PREC_FACTOR     = 8, // infix * /
    PREC_UNARY      = 9,
    PREC_PRIMARY    = 10  // highest
} Precedence;

typedef void (*InfixParseFn)(Parser *p);
typedef void (*PrefixParseFn)(Parser *p, bool canAssign);

typedef struct parse_rule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;

// prototypes
static void parse_number(Parser *p, bool canAssign);
static void parse_identifier(Parser *p, bool canAssign);
static void parse_grouping(Parser *p, bool canAssign);
static void parse_unary(Parser *p, bool canAssign);
static void parse_binary(Parser *p);
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
    [TK_PERCENT]         = {NULL, parse_binary, PREC_FACTOR},
    [TK_PERCENT_EQUAL]   = {NULL, NULL, PREC_NONE},
    [TK_XOR]             = {NULL, parse_binary, PREC_BIT_XOR},
    [TK_XOR_EQUAL]       = {NULL, NULL, PREC_NONE},
    [TK_PIPE]            = {NULL, parse_binary, PREC_BIT_OR},
    [TK_PIPE_EQUAL]      = {NULL, NULL, PREC_NONE},
    [TK_AMPERSAND]       = {NULL, parse_binary, PREC_BIT_AND},
    [TK_AMPERSAND_EQUAL] = {NULL, NULL, PREC_NONE},
    [TK_GREATER]         = {NULL, parse_binary, PREC_COMPARISON},
    [TK_GREATER_EQUAL]   = {NULL, parse_binary, PREC_COMPARISON},
    [TK_RSHIFT]          = {NULL, parse_binary, PREC_BIT_SHIFT},
    [TK_RSHIFT_EQUAL]    = {NULL, NULL, PREC_NONE},
    [TK_LESS]            = {NULL, parse_binary, PREC_COMPARISON},
    [TK_LESS_EQUAL]      = {NULL, parse_binary, PREC_COMPARISON},
    [TK_LSHIFT]          = {NULL, parse_binary, PREC_BIT_SHIFT},
    [TK_LSHIFT_EQUAL]    = {NULL, NULL, PREC_NONE},
    [TK_DOT]             = {NULL, NULL, PREC_NONE},
    [TK_ELIPSIS]         = {NULL, NULL, PREC_NONE},
    [TK_STRLIT]          = {NULL, NULL, PREC_NONE},
    [TK_CHARLIT]         = {NULL, NULL, PREC_NONE},
    [TK_NUMLIT]          = {parse_number, NULL, PREC_NONE},
    [TK_FLOATLIT]        = {NULL, NULL, PREC_NONE},
    [TK_IDENTIFIER]      = {parse_identifier, NULL, PREC_NONE},
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

static void parse_number(Parser *p, bool canAssign) {
    UNUSED(canAssign);
    Token tok = previous(p);
    int base = 10;
    char *lexeme = tok.lexeme;

    if(tok.length < 2) {
        // only base 10 literals can be 1 character
        goto end;
    }
    if(tok.lexeme[0] == '0') {
        switch(tok.lexeme[1]) {
            // ignore the 0x, 0b, and 0o prefixes as strtol doesn't understand them
            // (it does understand 0x)
            case 'x': base = 16; lexeme = lexeme + 2; break;
            case 'b': base = 2; lexeme = lexeme + 2; break;
            case 'o': base = 8; lexeme = lexeme + 2; break;
            default:
                // only option left is base 10
                break;
        }
    }

end:
    p->current_expr = newNumberNode((int)strtol(lexeme, NULL, base), previous(p).location);
}

static void parse_identifier(Parser *p, bool canAssign) {
    p->current_expr = newNode(ND_VAR, NULL, NULL, previous(p).location);
    p->current_expr->as.var.name = stringNCopy(previous(p).lexeme, previous(p).length);
    if(canAssign && peek(p).type == TK_EQUAL) {
        advance(p);
        p->current_expr = newNode(ND_ASSIGN, p->current_expr, expression(p), previous(p).location);
    }
}

static void parse_grouping(Parser *p, bool canAssign) {
    UNUSED(canAssign);
    p->current_expr = expression(p);
    consume(p, TK_RPAREN, "expected ')' after expression");
}

static void parse_unary(Parser *p, bool canAssign) {
    UNUSED(canAssign);
    TokenType operatorType = previous(p).type;
    parsePrecedence(p, PREC_UNARY);
    switch(operatorType) {
        case TK_MINUS:
            p->current_expr = newUnaryNode(ND_NEG, p->current_expr, previous(p).location);
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
    p->current_expr = NULL;
    parsePrecedence(p, rule->precedence + 1);
    switch(operatorType) {
        case TK_PIPE:
            p->current_expr = newNode(ND_BIT_OR, left, p->current_expr, previous(p).location);
            break;
        case TK_XOR:
            p->current_expr = newNode(ND_XOR, left, p->current_expr, previous(p).location);
            break;
        case TK_AMPERSAND:
            p->current_expr = newNode(ND_BIT_AND, left, p->current_expr, previous(p).location);
            break;
        case TK_RSHIFT:
            p->current_expr = newNode(ND_BIT_RSHIFT, left, p->current_expr, previous(p).location);
            break;
        case TK_LSHIFT:
            p->current_expr = newNode(ND_BIT_LSHIFT, left, p->current_expr, previous(p).location);
            break;
        case TK_EQUAL_EQUAL:
            p->current_expr = newNode(ND_EQ, left, p->current_expr, previous(p).location);
            break;
        case TK_BANG_EQUAL:
            p->current_expr = newNode(ND_NE, left, p->current_expr, previous(p).location);
            break;
        case TK_GREATER:
            p->current_expr = newNode(ND_GT, left, p->current_expr, previous(p).location);
            break;
        case TK_GREATER_EQUAL:
            p->current_expr = newNode(ND_GE, left, p->current_expr, previous(p).location);
            break;
        case TK_LESS:
            p->current_expr = newNode(ND_LT, left, p->current_expr, previous(p).location);
            break;
        case TK_LESS_EQUAL:
            p->current_expr = newNode(ND_LE, left, p->current_expr, previous(p).location);
            break;
        case TK_PLUS:
            p->current_expr = newNode(ND_ADD, left, p->current_expr, previous(p).location);
            break;
        case TK_MINUS:
            p->current_expr = newNode(ND_SUB, left, p->current_expr, previous(p).location);
            break;
        case TK_STAR:
            p->current_expr = newNode(ND_MUL, left, p->current_expr, previous(p).location);
            break;
        case TK_SLASH:
            if(p->current_expr->type == ND_NUM && p->current_expr->as.literal.int32 == 0) {
                warning(previous(p), "division by 0");
            }
            p->current_expr = newNode(ND_DIV, left, p->current_expr, previous(p).location);
            break;
        case TK_PERCENT:
            if(p->current_expr->type == ND_NUM && p->current_expr->as.literal.int32 == 0) {
                warning(previous(p), "causes division by 0");
            }
            p->current_expr = newNode(ND_REM, left, p->current_expr, previous(p).location);
            break;
        default:
            UNREACHABLE();
    }
}

static void parsePrecedence(Parser *p, Precedence prec) {
    advance(p);
    PrefixParseFn prefix = getRule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, previous(p), "expected an expression");
        return;
    }
    bool canAssign = prec <= PREC_ASSIGNMENT;
    prefix(p, canAssign);
    while(prec <= getRule(peek(p).type)->precedence) {
        advance(p);
        InfixParseFn infix = getRule(previous(p).type)->infix;
        infix(p);
    }

    if(canAssign && peek(p).type == TK_EQUAL) {
        // invalid assignment target
        error(p, peek(p), "lvalue required as left operand of assignment");
        advance(p);
    }
}

static ASTNode *expression(Parser *p) {
    // start parsing with the lowest precedence
    parsePrecedence(p, PREC_ASSIGNMENT);
    return p->current_expr;
}

static void synchronize(Parser *p) {
    // synchronize to statement boundaries
    while(peek(p).type != TK_EOF) {
        if(previous(p).type == TK_SEMICOLON) {
            return;
        }
        // FIXME: add all statements (and declarations)
        switch(peek(p).type) {
            case TK_FN:
            case TK_VAR:
            case TK_FOR:
            case TK_WHILE:
            case TK_IF:
            case TK_RETURN:
                return;
            default:
                break;
        }

        advance(p);
    }
}

static ASTNode *expr_stmt(Parser *p) {
    p->current_expr = newUnaryNode(ND_EXPR_STMT, expression(p), previous(p).location);
    consume(p, TK_SEMICOLON, "Expected ';' after expression");
    return p->current_expr;
}

static ASTNode *var_decl(Parser *p) {
    // 'var' is already consumed
    consume(p, TK_IDENTIFIER, "Expected an identifier after 'var'");
    char *name = stringNCopy(previous(p).lexeme, previous(p).length);
    
    // TODO: check for types
    
    ASTNode *var = newNode(ND_VAR, NULL, NULL, previous(p).location);
    var->as.var.name = name;

    if(peek(p).type == TK_EQUAL) {
        advance(p);
        var = newNode(ND_ASSIGN, var, expression(p), previous(p).location);
    }

    consume(p, TK_SEMICOLON, "Expected ';' after variable declaration");
    return var;
}

// expr_stmt -> expression ';'
// var_decl -> 'var' IDENTIFIER (':' TYPE)? ('=' expression)? ';'
// print_stmt -> 'print' expression ';'
// return_stmt -> 'return' expression ';'
// statement -> print_stmt | return_stmt | expr_stmt
// declaration -> var_decl | statement
static ASTNode *statement(Parser *p) {
    ASTNode *n = NULL;
    if(peek(p).type == TK_PRINT) {
        advance(p);
        n = newUnaryNode(ND_PRINT, expression(p), previous(p).location);
        consume(p, TK_SEMICOLON, "Expected ';' after 'print' statement");
    } else if(peek(p).type == TK_RETURN) {
        advance(p);
        n = newUnaryNode(ND_RETURN, expression(p), previous(p).location);
        consume(p, TK_SEMICOLON, "Expected ';' after 'return' statement");
    } else {
        n = expr_stmt(p);
    }
    return n;
}

static ASTNode *declaration(Parser *p) {
    ASTNode *n = NULL;
    if(peek(p).type == TK_VAR) {
        advance(p);
        n = var_decl(p);
    } else {
        n = statement(p);
    }
    return n;
}

bool parse(Parser *p, ASTProg *prog) {
    advance(p);
    while(peek(p).type != TK_EOF) {
        ASTNode *node = declaration(p);

        // reset the parser for the next statement
        if(p->panic_mode) {
            p->panic_mode = false;
            synchronize(p);
        }
        p->current_expr = NULL;
        if(p->had_error) {
            freeAST(node);
            continue;
        }
        arrayPush(&prog->statements, node);
    }
    return p->had_error ? false : true;
}
