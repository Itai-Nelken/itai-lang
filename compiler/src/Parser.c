#include <string.h> // memset()
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Token.h"
#include "ast.h"
#include "Error.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"

void parserInit(Parser *p, Compiler *c) {
    memset(p, 0, sizeof(*p));
    p->compiler = c;
    p->current_token.type = TK_GARBAGE;
    p->previous_token.type = TK_GARBAGE;
}

void parserFree(Parser *p) {
    memset(p, 0, sizeof(*p));
}

static inline bool is_eof(Parser *p) {
    return p->current_token.type == TK_EOF;
}

static inline Token advance(Parser *p) {
    // skip TK_GARBAGE tokens.
    // We don't know how to handle them, and the Scanner
    // has already reported errors for them anyway.
    Token tok;
    while((tok = scannerNextToken(p->scanner)).type == TK_GARBAGE) /* nothing */ ;
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

// if a valid String is provided as message, it will be freed.
// uses previous tokens location.
static void error(Parser *p, char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, true, previous(p).location, message);
    compilerAddError(p->compiler, err);
    if(stringIsValid(message)) {
        stringFree(message);
    }
}

static bool consume(Parser *p, TokenType expected) {
    if(peek(p).type != expected) {
        error(p, stringFormat("Expected '%s' but got '%s'!", tokenTypeString(expected), tokenTypeString(peek(p).type)));
        return false;
    }
    advance(p);
    return true;
}

static bool match(Parser *p, TokenType expected) {
    if(peek(p).type != expected) {
        return false;
    }
    advance(p);
    return true;
}

typedef enum precedences {
    PREC_LOWEST     = 0,  // lowest
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

// pre-declarations for parse functions and rule table
static ASTNode *parse_precedence(Parser *p, Precedence min_prec);
static ASTNode *parse_prefix_expression(Parser *p);
static ASTNode *parse_infix_expression(Parser *p, ASTNode *lhs);

static ParseRule rules[] = {
    //   token     |    prefix                 | infix | precedence
    [TK_LPAREN]      = {parse_prefix_expression, NULL, PREC_LOWEST},
    [TK_RPAREN]      = {NULL, NULL, PREC_LOWEST},
    [TK_PLUS]        = {parse_prefix_expression, parse_infix_expression, PREC_TERM},
    [TK_MINUS]       = {parse_prefix_expression, parse_infix_expression, PREC_TERM},
    [TK_STAR]        = {NULL, parse_infix_expression, PREC_FACTOR},
    [TK_SLASH]       = {NULL, parse_infix_expression, PREC_FACTOR},
    [TK_SEMICOLON]   = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL]       = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL_EQUAL] = {NULL, parse_infix_expression, PREC_EQUALITY},
    [TK_BANG]        = {NULL, NULL, PREC_LOWEST},
    [TK_BANG_EQUAL]  = {NULL, parse_infix_expression, PREC_EQUALITY},
    [TK_NUMBER]      = {parse_prefix_expression, NULL, PREC_LOWEST},
    [TK_IF]          = {NULL, NULL, PREC_LOWEST},
    [TK_ELSE]        = {NULL, NULL, PREC_LOWEST},
    [TK_IDENTIFIER]  = {NULL, NULL, PREC_LOWEST},
    [TK_GARBAGE]     = {NULL, NULL, PREC_LOWEST},
    [TK_EOF]         = {NULL, NULL, PREC_LOWEST}
};

static inline ParseRule *get_rule(TokenType type) {
    return &rules[(i32)type];
}

// nud == null denotation (has nothing to the left)
static inline PrefixParseFn get_nud(TokenType type) {
    return get_rule(type)->prefix;
}

// led == left denotation (has something to the left)
static inline InfixParseFn get_led(TokenType type) {
    return get_rule(type)->infix;
}

static inline Precedence get_precedence(TokenType type) {
    return get_rule(type)->precedence;
}

