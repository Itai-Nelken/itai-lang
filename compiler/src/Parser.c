#include <assert.h>
#include <stdbool.h>
#include "common.h"
#include "Errors.h"
#include "Array.h"
#include "Strings.h"
#include "Symbols.h"
#include "Token.h"
#include "Scanner.h"
#include "ast.h"
#include "Parser.h"

void initParser(Parser *p, Scanner *s, ASTProg *prog) {
    assert(p && s && prog);
    p->prog = prog;
    p->scanner = s;
    p->had_error = false;
    p->panic_mode = false;
}

void freeParser(Parser *p) {
    p->prog = NULL;
    p->scanner = NULL;
    p->had_error = false;
    p->panic_mode = false;
}

/*** utilities ***/
static void error(Parser *p, Token tok, const char *format, ...) {
    // suppress any errors that may be caused by previous errors
    if(p->panic_mode) {
        return;
    }
    p->had_error = true;
    p->panic_mode = true;

    va_list ap;
    va_start(ap, format);
    vprintErrorF(ERR_ERROR, tok.location, format, ap);
    va_end(ap);
}

static Token advance(Parser *p) {
    Token tok;
    while((tok = nextToken(p->scanner)).type == TK_ERROR) {
        error(p, tok, tok.errmsg);
    }
    p->previous_token = p->current_token;
    p->current_token = tok;
    return tok;
}

static inline Token peek(Parser *p) {
    return p->current_token;
}

static inline Token previous(Parser *p) {
    return p->previous_token;
}

static bool consume(Parser *p, TokenType expected, const char *message) {
    if(peek(p).type != expected) {
        error(p, peek(p), message);
        return false;
    }
    advance(p);
    return true;
}

static bool match(Parser *p, TokenType type) {
    if(peek(p).type == type) {
        advance(p);
        return true;
    }
    return false;
}

/*** expression parser ***/
typedef enum precedences {
    PREC_NONE       = 0,
    PREC_ASSIGNMENT = 1,  // infix =
    PREC_BIT_OR     = 2,  // infix |
    PREC_BIT_XOR    = 3,  // infix ^
    PREC_BIT_AND    = 4,  // infix &
    PREC_EQUALITY   = 5,  // infix == !=
    PREC_COMPARISON = 6,  // infix > >= < <=
    PREC_BIT_SHIFT  = 7,  // infix << >>
    PREC_TERM       = 8,  // infix + -
    PREC_FACTOR     = 9,  // infix * /
    PREC_UNARY      = 10, // unary + -
    PREC_CALL       = 11, // ()
    PREC_PRIMARY    = 12  // highest
} Precedence;

typedef ASTNode *(*PrefixParseFn)(Parser *p);
typedef ASTNode *(*InfixParseFn)(Parser *p, ASTNode *left);

typedef struct parse_rule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;

static ASTNode *parse_identifier(Parser *p);
static ASTNode *parse_number(Parser *p);
static ASTNode *parse_grouping(Parser *p);
static ASTNode *parse_unary(Parser *p);
static ASTNode *parse_term(Parser *p, ASTNode *left);
static ASTNode *parse_factor(Parser *p, ASTNode *left);
static ASTNode *parse_equality(Parser *p, ASTNode *left);
static ASTNode *parse_comparison(Parser *p, ASTNode *left);
static ASTNode *parse_bitwise(Parser *p, ASTNode *left);
static ASTNode *parse_assignment(Parser *p, ASTNode *lvalue);

