#include <stdlib.h> // strtoul
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Error.h"
#include "Ast.h"
#include "Scanner.h"
#include "Parser.h"

void parserInit(Parser *p, Scanner *s, Compiler *c) {
    p->compiler = c;
    p->scanner = s;
    p->program = NULL;
    // FIXME: Shoud the tokens have an empty Location as well?
    p->previous_token.type = TK_GARBAGE;
    p->current_token.type = TK_GARBAGE;
    p->had_error = false;
}

void parserFree(Parser *p) {
    p->compiler = NULL;
    p->scanner = NULL;
    p->program = NULL;
    p->previous_token.type = TK_GARBAGE;
    p->current_token.type = TK_GARBAGE;
    p->had_error = false;
}

/** Parser helpers **/

static inline bool is_eof(Parser *p) {
    return p->current_token.type == TK_EOF;
}

static inline Token previous(Parser *p) {
    return p->previous_token;
}

static inline Token current(Parser *p) {
    return p->current_token;
}

static Token advance(Parser *p) {
    // skip TK_GARBAGE tokens.
    // We don't know how to handle them, and the Scanner
    // has already reported errors for them anyway.
    Token tk;
    while((tk = scannerNextToken(p->scanner)).type == TK_GARBAGE) /* nothing */ ;
    p->previous_token = p->current_token;
    p->current_token = tk;
    return tk;
}

// NOTE: If 'message' is a valid String, it will be freed.
static void add_error(Parser *p, bool has_location, Location loc, char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, has_location, loc, message);
    compilerAddError(p->compiler, err);
    if(stringIsValid(message)) {
        stringFree(message);
    }
    p->had_error = true;
}

// NOTE: If 'message' is a valid String, it will be freed.
static inline void error_at(Parser *p, Location loc, char *message) {
    add_error(p, true, loc, message);
}

// NOTES: If 'message' is a valid String, it will be freed.
// The previous tokens' location is used.
static inline void error(Parser *p, char *message) {
    error_at(p, previous(p).location, message);
}

static bool consume(Parser *p, TokenType expected) {
    if(current(p).type != expected) {
        error_at(p, current(p).location, stringFormat("Expected '%s' but got '%s'.", tokenTypeString(expected), tokenTypeString(current(p).type)));
        return false;
    }
    advance(p);
    return true;
}

static bool match(Parser *p, TokenType expected) {
    if(current(p).type != expected) {
        return false;
    }
    advance(p);
    return true;
}

/* Helper macros */

#define TRY(type, result) ({ \
 type _tmp = (result);\
 if(!_tmp) { \
    return NULL; \
 } \
_tmp;})

#define TRY_CONSUME(parser, expected) TRY(bool, consume(parser, expected))

/** Expression parser ***/