/***
 * call parser function 'fn' with argument 'parser'
 * and return NULL from the current function if it returns NULL.
 *
 * @param fn A Parser function with the signature 'ASTNode *(*)(Parser *)'.
 * @param parser A pointer to a Parser that will be passed to 'fn'.
 * @param on_null An optional *single* ASTNode to free on failure.
 * @return The 'ASTNode *' returned from 'fn' or returns NULL from the *current* function.
 ***/
#define TRY_PARSE(fn, parser, on_null) ({ASTNode *_tmp_res = (fn)((parser)); if(!_tmp_res) { astFree(on_null); return NULL; } _tmp_res;})

/***
 * call parse_precedence() with arguments 'parser' and 'prec'.
 * and return NULL from the current function if it returns NULL.
 *
 * @param parser A pointer to a Parser that will be passed to 'fn'.
 * @param prec The precedence to pass to parse_precedence().
 * @param on_null An optional *single* ASTNode to free on failure.
 * @return The 'ASTNode *' returned from parse_precedence() or returns NULL from the *current* function.
 ***/
#define TRY_PARSE_PREC(parser, prec, on_null) ({ASTNode *_tmp_res = parse_precedence((parser), (prec)); if(!_tmp_res) { astFree(on_null); return NULL; } _tmp_res;})

/***
 * consume 'expected' and return NULL from the current function on failure.
 *
 * @param parser A pointer to a Parser that will be passed to 'fn'.
 * @param expected The expected TokenType.
 * @param on_null An optional *single* ASTNode to free on failure.
 * @return true if 'expected' waas consumed succesfully. returns NULL from the current function if not.
 ***/
#define CONSUME(parser, expected, on_null) ({if(!consume((parser), (expected))) { astFree(on_null); return NULL;} true;})

static ASTNodeType token_to_node_type(TokenType tk) {
    switch(tk) {
        case TK_PLUS:
            return ND_ADD;
        case TK_MINUS:
            return ND_SUB;
        case TK_STAR:
            return ND_MUL;
        case TK_SLASH:
            return ND_DIV;
        case TK_EQUAL_EQUAL:
            return ND_EQ;
        case TK_BANG_EQUAL:
            return ND_NE;
        case TK_NUMBER:
            return ND_NUMBER;
        default:
            UNREACHABLE();
    }
}

static ASTNode *parse_prefix_expression(Parser *p) {
    switch(previous(p).type) {
        case TK_NUMBER:
            return astNewNumberNode(previous(p).location, previous(p).as.number_constant);
        case TK_PLUS:
        case TK_MINUS: {
            TokenType operator = previous(p).type;
            Location operator_loc = previous(p).location;
            ASTNode *operand = parse_precedence(p, PREC_UNARY);
            if(operator == TK_PLUS) {
                operand->location = locationMerge(operator_loc, operand->location);
                return operand;
            }
            return astNewUnaryNode(ND_NEG, locationMerge(operator_loc, operand->location), operand);
        }
        case TK_LPAREN: {
            ASTNode *expr = parse_precedence(p, PREC_LOWEST);
            CONSUME(p, TK_RPAREN, expr);
            return expr;
        }
        default:
            UNREACHABLE();
    }
}

static ASTNode *parse_infix_expression(Parser *p, ASTNode *lhs) {
    ASTNodeType type = token_to_node_type(previous(p).type);
    Location expr_loc = locationMerge(lhs->location, previous(p).location);
    ASTNode *rhs = TRY_PARSE_PREC(p, (Precedence)(get_precedence(previous(p).type + 1)), lhs);
    //ASTNode *rhs = parse_precedence(p, (Precedence)(get_precedence(previous(p).type + 1)));
    //if(!rhs) {
    //    astFree(lhs);
    //    return NULL;
    //}
    return astNewBinaryNode(type, locationMerge(expr_loc, rhs->location), lhs, rhs);
}

