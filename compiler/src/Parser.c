#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include "common.h"
#include "memory.h"
#include "utilities.h"
#include "Strings.h"
#include "Array.h"
#include "ast.h"
#include "Token.h"
#include "Errors.h"
#include "Scanner.h"
#include "Parser.h"

void initParser(Parser *p, const char *filename, char *source) {
    p->current_fn = NULL;
    p->current_expr = NULL;
    p->had_error = false;
    p->panic_mode = false;
    p->scope_depth = 0;
    p->scopes = NULL;
    initScanner(&p->scanner, filename, source);
}

static void free_obj_callback(void *obj, void *cl) {
    UNUSED(cl);
    ASTObj *o = (ASTObj *)obj;
    // don't free the name as it's used in the ASTFunctions
    FREE(o);
}

void freeParser(Parser *p) {
    freeScanner(&p->scanner);
    if(p->scopes != NULL) {
        while(p->scopes != NULL) {
            Scope *previous = p->scopes->previous;
            arrayMap(&p->scopes->locals, free_obj_callback, NULL);
            freeArray(&p->scopes->locals);
            FREE(p->scopes);
            p->scopes = previous;
        }
    }
}

static Scope *newScope(int depth) {
    Scope *sc = CALLOC(1, sizeof(*sc));
    sc->depth = depth;
    initArray(&sc->locals);
    sc->previous = NULL;
    return sc;
}

static void beginScope(Parser *p) {
    p->scope_depth++;
    Scope *sc = newScope(p->scope_depth);
    sc->previous = p->scopes;
    p->scopes = sc;
}

// FIXME: handle local variables that are inside the current function's scope
static void endScope(Parser *p) {
    assert(p->scope_depth > 0);
    Scope *sc = p->scopes;
    p->scopes = sc->previous;
    // don't free the local names as they are also referenced
    // in the ASTObj's inside the ASTNode's, and will be freed
    // when the ASTNode's will be freed.
    freeArray(&sc->locals);
    FREE(sc);
    p->scope_depth--;
}

static inline Scope *currentScope(Parser *p) {
    return p->scopes;
}

static ASTObj *find_local(Parser *p, char *name) {
    Scope *sc = p->scopes;
    while(sc != NULL) {
        for(int i = 0; i < (int)sc->locals.used; ++i) {
            ASTObj *l = ARRAY_GET_AS(ASTObj *, &sc->locals, i);
            if(stringEqual(l->name, name)) {
                return l;
            }
        }
        sc = sc->previous;
    }
    return NULL;
}

static void add_local(Parser *p, char *name) {
    ASTObj *local = CALLOC(1, sizeof(*local));
    local->type = OBJ_LOCAL;
    local->name = stringIsValid(name) ? name : stringCopy(name);
    // if the local already exists, add ".<scope_depth>" to it.
    if(find_local(p, name) != NULL) {
        stringAppend(local->name, ".%d", p->scope_depth);
    }
    arrayPush(&p->scopes->locals, local); // for finding locals
    assert(p->current_fn != NULL);
    arrayPush(&p->current_fn->locals, local); // for codegeneration
}

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

static inline void warning(Token tok, const char *message) {
    printError(ERR_WARNING, tok.location, message);
}

