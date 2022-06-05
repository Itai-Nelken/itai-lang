#include <assert.h>
#include <stdbool.h>
#include <stdint.h> // intptr_t
#include "common.h"
#include "memory.h"
#include "Errors.h"
#include "Array.h"
#include "Strings.h"
#include "Symbols.h"
#include "Token.h"
#include "Scanner.h"
#include "ast.h"
#include "Parser.h"

static Scope *newScope(Scope *previous) {
    Scope *sc;
    NEW0(sc);
    initArray(&sc->ids);
    sc->previous = previous;
    return sc;
}

static void freeScope(Scope *sc) {
    freeArray(&sc->ids);
    sc->previous = NULL;
    FREE(sc);
}

void initParser(Parser *p, Scanner *s, ASTProg *prog) {
    assert(p && s && prog);
    p->prog = prog;
    p->current_id_table = &prog->identifiers;
    p->scanner = s;
    p->had_error = false;
    p->panic_mode = false;
    p->scopes = NULL;
    p->scope_depth = 0;
    p->current_fn = NULL;
}

void freeParser(Parser *p) {
    p->prog = NULL;
    p->current_id_table = NULL;
    p->scanner = NULL;
    p->had_error = false;
    p->panic_mode = false;
    while(p->scopes != NULL) {
        Scope *previous = p->scopes->previous;
        freeScope(p->scopes);
        p->scopes = previous;
    }
    p->scopes = NULL;
    p->scope_depth = 0;
    p->current_fn = NULL;
}

/*** Scope management ***/
static inline void beginScope(Parser *p) {
    p->scopes = newScope(p->scopes);
    p->scope_depth++;
}

static inline void endScope(Parser *p) {
    Scope *sc = p->scopes;
    assert(p->scope_depth > 0);
    p->scopes = sc->previous;
    freeScope(sc);
    p->scope_depth--;
}

/*** variables ***/
static void error(Parser *p, Location loc, const char *format, ...);
// globals
// locals
static bool localExists(Parser *p, int id) {
    assert(p->scope_depth > 0);
    Scope *sc = p->scopes;
    while(sc != NULL) {
        for(size_t i = 0; i < sc->ids.used; ++i) {
            if((int)ARRAY_GET_AS(intptr_t, &sc->ids, i) == id) {
                return true;
            }
        }
    }
    return false;
}

static bool registerLocal(Parser *p, Location loc, int id) {
    assert(p->scope_depth > 0);
    // if a local with the same id exists in the current scope, error.
    for(size_t i = 0; i < p->scopes->ids.used; ++i) {
        if((int)ARRAY_GET_AS(intptr_t, &p->scopes->ids, i) == id) {
            error(p, loc, "Redefinition of local variable '%s' in same scope", GET_SYMBOL_AS(ASTIdentifier, p->current_id_table, id)->text);
            return false;
        }
    }
    arrayPush(&p->scopes->ids, (void *)(intptr_t)id);
    return true;
}

/*** utilities ***/
static void error(Parser *p, Location loc, const char *format, ...) {
    // suppress any errors that may be caused by previous errors
    if(p->panic_mode) {
        return;
    }
    p->had_error = true;
    p->panic_mode = true;

    va_list ap;
    va_start(ap, format);
    vprintErrorF(ERR_ERROR, loc, format, ap);
    va_end(ap);
}

