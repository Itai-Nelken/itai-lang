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
    p->current_module = 0;
    // set current and previous tokens to TK_GARBAGE with empty locations
    // so errors can be reported using them.
    p->previous_token.type = TK_GARBAGE;
    p->previous_token.location = locationNew(0, 0, 0);
    p->current_token.type = TK_GARBAGE;
    p->current_token.location = locationNew(0, 0, 0);
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

#define TRY(type, result, on_null) ({ \
 type _tmp = (result);\
 if(!_tmp) { \
    astNodeFree((on_null)); \
    return NULL; \
 } \
_tmp;})

#define TRY_CONSUME(parser, expected, on_null) TRY(bool, consume(parser, expected), on_null)

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

typedef ASTNode *(*PrefixParseFn)(Parser *p);
typedef ASTNode *(*InfixParseFn)(Parser *p, ASTNode *lhs);

typedef struct parse_rule {
    Precedence precedence;
    PrefixParseFn prefix;
    InfixParseFn infix;
} ParseRule;

/* Parse functions forward-declarations */
static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_number_literal(Parser *p);
static ASTNode *parse_grouping_expr(Parser *p);
static ASTNode *parse_identifier_expr(Parser *p);
static ASTNode *parse_term_expr(Parser *p, ASTNode *lhs);

static ParseRule rules[] = {
    [TK_LPAREN]      = {PREC_LOWEST, parse_grouping_expr, NULL},
    [TK_RPAREN]      = {PREC_LOWEST, NULL, NULL},
    [TK_LBRACE]      = {PREC_LOWEST, NULL, NULL},
    [TK_RBRACE]      = {PREC_LOWEST, NULL, NULL},
    [TK_PLUS]        = {PREC_TERM,   NULL, parse_term_expr},
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
    [TK_IDENTIFIER]  = {PREC_LOWEST, parse_identifier_expr, NULL},
    [TK_GARBAGE]     = {PREC_LOWEST, NULL, NULL},
    [TK_EOF]         = {PREC_LOWEST, NULL, NULL}
};

static ParseRule *get_rule(TokenType type) {
    return &rules[type];
}

static ASTString parse_identifier(Parser *p) {
    TRY_CONSUME(p, TK_IDENTIFIER, 0);
    return astProgramAddString(p->program, stringNCopy(previous(p).lexeme, previous(p).length));
}

static ASTNode *parse_identifier_expr(Parser *p) {
    ASTString str = astProgramAddString(p->program, stringNCopy(previous(p).lexeme, previous(p).length));
    return astNewIdentifierNode(previous(p).location, str);
}

static ASTNode *parse_number_literal(Parser *p) {
    // TODO: Support hex, octal & binary.
    u64 value = strtoul(previous(p).lexeme, NULL, 10);
    return astNewLiteralValueNode(ND_NUMBER_LITERAL, previous(p).location, LITERAL_VALUE(LIT_NUMBER, number, value));
}

static ASTNode *parse_grouping_expr(Parser *p) {
    ASTNode *expr = TRY(ASTNode *, parse_expression(p), 0);
    TRY_CONSUME(p, TK_RPAREN, expr);
    return expr;
}

static ASTNode *parse_term_expr(Parser *p, ASTNode *lhs) {
    TokenType op = previous(p).type;
    ASTNode *rhs = TRY(ASTNode *, parse_expression(p), lhs);

    ASTNodeType node_type;
    switch(op) {
        case TK_PLUS: node_type = ND_ADD; break;
        default: UNREACHABLE();
    }
    return astNewBinaryNode(node_type, locationMerge(lhs->location, rhs->location), lhs, rhs);
}

static ASTNode *parse_precedence(Parser *p, Precedence min_prec) {
    advance(p);
    PrefixParseFn prefix = get_rule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, stringFormat("Expected an expression but got '%s'.", tokenTypeString(previous(p).type)));
        return NULL;
    }
    ASTNode *tree = prefix(p);
    if(!tree) {
        // Assume the error was already reported.
        return NULL;
    }
    while(!is_eof(p) && min_prec < get_rule(current(p).type)->precedence) {
        advance(p);
        InfixParseFn infix = get_rule(previous(p).type)->infix;
        tree = infix(p, tree);
        if(!tree) {
            // Assume the error was already reported.
            return NULL;
        }
    }

    return tree;
}

static inline ASTNode *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_LOWEST);
}

// primitive_type -> i32 | u32
static Type *parse_primitive_type(Parser *p) {
    if(match(p, TK_I32)) {
        return p->program->primitives.int32;
    } else if(match(p, TK_U32)) {
        return p->program->primitives.uint32;
    }
    return NULL;
}

// complex_type -> structs, enums, functions, custom types
static Type *parse_complex_type(Parser *p) {
    error_at(p, current(p).location, "Expected typename.");
    return NULL;
}

// type -> primitive_type | complex_type
static Type *parse_type(Parser *p) {
    Type *ty = parse_primitive_type(p);
    if(!ty) {
        ty = TRY(Type *, parse_complex_type(p), 0);
    }
    return ty;
}

// variable_decl -> 'var' identifier (':' type)? ('=' expression)? ';'
static ASTNode *parse_variable_decl(Parser *p, Array *obj_array) {
    // Assumes 'var' was already consumed.
    Location var_loc = previous(p).location;

    ASTString name = TRY(ASTString, parse_identifier(p), 0);
    Location name_loc = previous(p).location;

    Type *type = NULL;
    if(match(p, TK_COLON)) {
        type = TRY(Type *, parse_type(p), 0);
    }

    ASTNode *initializer = NULL;
    if(match(p, TK_EQUAL)) {
        initializer = TRY(ASTNode *, parse_expression(p), 0);
    }

    TRY_CONSUME(p, TK_SEMICOLON, initializer);

    ASTObj *var = astNewObj(OBJ_VAR, locationMerge(var_loc, name_loc));
    var->as.var.name = name;
    var->as.var.type = type;

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
            //case TK_MODULE:
            //case TK_PUBLIC:
                return;
            default:
                advance(p);
                break;
        }
    }
}

static void init_primitive_types(ASTProgram *prog, ASTModule *root_module) {
#define DEF(typename, type, name, size) {Type *ty; NEW0(ty); typeInit(ty, (type), astProgramAddString(prog, (name)), (size)); prog->primitives.typename = astModuleAddType(root_module, ty);}

    DEF(int32, TY_I32, "i32", 4);
    DEF(uint32, TY_U32, "u32", 4);

#undef DEF
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
    ASTModule *root_module = astModuleNew(astProgramAddString(prog, "___root_module___"));
    p->current_module = astProgramAddModule(prog, root_module);
    init_primitive_types(prog, root_module);

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