static Token advance(Parser *p) {
    Token tok;
    while((tok = nextToken(&p->scanner)).type == TK_ERROR) {
        error(p, tok, tok.errmsg);
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

static bool consume(Parser *p, TokenType expected, const char *message) {
    if(peek(p).type != expected) {
        error(p, peek(p), message);
        return false;
    }
    advance(p);
    return true;
}

typedef enum precedence {
    PREC_NONE       = 0,
    PREC_ASSIGNMENT = 1, // infix =
    PREC_BIT_OR     = 2,  // infix |
    PREC_BIT_XOR    = 3,  // infix ^
    PREC_BIT_AND    = 4,  // infix &
    PREC_EQUALITY   = 5, // infix == !=
    PREC_COMPARISON = 6, // infix > >= < <=
    PREC_BIT_SHIFT  = 7, // infix << >>
    PREC_TERM       = 8, // infix + -
    PREC_FACTOR     = 9, // infix * /
    PREC_UNARY      = 10, // unary + -
    PREC_CALL       = 11, // ()
    PREC_PRIMARY    = 12  // highest
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
static void parse_call(Parser *p);
static void parse_binary(Parser *p);
static void parsePrecedence(Parser *p, Precedence prec);
static ASTNode *expression(Parser *p);

static ParseRule rules[TK__COUNT] = {
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
    //ASTNode *n = newNode(ND_VAR, NULL, NULL, previous(p).location);
    Location loc = previous(p).location;

    char *name = stringNCopy(previous(p).lexeme, previous(p).length);
    //n->as.var.name = name;
    
    ASTObjType obj_type;
    if(p->scope_depth > 0 && find_local(p, name) != NULL) {
        //n->as.var.type = OBJ_LOCAL;
        obj_type = OBJ_LOCAL;
    } else {
        //n->as.var.type = OBJ_GLOBAL;
        obj_type = OBJ_GLOBAL;
    }
    
    ASTNode *n = newObjNode(ND_VAR, loc, (ASTObj){.name = name, .type = obj_type});
    if(canAssign && peek(p).type == TK_EQUAL) {
        advance(p);
        //n = newNode(ND_ASSIGN, n, expression(p), previous(p).location);
        n = newBinaryNode(ND_ASSIGN, previous(p).location, n, expression(p));
    }
    p->current_expr = n;
}

static void parse_call(Parser *p) {
    // IDENTIFIER '(' are already consumed
    if(!consume(p, TK_RPAREN, "Expected ')'")) {
        p->current_expr = NULL;
        return;
    }
    //char *name = p->current_expr->as.var.name;
    char *name = AS_OBJ_NODE(p->current_expr)->obj.name;
    freeAST(p->current_expr); // don't need this
    //p->current_expr = newNode(ND_FN_CALL, NULL, NULL, previous(p).location);
    //p->current_expr->as.name = name;
    p->current_expr = newObjNode(ND_FN_CALL, previous(p).location, (ASTObj){.name = name});
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
            p->current_expr = newUnaryNode(ND_NEG, previous(p).location, p->current_expr);
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
            //p->current_expr = newNode(ND_BIT_OR, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_BIT_OR, previous(p).location, left, p->current_expr);
            break;
        case TK_XOR:
            //p->current_expr = newNode(ND_XOR, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_XOR, previous(p).location, left, p->current_expr);
            break;
        case TK_AMPERSAND:
            //p->current_expr = newNode(ND_BIT_AND, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_BIT_AND, previous(p).location, left, p->current_expr);
            break;
        case TK_RSHIFT:
            //p->current_expr = newNode(ND_BIT_RSHIFT, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_BIT_RSHIFT, previous(p).location, left, p->current_expr);
            break;
        case TK_LSHIFT:
            //p->current_expr = newNode(ND_BIT_LSHIFT, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_BIT_LSHIFT, previous(p).location, left, p->current_expr);
            break;
        case TK_EQUAL_EQUAL:
            //p->current_expr = newNode(ND_EQ, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_EQ, previous(p).location, left, p->current_expr);
            break;
        case TK_BANG_EQUAL:
            //p->current_expr = newNode(ND_NE, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_NE, previous(p).location, left, p->current_expr);
            break;
        case TK_GREATER:
            //p->current_expr = newNode(ND_GT, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_GT, previous(p).location, left, p->current_expr);
            break;
        case TK_GREATER_EQUAL:
            //p->current_expr = newNode(ND_GE, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_GE, previous(p).location, left, p->current_expr);
            break;
        case TK_LESS:
            //p->current_expr = newNode(ND_LT, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_LT, previous(p).location, left, p->current_expr);
            break;
        case TK_LESS_EQUAL:
            //p->current_expr = newNode(ND_LE, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_LE, previous(p).location, left, p->current_expr);
            break;
        case TK_PLUS:
            //p->current_expr = newNode(ND_ADD, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_ADD, previous(p).location, left, p->current_expr);
            break;
        case TK_MINUS:
            //p->current_expr = newNode(ND_SUB, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_SUB, previous(p).location, left, p->current_expr);
            break;
        case TK_STAR:
            //p->current_expr = newNode(ND_MUL, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_MUL, previous(p).location, left, p->current_expr);
            break;
        case TK_SLASH:
            //if(p->current_expr->type == ND_NUM && p->current_expr->as.literal.int32 == 0) {
            if(p->current_expr->type == ND_NUM && AS_LITERAL_NODE(p->current_expr)->as.int32 == 0) {
                warning(previous(p), "division by 0");
            }
            //p->current_expr = newNode(ND_DIV, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_DIV, previous(p).location, left, p->current_expr);
            break;
        case TK_PERCENT:
            //if(p->current_expr->type == ND_NUM && p->current_expr->as.literal.int32 == 0) {
            if(p->current_expr->type == ND_NUM && AS_LITERAL_NODE(p->current_expr)->as.int32 == 0) {
                warning(previous(p), "causes division by 0");
            }
            //p->current_expr = newNode(ND_REM, left, p->current_expr, previous(p).location);
            p->current_expr = newBinaryNode(ND_REM, previous(p).location, left, p->current_expr);
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
    // to prevent uninitialized nodes (invalid pointers) from being returned.
    if(p->had_error) {
        return NULL;
    }
    return p->current_expr;
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

/*** declarations ***/
static ASTNode *block(Parser *p);

static ASTFunction *fn_decl(Parser *p) {
    // 'fn' is already consumed
    if(!consume(p, TK_IDENTIFIER, "Expected an identifier after 'fn'")) {
        return NULL;
    }
    char *name = stringNCopy(previous(p).lexeme, previous(p).length);
    Location loc = previous(p).location;

    if(!consume(p, TK_LPAREN, "Expected '('")) {
        freeString(name);
        return NULL;
    }
    // TODO: parameters
    if(!consume(p, TK_RPAREN, "Expected ')'")) {
        freeString(name);
        return NULL;
    }

    // TODO: return type

    if(!consume(p, TK_LBRACE, "Expected '{'")) {
        freeString(name);
        return NULL;
    }
    ASTFunction *fn = newFunction(name, loc, NULL);
    freeString(name); // newFunction() makes it's own copy
    // NOTE: assumes there are only toplevel functions.
    p->current_fn = fn;
    beginScope(p);
    fn->body = block(p);
    endScope(p);
    p->current_fn = NULL;

    return fn;
}

// FIXME: var node returned if no assignment.
//        that means the codegen simply emits
//        the instruction to get the value for no reason.
static ASTNode *var_decl(Parser *p) {
    // 'var' is already consumed
    if(!consume(p, TK_IDENTIFIER, "Expected an identifier after 'var'")) {
        return NULL;
    }
    char *name = stringNCopy(previous(p).lexeme, previous(p).length);
    
    // TODO: check for types
    
    //ASTNode *var = newNode(ND_VAR, NULL, NULL, previous(p).location);
    //var->as.var.name = name;
    Location loc = previous(p).location;
    ASTObj obj = {
        .name = name
    };
    if(p->scope_depth > 0) {
        add_local(p, name);
        //var->as.var.type = OBJ_LOCAL;
        obj.type = OBJ_LOCAL;
    } else {
        //var->as.var.type = OBJ_GLOBAL;
        obj.type = OBJ_GLOBAL;
    }

    ASTNode *var = newObjNode(ND_VAR, loc, obj);
    if(peek(p).type == TK_EQUAL) {
        advance(p);
        loc = previous(p).location;
        //var = newNode(ND_ASSIGN, var, expression(p), previous(p).location);
        var = newBinaryNode(ND_ASSIGN, loc, var, expression(p));
    }

    // assignment and variable referencing are expressions, but they
    // are also statements (declarations). so wrap the ndoe in en expression
    // statement so it can be walked as an expression.
    var = newUnaryNode(ND_EXPR_STMT, loc, var);
    consume(p, TK_SEMICOLON, "Expected ';' after variable declaration");
    return var;
}

static ASTNode *declaration(Parser *p);

static ASTNode *block(Parser *p) {
    //ASTNode *n = newNode(ND_BLOCK, NULL, NULL, previous(p).location);
    ASTNode *n = newBlockNode(previous(p).location);
    Array *body = &AS_BLOCK_NODE(n)->body;
    //initArray(&n->as.body);
    while(peek(p).type != TK_RBRACE && peek(p).type != TK_EOF) {
        //arrayPush(&n->as.body, declaration(p));
        arrayPush(body, declaration(p));
    }
    consume(p, TK_RBRACE, "Expected '}' after block");
    return n;
}

/*** statements ***/
static ASTNode *expr_stmt(Parser *p) {
    ASTNode *n = newUnaryNode(ND_EXPR_STMT, previous(p).location, expression(p));
    consume(p, TK_SEMICOLON, "Expected ';' after expression");
    return n;
}

static ASTNode *return_stmt(Parser *p) {
    ASTNode *n = NULL;
    if(peek(p).type != TK_SEMICOLON) {
        n = newUnaryNode(ND_RETURN, previous(p).location, expression(p));
    } else {
        n = newUnaryNode(ND_RETURN, previous(p).location, NULL);
    }
    consume(p, TK_SEMICOLON, "Expected ';' after 'return' statement");
    return n;
}

static ASTNode *if_stmt(Parser *p) {
    //ASTNode *n = newNode(ND_IF, NULL, NULL, previous(p).location);
    //n->as.conditional.condition = expression(p);
    Location loc = previous(p).location;
    ASTNode *condition = expression(p);
    consume(p, TK_LBRACE, "Expected '{'");
    //n->as.conditional.then = block(p);
    ASTNode *then = block(p);
    ASTNode *else_;
    if(peek(p).type == TK_ELSE) {
        advance(p);
        consume(p, TK_LBRACE, "Expected '{'");
        //n->as.conditional.els = block(p);
        else_ = block(p);
    } else {
        //n->as.conditional.els = NULL;
        else_ = NULL;
    }
    return newConditionalNode(ND_IF, loc, condition, then, else_);
}

static ASTNode *while_stmt(Parser *p) {
    //ASTNode *n = newNode(ND_LOOP, NULL, NULL, previous(p).location);
    Location loc = previous(p).location;
    // condition
    //n->as.conditional.condition = expression(p);
    ASTNode *condition = expression(p);
    // body
    consume(p, TK_LBRACE, "Expected '{'");
    //n->as.conditional.then = block(p);
    return newLoopNode(loc, NULL, condition, NULL, block(p));
}

static ASTNode *for_stmt(Parser *p) {
    //ASTNode *n = newNode(ND_LOOP, NULL, NULL, previous(p).location);
    Location loc = previous(p).location;
    ASTNode *init, *cond, *inc, *body;
    beginScope(p);

    // initializer clause
    if(peek(p).type == TK_SEMICOLON) {
        advance(p);
        //n->as.conditional.initializer = NULL;
        init = NULL;
    } else if(peek(p).type == TK_VAR) {
        advance(p);
        //n->as.conditional.initializer = var_decl(p);
        init = var_decl(p);
    } else {
        // expr_stmt consumes the ';'
        //n->as.conditional.initializer = expr_stmt(p);
        init = expr_stmt(p);
    }

    // condition clause
    if(peek(p).type != TK_SEMICOLON) {
        //n->as.conditional.condition = expression(p);
        cond = expression(p);
    } else {
        //n->as.conditional.condition = NULL;
        cond = NULL;
    }
    consume(p, TK_SEMICOLON, "Expected ';'");

    // increment clause
    if(peek(p).type != TK_LBRACE) {
        //n->as.conditional.increment = expression(p);
        inc = expression(p);
    } else {
        //n->as.conditional.increment = NULL;
        inc = NULL;
    }

    // body
    consume(p, TK_LBRACE, "Expected '{'");
    //n->as.conditional.then = block(p);
    body = block(p);

    endScope(p);
    return newLoopNode(loc, init, cond, inc, body);
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
static ASTNode *statement(Parser *p) {
    ASTNode *n = NULL;
    switch(peek(p).type) {
        case TK_PRINT:
            advance(p);
            n = newUnaryNode(ND_PRINT, previous(p).location, expression(p));
            consume(p, TK_SEMICOLON, "Expected ';' after 'print' statement");
            break;
        case TK_WHILE:
            advance(p);
            n = while_stmt(p);
            break;
        case TK_FOR:
            advance(p);
            n = for_stmt(p);
            break;
        case TK_IF:
            advance(p);
            n = if_stmt(p);
            break;
        case TK_RETURN:
            advance(p);
            n = return_stmt(p);
            break;
        case TK_LBRACE:
            advance(p);
            beginScope(p);
            n = block(p);
            endScope(p);
            break;
        default:
            n = expr_stmt(p);
            break;
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
        switch (peek(p).type) {
            case TK_FN: {
                advance(p);
                ASTFunction *fn = fn_decl(p);
                if(fn != NULL) {
                    arrayPush(&prog->functions, fn);
                }
                break;
            }
            case TK_VAR: {
                advance(p);
                ASTNode *var = var_decl(p);
                if(var != NULL) {
                    arrayPush(&prog->globals, var);
                }
                break;
            }
            default:
                error(p, peek(p), "Only  ['fn', 'var'] allowed in global scope");
                return false; // to prevent an infinite loop
        }

        // reset the parser state
        if(p->panic_mode) {
            p->panic_mode = false;
            synchronize(p);
        }
        p->current_expr = NULL;
    }
    return !p->had_error;
}