static const ParseRule rules[TK__COUNT] = {
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
    [TK_MINUS]           = {parse_unary, parse_term, PREC_TERM},
    [TK_MINUS_EQUAL]     = {NULL, NULL, PREC_NONE},
    [TK_PLUS]            = {parse_unary, parse_term, PREC_TERM},
    [TK_PLUS_EQUAL]      = {NULL, NULL, PREC_NONE},
    [TK_SLASH]           = {NULL, parse_factor, PREC_FACTOR},
    [TK_SLASH_EQUAL]     = {NULL, NULL, PREC_NONE},
    [TK_STAR]            = {NULL, parse_factor, PREC_FACTOR},
    [TK_STAR_EQUAL]      = {NULL, NULL, PREC_NONE},
    [TK_BANG]            = {NULL, NULL, PREC_NONE},
    [TK_BANG_EQUAL]      = {NULL, parse_equality, PREC_EQUALITY},
    [TK_EQUAL]           = {NULL, parse_assignment, PREC_ASSIGNMENT},
    [TK_EQUAL_EQUAL]     = {NULL, parse_equality, PREC_EQUALITY},
    [TK_PERCENT]         = {NULL, parse_factor, PREC_FACTOR},
    [TK_PERCENT_EQUAL]   = {NULL, NULL, PREC_NONE},
    [TK_XOR]             = {NULL, parse_bitwise, PREC_BIT_XOR},
    [TK_XOR_EQUAL]       = {NULL, NULL, PREC_NONE},
    [TK_PIPE]            = {NULL, parse_bitwise, PREC_BIT_OR},
    [TK_PIPE_EQUAL]      = {NULL, NULL, PREC_NONE},
    [TK_AMPERSAND]       = {NULL, parse_bitwise, PREC_BIT_AND},
    [TK_AMPERSAND_EQUAL] = {NULL, NULL, PREC_NONE},
    [TK_GREATER]         = {NULL, parse_comparison, PREC_COMPARISON},
    [TK_GREATER_EQUAL]   = {NULL, parse_comparison, PREC_COMPARISON},
    [TK_RSHIFT]          = {NULL, parse_bitwise, PREC_BIT_SHIFT},
    [TK_RSHIFT_EQUAL]    = {NULL, NULL, PREC_NONE},
    [TK_LESS]            = {NULL, parse_comparison, PREC_COMPARISON},
    [TK_LESS_EQUAL]      = {NULL, parse_comparison, PREC_COMPARISON},
    [TK_LSHIFT]          = {NULL, parse_bitwise, PREC_BIT_SHIFT},
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

static inline const ParseRule *getRule(TokenType type) {
    return &rules[type];
}

static ASTNode *parsePrecedence(Parser *p, Precedence precedence);
static ASTNode *expression(Parser *p);

static ASTNode *parse_identifier(Parser *p) {
    // FIXME: this function currently creates a variable node,
    //        make it crate a proper identifier, maybe ASTIdentifier.

    char *name = stringNCopy(previous(p).lexeme, previous(p).length);
    int id = symbolExists(&p->prog->globals, name)   ?
             getIdFromName(&p->prog->globals, name)  :
             addSymbol(&p->prog->globals, name, NULL);
    freeString(name);
    return newVarNode(previous(p).location, id);
}

static ASTNode *parse_number(Parser *p) {
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
    return newNumberNode((int)strtol(lexeme, NULL, base), previous(p).location);
}
static ASTNode *parse_grouping(Parser *p) {
    ASTNode *expr = expression(p);
    consume(p, TK_RPAREN, "expected ')' after expression");
    return expr;
}
static ASTNode *parse_unary(Parser *p) {
    TokenType operatorType = previous(p).type;
    Location operatorLoc = previous(p).location;
    ASTNode *operand = parsePrecedence(p, PREC_UNARY);
    switch(operatorType) {
        case TK_PLUS:
            // nothing, leave the operand alone.
            break;
        case TK_MINUS:
            operand = newUnaryNode(ND_NEG, operatorLoc, operand);
            break;
        default:
            UNREACHABLE();
    }
    return operand;
}
static ASTNode *parse_term(Parser *p, ASTNode *left) {
    TokenType operatorType = previous(p).type;
    Location operatorLoc = previous(p).location;
    Precedence prec = getRule(operatorType)->precedence;
    ASTNode *right = parsePrecedence(p, (Precedence)prec + 1);

    switch(operatorType) {
        case TK_PLUS:
            left = newBinaryNode(ND_ADD, operatorLoc, left, right);
            break;
        case TK_MINUS:
            left = newBinaryNode(ND_SUB, operatorLoc, left, right);
            break;
        default:
            UNREACHABLE();
    }
    return left;
}
static ASTNode *parse_factor(Parser *p, ASTNode *left) {
    TokenType operatorType = previous(p).type;
    Location operatorLoc = previous(p).location;
    Precedence prec = getRule(operatorType)->precedence;
    ASTNode *right = parsePrecedence(p, (Precedence)prec + 1);

    switch(operatorType) {
        case TK_STAR:
            left = newBinaryNode(ND_MUL, operatorLoc, left, right);
            break;
        case TK_SLASH:
            left = newBinaryNode(ND_DIV, operatorLoc, left, right);
            break;
        case TK_PERCENT:
            left = newBinaryNode(ND_REM, operatorLoc, left, right);
            break;
        default:
            UNREACHABLE();
    }
    return left;
}
static ASTNode *parse_equality(Parser *p, ASTNode *left) {
    TokenType operatorType = previous(p).type;
    Location operatorLoc = previous(p).location;
    Precedence prec = getRule(operatorType)->precedence;
    ASTNode *right = parsePrecedence(p, (Precedence)prec + 1);

    switch(operatorType) {
        case TK_EQUAL_EQUAL:
            left = newBinaryNode(ND_EQ, operatorLoc, left, right);
            break;
        case TK_BANG_EQUAL:
            left = newBinaryNode(ND_NE, operatorLoc, left, right);
            break;
        default:
            UNREACHABLE();
    }
    return left;
}
static ASTNode *parse_comparison(Parser *p, ASTNode *left) {
    TokenType operatorType = previous(p).type;
    Location operatorLoc = previous(p).location;
    Precedence prec = getRule(operatorType)->precedence;
    ASTNode *right = parsePrecedence(p, (Precedence)prec + 1);

    switch(operatorType) {
        case TK_GREATER:
            left = newBinaryNode(ND_GT, operatorLoc, left, right);
            break;
        case TK_GREATER_EQUAL:
            left = newBinaryNode(ND_GE, operatorLoc, left, right);
            break;
        case TK_LESS:
            left = newBinaryNode(ND_LT, operatorLoc, left, right);
            break;
        case TK_LESS_EQUAL:
            left = newBinaryNode(ND_LE, operatorLoc, left, right);
            break;
        default:
            UNREACHABLE();
    }
    return left;
}
static ASTNode *parse_bitwise(Parser *p, ASTNode *left) {
    TokenType operatorType = previous(p).type;
    Location operatorLoc = previous(p).location;
    Precedence prec = getRule(operatorType)->precedence;
    ASTNode *right = parsePrecedence(p, (Precedence)prec + 1);

    switch(operatorType) {
        case TK_XOR:
            left = newBinaryNode(ND_XOR, operatorLoc, left, right);
            break;
        case TK_PIPE:
            left = newBinaryNode(ND_BIT_OR, operatorLoc, left, right);
            break;
        case TK_AMPERSAND:
            left = newBinaryNode(ND_BIT_AND, operatorLoc, left, right);
            break;
        case TK_RSHIFT:
            left = newBinaryNode(ND_BIT_RSHIFT, operatorLoc, left, right);
            break;
        case TK_LSHIFT:
            left = newBinaryNode(ND_BIT_LSHIFT, operatorLoc, left, right);
            break;
        default:
            UNREACHABLE();
    }
    return left;
}

static ASTNode *parse_assignment(Parser *p, ASTNode *lvalue) {
    Location equalsLoc = previous(p).location;
    ASTNode *rvalue = expression(p);
    return newBinaryNode(ND_ASSIGN, equalsLoc, lvalue, rvalue);
}

static ASTNode *parsePrecedence(Parser *p, Precedence precedence) {
    advance(p);
    PrefixParseFn prefix = getRule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, previous(p), "expected an expression");
        return NULL;
    }
    ASTNode *left = prefix(p);
    while(precedence <= getRule(peek(p).type)->precedence) {
        advance(p);
        InfixParseFn infix = getRule(previous(p).type)->infix;
        left = infix(p, left);
    }
    return left;
}