static Token advance(Parser *p) {
    Token tok;
    while((tok = nextToken(p->scanner)).type == TK_ERROR) {
        error(p, tok.location, tok.errmsg);
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
        error(p, peek(p).location, message);
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
static ASTNode *parse_call(Parser *p, ASTNode *callee);

static const ParseRule rules[TK__COUNT] = {
    [TK_LPAREN]          = {parse_grouping, parse_call, PREC_CALL},
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
    ASTIdentifier *id = newIdentifier(previous(p).lexeme, previous(p).length);
    int id_idx = -1;
    if(symbolExists(p->current_id_table, id->text)) {
        id_idx = getIdFromName(p->current_id_table, id->text);
        freeIdentifier(id);
    } else {
        id_idx = addSymbol(p->current_id_table, id->text, id);
    }

    return newIdentifierNode(previous(p).location, id_idx);
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

static ASTNode *parse_call(Parser *p, ASTNode *callee) {
    // TODO: arguments
    if(!consume(p, TK_RPAREN, "Expected ')'")) {
        freeAST(callee);
        return NULL;
    }
    return newUnaryNode(ND_CALL, previous(p).location, callee);
}

static ASTNode *parsePrecedence(Parser *p, Precedence precedence) {
    advance(p);
    PrefixParseFn prefix = getRule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, previous(p).location, "expected an expression");
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
static ASTNode *declaration(Parser *p);
// statements
static ASTNode *block(Parser *p) {
    // assumes TK_LBRACE ({) was already consumed
    ASTNode *n = newBlockNode(previous(p).location);
    Array *body = &AS_BLOCK_NODE(n)->body;
    int i = 0;
    while(i <= MAX_DECLS_IN_BLOCK && peek(p).type != TK_RBRACE) {
        ASTNode *decl = declaration(p);
        if(decl) {
            arrayPush(body, (void *)decl);
        }
        i++;
    }

    if(!consume(p, TK_RBRACE, "Expected '}'")) {
        freeAST(n);
        return NULL;
    }
    return n;
}

static ASTNode *expr_stmt(Parser *p) {
    ASTNode *n = newUnaryNode(ND_EXPR_STMT, previous(p).location, expression(p));
    if(!consume(p, TK_SEMICOLON, "expected ';' after expression")) {
        freeAST(n);
        return NULL;
    }
    return n;
}

static ASTNode *return_stmt(Parser *p) {
    ASTNode *n = newUnaryNode(ND_RETURN, previous(p).location, expression(p));
    if(!consume(p, TK_SEMICOLON, "expected ';' after expression")) {
        freeAST(n);
        return NULL;
    }
    return n;
}

static ASTNode *statement(Parser *p) {
    ASTNode *n = NULL;
    if(match(p, TK_RETURN)) {
        n = return_stmt(p);
    } else if(match(p, TK_LBRACE)) {
        beginScope(p);
        n = block(p);
        endScope(p);
    } else {
        n = expr_stmt(p);
    }
    return n;
}

// declarations
static ASTFunction *fn_decl(Parser *p) {
    // assumes fn keyword already consumed

    if(!consume(p, TK_IDENTIFIER, "Expected identifier after 'fn'")) {
        return NULL;
    }
    ASTIdentifierNode *name = AS_IDENTIFIER_NODE(parse_identifier(p));

    if(!consume(p, TK_LPAREN, "Expected '('")) {
        freeAST(AS_NODE(name));
        return NULL;
    }
    // TODO: parameters
    if(!consume(p, TK_RPAREN, "Expected ')'")) {
        freeAST(AS_NODE(name));
        return NULL;
    }
    // TODO: return type

    if(!consume(p, TK_LBRACE, "Expected '{'")) {
        freeAST(AS_NODE(name));
        return NULL;
    }

    ASTFunction *fn = newFunction(name);
    // NOTE: this only works if only toplevel functions are allowed.
    p->current_fn = fn;
    SymTable *old = p->current_id_table;
    p->current_id_table = &fn->identifiers;
    beginScope(p);
    fn->body = AS_BLOCK_NODE(block(p));
    endScope(p);
    p->current_id_table = old;
    p->current_fn = NULL;

    if(fn->body == NULL) {
        freeFunction(fn);
        fn = NULL;
    }
    return fn;
}

static ASTNode *var_decl(Parser *p) {
    // assumes var keyword already consumed

    // save the location of the 'var' keyword.
    Location var_loc = previous(p).location;

    if(!consume(p, TK_IDENTIFIER, "Expected identifier after 'var'")) {
        return NULL;
    }
    ASTNode *n = parse_identifier(p);
    Location id_loc = n->loc;
    n->loc = var_loc;

    if(match(p, TK_EQUAL)) {
        n = newBinaryNode(ND_ASSIGN, previous(p).location, n, expression(p));
    }

    if(!consume(p, TK_SEMICOLON, "Expected ';' after variable declaration")) {
        freeAST(n);
        return NULL;
    }

    if(p->current_fn) {
        ASTIdentifierNode *id_node = n->type == ND_ASSIGN ? AS_IDENTIFIER_NODE(AS_UNARY_NODE(n)->child) : AS_IDENTIFIER_NODE(n);
        arrayPush(&p->current_fn->locals, (void *)n);
        registerLocal(p, id_loc, id_node->id);
        n = NULL;
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
// return_stmt   -> 'return' expression? ';'
// if_stmt       -> 'if' expression block ('else' block)?
// for_stmt      -> 'for' (var_decl | expr_stmt | ';') expression? ';' expression? block
// while_stmt    -> 'while' expression block
// block         -> '{' declaration* '}'
// statement     -> if_stmt
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
        if(match(p, TK_FN)) {
            ASTFunction *fn = fn_decl(p);
            if(fn) {
                arrayPush(&p->prog->functions, (void *)fn);
            }
        } else if(match(p, TK_VAR)) {
            ASTNode *n = var_decl(p);
            if(n) {
                arrayPush(&p->prog->globals, (void *)n);
            }
        } else {
            error(p, peek(p).location, "Only ['fn', 'var'] are allowed in the global scope");
            // advance so we don't get stuck in an infinite loop with the same token.
            advance(p);
        }

        // reset the parser state
        if(p->panic_mode) {
            p->panic_mode = false;
            synchronize(p);
        }
    }
    return !p->had_error;
}
