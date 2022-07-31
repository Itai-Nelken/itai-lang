#include <stdio.h>
#include <string.h> // memset()
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Token.h"
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

ASTNode *astNewNumberNode(Location loc, NumberConstant value) {
    ASTNumberNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = ND_NUMBER,
        .location = loc
    };
    n->value = value;
    return AS_NODE(n);
}

ASTNode *astNewUnaryNode(ASTNodeType type, Location loc, ASTNode *operand) {
    ASTUnaryNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->operand = operand;
    return AS_NODE(n);
}

ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right) {
    ASTBinaryNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->left = left;
    n->right = right;
    return AS_NODE(n);
}

static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_ADD]    = "ND_ADD",
        [ND_SUB]    = "ND_SUB",
        [ND_MUL]    = "ND_MUL",
        [ND_DIV]    = "ND_DIV",
        [ND_NEG]    = "ND_NEG",
        [ND_NUMBER] = "ND_NUMBER"
    };
    return names[(i32)type];
}

static const char *node_name(ASTNodeType type) {
    switch(type) {
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
            return "ASTBinaryNode";
        // unary node
        case ND_NEG:
            return "ASTUnaryNode";
        // other nodes
        case ND_NUMBER:
            return "ASTNumberNode";
        default:
            break;
    }
    UNREACHABLE();
}

void astPrint(FILE *to, ASTNode *node) {
    fprintf(to, "%s{\x1b[1mtype:\x1b[0;33m %s\x1b[0m", node_name(node->type), node_type_name(node->type));
    switch(node->type) {
        // other nodes
        case ND_NUMBER:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            printNumberConstant(to, AS_NUMBER_NODE(node)->value);
            break;
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
            fprintf(to, ", \x1b[1mleft:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->left);
            fprintf(to, ", \x1b[1mright:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
            fprintf(to, ", \x1b[1moperand:\x1b[0m ");
            astPrint(to, AS_UNARY_NODE(node)->operand);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

void astFree(ASTNode *node) {
    switch(node->type) {
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
            astFree(AS_BINARY_NODE(node)->left);
            astFree(AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
            astFree(AS_UNARY_NODE(node)->operand);
            break;
        default:
            break;
    }
    FREE(node);
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
    errorInit(err, ERR_ERROR, previous(p).location, message);
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
    // token  | prefix | infix | precedence
    [TK_LPAREN] = {parse_prefix_expression, NULL, PREC_LOWEST},
    [TK_RPAREN] = {NULL, NULL, PREC_LOWEST},
    [TK_PLUS]   = {parse_prefix_expression, parse_infix_expression, PREC_TERM},
    [TK_MINUS]  = {parse_prefix_expression, parse_infix_expression, PREC_TERM},
    [TK_STAR]   = {NULL, parse_infix_expression, PREC_FACTOR},
    [TK_SLASH]  = {NULL, parse_infix_expression, PREC_FACTOR},
    [TK_NUMBER] = {parse_prefix_expression, NULL, PREC_LOWEST},
    [TK_GARBAGE] = {NULL, NULL, PREC_LOWEST},
    [TK_EOF]    = {NULL, NULL, PREC_LOWEST}
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

static bool is_binary_operator(TokenType type) {
    switch(type) {
        case TK_PLUS:
        case TK_MINUS:
        case TK_STAR:
        case TK_SLASH:
            return true;
        default:
            break;
    }
    return false;
}

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
            if(!consume(p, TK_RPAREN)) {
                astFree(expr);
                return NULL;
            }
            return expr;
        }
        default:
            break;
    }
    error(p, stringFormat("Expected one of [<number>, '('] but got '%s'!", tokenTypeString(previous(p).type)));
    return NULL;
}

static ASTNode *parse_infix_expression(Parser *p, ASTNode *lhs) {
    if(is_binary_operator(previous(p).type)) {
        ASTNodeType type = token_to_node_type(previous(p).type);
        Location expr_loc = locationMerge(lhs->location, previous(p).location);
        ASTNode *rhs = parse_precedence(p, (Precedence)(get_precedence(previous(p).type + 1)));
        return astNewBinaryNode(type, locationMerge(expr_loc, rhs->location), lhs, rhs);
    } else {
        error(p, stringFormat("Expected one of ['+', '-', '*', '/'] but got '%s'!", tokenTypeString(previous(p).type)));
    }
    return NULL;
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

ASTNode *parserParse(Parser *p, Scanner *s) {
    p->scanner = s;
    // prime the parser
    advance(p);
    ASTNode *expr = parse_expression(p);
    p->scanner = NULL;
    return expr;
}