static ASTNode *expression(Parser *p) {
    // parse with the lowest precedence
    ASTNode *expr = parsePrecedence(p, PREC_ASSIGNMENT);
    // prevent uninitialized nodes from being returned
    return p->had_error ? NULL : expr;
}

/*** statement parser ***/
// statements
static ASTNode *expr_stmt(Parser *p) {
    ASTNode *n = newUnaryNode(ND_EXPR_STMT, previous(p).location, expression(p));
    consume(p, TK_SEMICOLON, "expected ';' after expression");
    return n;
}

static ASTNode *statement(Parser *p) {
    return expr_stmt(p);
}

// declarations
static ASTNode *var_decl(Parser *p) {
    // assumes var keyword already consumed
    // save the location of the 'var' keyword.
    Location var_loc = previous(p).location;
    if(!consume(p, TK_IDENTIFIER, "Expected identifier after 'var'")) {
        return NULL;
    }
    char *name = stringNCopy(previous(p).lexeme, previous(p).length);
    int id = addSymbol(&p->prog->globals, name, NULL);
    freeString(name); // FIXME: find a better way to get the identifier string.
    ASTNode *n = newVarNode(var_loc, id);

    if(match(p, TK_EQUAL)) {
        n = newBinaryNode(ND_ASSIGN, previous(p).location, n, expression(p));
    }

    if(!consume(p, TK_SEMICOLON, "Expected ';' after variable declaration")) {
        freeAST(n);
        return NULL;
    }
    return n;
}