static ASTNode *parse_precedence(Parser *p, Precedence min_prec) {
    advance(p);
    PrefixParseFn nud = get_nud(previous(p).type);
    if(!nud) {
        error(p, "Expected an expression!");
        return NULL;
    }
    ASTNode *lhs = nud(p);
    if(!lhs) {
        return NULL;
    }
    while(!is_eof(p) && min_prec < get_precedence(peek(p).type)) {
        advance(p);
        InfixParseFn led = get_led(previous(p).type);
        lhs = led(p, lhs);
    }
    return lhs;
}

static inline ASTNode *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_LOWEST);
}

// pre-declarations
static ASTNode *parse_statement(Parser *p);

// block -> '{' statement* '}'
static ASTNode *parse_block(Parser *p) {
    // assumes '{' was already consumed.
    Array body;
    arrayInit(&body);
    ASTListNode *n = AS_LIST_NODE(astNewListNode(ND_BLOCK, previous(p).location, body));
    while(!is_eof(p) && peek(p).type != TK_RBRACE) {
        arrayPush(&n->body, TRY_PARSE(parse_statement, p, AS_NODE(n)));
    }
    if(peek(p).type != TK_RBRACE) {
        error(p, stringFormat("Expected '}' after block but got '%s'.", tokenTypeString(peek(p).type)));
        astFree(AS_NODE(n));
        return NULL;
    }
    advance(p); // consume the '}'.
    return AS_NODE(n);
}

// if_stmt -> 'if' expression block ('else' (if_stmt | block))?
static ASTNode *parse_if_stmt(Parser *p) {
    // assumes 'if' was already consumed.
    Location if_loc = previous(p).location;
    ASTNode *condition = TRY_PARSE(parse_expression, p, 0);
    CONSUME(p, TK_LBRACE, condition);
    ASTNode *body = TRY_PARSE(parse_block, p, condition);

    ASTNode *if_ = astNewConditionalNode(ND_IF, locationMerge(if_loc, locationMerge(condition->location, body->location)), condition, AS_LIST_NODE(body), NULL);
    if(!match(p, TK_ELSE)) {
        return if_;
    }
    if(match(p, TK_LBRACE)) {
        AS_CONDITIONAL_NODE(if_)->else_ = TRY_PARSE(parse_block, p, if_);
    } else if(match(p, TK_IF)) {
        AS_CONDITIONAL_NODE(if_)->else_ = TRY_PARSE(parse_if_stmt, p, if_);
    } else {
        error(p, stringFormat("Expected one of ['if', '{'] after 'else' but got '%s'", tokenTypeString(peek(p).type)));
        astFree(if_);
        return NULL;
    }
    return if_;
}

// expr_stmt -> expression ';'
static ASTNode *parse_expr_stmt(Parser *p) {
    ASTNode *expr = parse_expression(p);
    if(!expr) {
        return NULL;
    }
    if(!consume(p, TK_SEMICOLON)) {
        astFree(expr);
        return NULL;
    }
    return astNewUnaryNode(ND_EXPR_STMT, locationMerge(expr->location, previous(p).location), expr);
}

// statement -> if_stmt | expr_stmt
static ASTNode *parse_statement(Parser *p) {
    ASTNode *result = NULL;
    if(match(p, TK_IF)) {
        result = parse_if_stmt(p);
    } else {
        result = parse_expr_stmt(p);
    }
    return result;
}

#undef CONSUME
#undef TRY_PARSE_PREC
#undef TRY_PARSE

ASTNode *parserParse(Parser *p, Scanner *s) {
    p->scanner = s;
    // prime the parser
    advance(p);
    // if the scanner failed to set the source, we can't do anything.
    if(p->scanner->failed_to_set_source) {
        p->scanner = NULL;
        return NULL;
    }
    ASTNode *expr = parse_statement(p);
    p->scanner = NULL;
    return expr;
}