typedef enum precedence {
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

typedef ASTNode *(*ParseFn)(Parser *p);

typedef struct parse_rule {
    Precedence precedence;
    ParseFn prefix, infix;
} ParseRule;

/* Parse functions forward-declarations */
static ASTNode *parse_number_literal(Parser *p);

static ParseRule rules[] = {
    [TK_LPAREN]      = {PREC_LOWEST, NULL, NULL},
    [TK_RPAREN]      = {PREC_LOWEST, NULL, NULL},
    [TK_LBRACE]      = {PREC_LOWEST, NULL, NULL},
    [TK_RBRACE]      = {PREC_LOWEST, NULL, NULL},
    [TK_PLUS]        = {PREC_LOWEST, NULL, NULL},
    [TK_STAR]        = {PREC_LOWEST, NULL, NULL},
    [TK_SLASH]       = {PREC_LOWEST, NULL, NULL},
    [TK_SEMICOLON]   = {PREC_LOWEST, NULL, NULL},
    [TK_COLON]       = {PREC_LOWEST, NULL, NULL},
    [TK_MINUS]       = {PREC_LOWEST, NULL, NULL},
    [TK_ARROW]       = {PREC_LOWEST, NULL, NULL},
    [TK_EQUAL]       = {PREC_LOWEST, NULL, NULL},
    [TK_EQUAL_EQUAL] = {PREC_LOWEST, NULL, NULL},
    [TK_BANG]        = {PREC_LOWEST, NULL, NULL},
    [TK_BANG_EQUAL]  = {PREC_LOWEST, NULL, NULL},
    [TK_NUMBER]      = {PREC_LOWEST, parse_number_literal, NULL},
    [TK_IF]          = {PREC_LOWEST, NULL, NULL},
    [TK_ELSE]        = {PREC_LOWEST, NULL, NULL},
    [TK_WHILE]       = {PREC_LOWEST, NULL, NULL},
    [TK_FN]          = {PREC_LOWEST, NULL, NULL},
    [TK_RETURN]      = {PREC_LOWEST, NULL, NULL},
    [TK_I32]         = {PREC_LOWEST, NULL, NULL},
    [TK_VAR]         = {PREC_LOWEST, NULL, NULL},
    [TK_U32]         = {PREC_LOWEST, NULL, NULL},
    [TK_IDENTIFIER]  = {PREC_LOWEST, NULL, NULL},
    [TK_GARBAGE]     = {PREC_LOWEST, NULL, NULL},
    [TK_EOF]         = {PREC_LOWEST, NULL, NULL}
};

static ParseRule *get_rule(TokenType type) {
    return &rules[type];
}

static ASTString parse_identifier(Parser *p) {
    TRY_CONSUME(p, TK_IDENTIFIER);
    return astProgramAddString(p->program, stringNCopy(previous(p).lexeme, previous(p).length));
}

static ASTNode *parse_number_literal(Parser *p) {
    // TODO: Support hex, octal & binary.
    u64 value = strtoul(previous(p).lexeme, NULL, 10);
    return astNewLiteralValueNode(ND_NUMBER_LITERAL, previous(p).location, LITERAL_VALUE(LIT_NUMBER, number, value));
}

static ASTNode *parse_precedence(Parser *p, Precedence min_prec) {
    advance(p);
    ParseFn prefix = get_rule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, stringFormat("Expected an expression but got '%s'.", tokenTypeString(previous(p).type)));
        return NULL;
    }
    ASTNode *lhs = prefix(p);
    if(!lhs) {
        // Assume the error was already reported.
        return NULL;
    }
    while(!is_eof(p) && min_prec < get_rule(current(p).type)->precedence) {
        advance(p);
        ParseFn infix = get_rule(previous(p).type)->infix;
        lhs = infix(p);
        if(!lhs) {
            // Assume the error was already reported.
            return NULL;
        }
    }

    return lhs;
}

static inline ASTNode *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_LOWEST);
}

// variable_decl -> 'var' identifier (':' typename)? ('=' expression)? ';'
static ASTNode *parse_variable_decl(Parser *p, Array *obj_array) {
    // Assumes 'var' was already consumed.
    Location var_loc = previous(p).location;

    ASTString name = TRY(ASTString, parse_identifier(p));
    Location name_loc = previous(p).location;

    // TODO: type.

    ASTNode *initializer = NULL;
    if(match(p, TK_EQUAL)) {
        initializer = TRY(ASTNode *, parse_expression(p));
    }

    // FIXME: free 'initializer' on fail (if needed).
    TRY_CONSUME(p, TK_SEMICOLON);

    ASTObj *var = astNewObj(OBJ_VAR, locationMerge(var_loc, name_loc));
    var->as.var.name = name;

    // Add the variable object.
    arrayPush(obj_array, (void *)var);

    // includes everything from the 'var' to the ';'.
    Location full_loc = locationMerge(var_loc, previous(p).location);
    ASTNode *var_node = astNewObjNode(ND_VARIABLE, locationMerge(var_loc, name_loc), var);
    if(initializer != NULL) {
        return astNewBinaryNode(ND_ASSIGN, full_loc, var_node, initializer);
    }
    return var_node;
}

#undef TRY_CONSUME
#undef TRY

// synchronize to declaration boundaries.
// TODO: if inside a function, synchronize to statement boundaries.
static void synchronize(Parser *p) {
    while(!is_eof(p)) {
        switch(current(p).type) {
            //case TK_FN:
            case TK_VAR:
            //case TK_IMPORT:
                return;
            default:
                advance(p);
                break;
        }
    }
}

bool parserParse(Parser *p, ASTProgram *prog) {
    p->program = prog;

    // prime the parser
    advance(p);
    // if the scanner failed to set the source, we can't do anything.
    if(p->scanner->failed_to_set_source) {
        // The scanner already reported the error.
        p->program = NULL;
        return false;
    }

    // Create the root module.
    ASTModule *root_module = astModuleNew(astProgramAddString(prog, "___root___"));
    astProgramAddModule(prog, root_module);

    while(!is_eof(p)) {
        if(match(p, TK_VAR)) {
            ASTNode *global = parse_variable_decl(p, &root_module->objects);
            if(global != NULL) {
                arrayPush(&root_module->globals, (void *)global);
            } else {
                synchronize(p);
            }
        } else {
            error_at(p, current(p).location, stringFormat("Expected one of ['var'], but got '%s'.", tokenTypeString(current(p).type)));
            synchronize(p);
        }
    }

    p->program = NULL;
    return !p->had_error;
}