static ASTNode *declaration(Parser *p) {
    ASTNode *n = NULL;
    if(match(p, TK_VAR)) {
        n = var_decl(p);
    } else {
        n = statement(p);
    }
    return n;
}

static void synchronize(Parser *p) {
    // synchronize to statement boundaries
    while(peek(p).type != TK_EOF) {
        if(previous(p).type == TK_SEMICOLON) {
            return;
        }
        switch(peek(p).type) {
            case TK_MODULE:
            case TK_EXPORT:
            case TK_IMPORT:
            case TK_USING:
            case TK_TYPE:
            case TK_ENUM:
            case TK_STRUCT:
            case TK_CONST:
            case TK_VAR:
            case TK_FN:
            case TK_FOR:
            case TK_WHILE:
            case TK_IF:
            case TK_SWITCH:
            case TK_RETURN:
                return;
            default:
                break;
        }

        advance(p);
    }
}

// === GRAMMAR ===
// == EXPRESSIONS ==
// primary       -> '(' expression ')'
//                | NUMBER
//                | IDENTIFIER
// unary         -> ('+' | '-') primary
//                  | primary
// factor        -> unary (('*' | '/') unary)*
// term          -> factor (('+' | '-') factor)*
// bit_shift     -> term (('<<' | '>>') term)*
// comparison    -> bit_shift (('>' | '>=' | '<' | '<=') bit_shift)*
// equality      -> comparison (('==' | '!=') comparison)*
// bit_and       -> equality ('&' equality)*
// bit_xor       -> bit_and ('^' bit_and)*
// bit_or        -> bit_xor ('|' bit_xor)*
// assignment    -> IDENTIFIER '=' assignment
//                | bit_or
// expression    -> assignment
//
// == STATEMENTS ==
// TODO: add function arguments
// fn_decl       -> 'fn' IDENTIFIER '(' ')' ('->' TYPE)? block
// var_decl      -> 'var' IDENTIFIER (':' TYPE)? ('=' expression)? ';'
// expr_stmt     -> expression ';'
// print_stmt    -> 'print' expression ';'
// return_stmt   -> 'return' expression? ';'
// if_stmt       -> 'if' expression block ('else' block)?
// for_stmt      -> 'for' (var_decl | expr_stmt | ';') expression? ';' expression? block
// while_stmt    -> 'while' expression block
// block         -> '{' declaration* '}'
// statement     -> print_stmt
//                | if_stmt
//                | for_stmt
//                | while_stmt
//                | return_stmt
//                | block
//                | expr_stmt
// TODO: support closures by fn_decl inside block or only fn literal?
// declaration   -> var_decl
//                | statement
// program       -> (fn_decl | var_decl)* EOF
bool parserParse(Parser *p) {
    advance(p);
    while(peek(p).type != TK_EOF) {
        ASTNode *node = declaration(p);
        if(node) {
            arrayPush(&p->prog->statements, node);
        }

        // reset the parser state
        if(p->panic_mode) {
            p->panic_mode = false;
            synchronize(p);
        }
        advance(p);
    }
    return !p->had_error;
}
