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
    p->program = NULL;
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

// if a valid String is provided as messages, it will be freed.
static void add_error(Parser *p, bool has_location, Location loc, char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, has_location, loc, message);
    compilerAddError(p->compiler, err);
    if(stringIsValid(message)) {
        stringFree(message);
    }
}

// if a valid String is provided as message, it will be freed.
static inline void error_at(Parser *p, Location loc, char *message) {
    add_error(p, true, loc, message);
}

// if 'message' is a valid String, it will be freed.
// uses the previous tokens location.
static inline void error(Parser *p, char *message) {
    error_at(p, previous(p).location, message);
}

static bool consume(Parser *p, TokenType expected) {
    if(peek(p).type != expected) {
        error_at(p, peek(p).location, stringFormat("Expected '%s' but got '%s'!", tokenTypeString(expected), tokenTypeString(peek(p).type)));
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

typedef ASTNode *(*PrefixParseFn)(Parser *p, bool can_assign);
typedef ASTNode *(*InfixParseFn)(Parser *p, ASTNode *left);

typedef struct parse_rule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;

// forward-declarations for parse functions and rule table
static ASTNode *parse_precedence(Parser *p, Precedence min_prec);
static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_prefix_expression(Parser *p, bool can_assign);
static ASTNode *parse_infix_expression(Parser *p, ASTNode *lhs);
static ASTNode *parse_call_expression(Parser *p, ASTNode *callee);
static ASTNode *parse_statement(Parser *p);
static ASTIdentifier *parse_identifier_from_token(Parser *p, Token tk);

static ParseRule rules[] = {
    //   token     |    prefix                 | infix | precedence
    [TK_LPAREN]      = {parse_prefix_expression, parse_call_expression, PREC_CALL},
    [TK_RPAREN]      = {NULL, NULL, PREC_LOWEST},
    [TK_PLUS]        = {parse_prefix_expression, parse_infix_expression, PREC_TERM},
    [TK_STAR]        = {NULL, parse_infix_expression, PREC_FACTOR},
    [TK_SLASH]       = {NULL, parse_infix_expression, PREC_FACTOR},
    [TK_SEMICOLON]   = {NULL, NULL, PREC_LOWEST},
    [TK_MINUS]       = {parse_prefix_expression, parse_infix_expression, PREC_TERM},
    [TK_ARROW]       = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL]       = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL_EQUAL] = {NULL, parse_infix_expression, PREC_EQUALITY},
    [TK_BANG]        = {NULL, NULL, PREC_LOWEST},
    [TK_BANG_EQUAL]  = {NULL, parse_infix_expression, PREC_EQUALITY},
    [TK_NUMBER]      = {parse_prefix_expression, NULL, PREC_LOWEST},
    [TK_IF]          = {NULL, NULL, PREC_LOWEST},
    [TK_ELSE]        = {NULL, NULL, PREC_LOWEST},
    [TK_WHILE]       = {NULL, NULL, PREC_LOWEST},
    [TK_FN]          = {NULL, NULL, PREC_LOWEST},
    [TK_RETURN]      = {NULL, NULL, PREC_LOWEST},
    [TK_I32]         = {NULL, NULL, PREC_LOWEST},
    [TK_IDENTIFIER]  = {parse_prefix_expression, NULL, PREC_LOWEST},
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

// infix operators.
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
        default:
            UNREACHABLE();
    }
}

static ASTNode *parse_prefix_expression(Parser *p, bool can_assign) {
    switch(previous(p).type) {
        case TK_NUMBER:
            return astNewNumberNode(previous(p).location, previous(p).as.number_constant);
        case TK_IDENTIFIER: {
            ASTIdentifier *id = parse_identifier_from_token(p, previous(p));
            if(!id) {
                return NULL;
            }
            ASTNode *node = astNewIdentifierNode(EMPTY_SYMBOL_ID, id);
            if(can_assign && match(p, TK_EQUAL)) {
                ASTNode *rvalue = TRY_PARSE(parse_expression, p, node);
                node = astNewBinaryNode(ND_ASSIGN, locationMerge(node->location, rvalue->location), node, rvalue);
            }
            return node;
        }
        case TK_PLUS:
        case TK_MINUS: {
            TokenType operator = previous(p).type;
            Location operator_loc = previous(p).location;
            ASTNode *operand = TRY_PARSE_PREC(p, PREC_UNARY, 0);
            if(operator == TK_PLUS) {
                operand->location = locationMerge(operator_loc, operand->location);
                return operand;
            }
            return astNewUnaryNode(ND_NEG, locationMerge(operator_loc, operand->location), operand);
        }
        case TK_LPAREN: {
            ASTNode *expr = TRY_PARSE_PREC(p, PREC_LOWEST, 0);
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
    return astNewBinaryNode(type, locationMerge(expr_loc, rhs->location), lhs, rhs);
}

static ASTNode *parse_call_expression(Parser *p, ASTNode *callee) {
    Location loc = locationMerge(callee->location, previous(p).location);
    // TODO: generic arguments (parseedin parse_prefix_expression => TK_IDENTIFIER?), arguments
    CONSUME(p, TK_RPAREN, callee);
    return astNewUnaryNode(ND_CALL, locationMerge(loc, previous(p).location), callee);
}

static ASTNode *parse_precedence(Parser *p, Precedence min_prec) {
    advance(p);
    PrefixParseFn nud = get_nud(previous(p).type);
    if(!nud) {
        error(p, stringFormat("Expected an expression but got '%s'!", tokenTypeString(previous(p).type)));
        return NULL;
    }
    bool can_assign = min_prec <= PREC_ASSIGNMENT;
    ASTNode *lhs = nud(p, can_assign);
    if(!lhs) {
        return NULL;
    }
    while(!is_eof(p) && min_prec < get_precedence(peek(p).type)) {
        advance(p);
        InfixParseFn led = get_led(previous(p).type);
        lhs = led(p, lhs);
        if(!lhs) {
            break;
        }
    }

    if(can_assign && peek(p).type == TK_EQUAL) {
        error(p, "Invalid assignment target!");
        advance(p);
    }

    return lhs;
}

static inline ASTNode *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_LOWEST);
}

/* helpers */

static ASTIdentifier *parse_identifier_from_token(Parser *p, Token tk) {
    if(tk.type != TK_IDENTIFIER) {
        error_at(p, tk.location, stringFormat("Expected an identifier but got '%s'!", tokenTypeString(tk.type)));
        return NULL;
    }
    SymbolID id_idx = symbolTableAddIdentifier(&p->program->symbols, tk.as.identifier.text, tk.as.identifier.length);
    return astNewIdentifier(tk.location, id_idx);
}

static ASTIdentifier *parse_identifier(Parser *p) {
    CONSUME(p, TK_IDENTIFIER, 0);
    return parse_identifier_from_token(p, previous(p));
}

#define TRY_PARSE_ID(parser) ({ASTIdentifier *_tmp_res = parse_identifier((parser)); if(!_tmp_res) { return NULL; } _tmp_res;})

/* type parser */

static SymbolID parse_typename(Parser *p) {
    if(match(p, TK_I32)) {
        return astProgramGetPrimitiveType(p->program, TY_I32);
    }
    error_at(p, peek(p).location, stringFormat("Expected a typename but got '%s'!", tokenTypeString(peek(p).type)));
    return EMPTY_SYMBOL_ID;
}

static SymbolID parse_type(Parser *p) {
    // TODO: advanced types ([T], T? etc.)
    // FIXME: handle EMPTY_SYMBOL_ID returned from parse_typename.
    return parse_typename(p);
}

/* statements */

static ASTNode *parse_block_construct(Parser *p, ASTNode *(*parse_fn_callback)(Parser *)) {
    // assumes '{' was already consumed.
    Array body;
    arrayInit(&body);
    ASTListNode *n = AS_LIST_NODE(astNewListNode(ND_BLOCK, previous(p).location, body));
    Location location = previous(p).location;
    while(!is_eof(p) && peek(p).type != TK_RBRACE) {
        ASTNode *node = TRY_PARSE(parse_fn_callback, p, AS_NODE(n));
        location = locationMerge(location, node->location);
        arrayPush(&n->body, (void *)node);
    }
    if(peek(p).type != TK_RBRACE) {
        error(p, stringFormat("Expected '}' after block but got '%s'.", tokenTypeString(peek(p).type)));
        astFree(AS_NODE(n));
        return NULL;
    }
    advance(p); // consume the '}'.
    n->header.location = location;
    return AS_NODE(n);
}

// block -> '{' statement* '}'
static inline ASTNode *parse_block(Parser *p) {
    // assumes '{' was already consumed.
    return parse_block_construct(p, parse_statement);
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

// while_stmt -> 'while' expression block
static ASTNode *parse_while_stmt(Parser *p) {
    // assumes 'while' was already consumed.
    Location while_loc = previous(p).location;
    ASTNode *condition = TRY_PARSE(parse_expression, p, 0);
    CONSUME(p, TK_LBRACE, condition);
    ASTNode *body = TRY_PARSE(parse_block, p, condition);

    return astNewLoopNode(ND_LOOP, locationMerge(while_loc, locationMerge(condition->location, body->location)), NULL, condition, NULL, AS_LIST_NODE(body));
}

// return_stmt -> 'return' expression? ';'
static ASTNode *parse_return_stmt(Parser *p) {
    // assumes 'return' was already consumed.
    Location loc = previous(p).location;
    ASTNode *operand = NULL;
    if(peek(p).type != TK_SEMICOLON) {
        operand = TRY_PARSE(parse_expression, p, 0);
        loc = locationMerge(loc, operand->location);
    }
    CONSUME(p, TK_SEMICOLON, operand);
    return astNewUnaryNode(ND_RETURN, loc, operand);
}

// expr_stmt -> expression ';'
static ASTNode *parse_expr_stmt(Parser *p) {
    ASTNode *expr = TRY_PARSE(parse_expression, p, 0);
    CONSUME(p, TK_SEMICOLON, expr);
    return astNewUnaryNode(ND_EXPR_STMT, locationMerge(expr->location, previous(p).location), expr);
}

// statement -> if_stmt
//            | while_stmt
//            | return_stmt
//            | expr_stmt
static ASTNode *parse_statement(Parser *p) {
    ASTNode *result = NULL;
    if(match(p, TK_IF)) {
        result = parse_if_stmt(p);
    } else if(match(p, TK_WHILE)) {
        result = parse_while_stmt(p);
    } else if(match(p, TK_RETURN)) {
        result = parse_return_stmt(p);
    } else {
        result = parse_expr_stmt(p);
    }
    return result;
}

/* declarations */

// fn_decl -> 'fn' IDENTIFIER generic_parameters? '(' parameter_list? ')' ('->' type)? block
static ASTFunctionObj *parse_function_decl(Parser *p) {
    // assumes 'fn' was already consumed.
    Location location = previous(p).location;
    ASTIdentifier *name = TRY_PARSE_ID(p);
    location = locationMerge(location, name->location);
    // TODO: generic parameters

    CONSUME(p, TK_LPAREN, 0);
    location = locationMerge(location, previous(p).location);
    // TODO: parameters
    CONSUME(p, TK_RPAREN, 0);
    location = locationMerge(location, previous(p).location);

    // return type
    SymbolID return_type = astProgramGetPrimitiveType(p->program, TY_VOID);
    if(match(p, TK_ARROW)) {
        return_type = parse_type(p);
    }

    CONSUME(p, TK_LBRACE, 0);
    ASTListNode *body = AS_LIST_NODE(TRY_PARSE(parse_block, p, 0));

    return AS_FUNCTION_OBJ(astNewFunctionObj(locationMerge(location, body->header.location), name, return_type, body));
}

static ASTVariableObj *parse_variable_decl(Parser *p) {
    // assumes 'var' was already consumed.
    Location location = previous(p).location;

    // name
    ASTIdentifier *name = TRY_PARSE_ID(p);

    // type
    SymbolID type = EMPTY_SYMBOL_ID;
    if(match(p, TK_COLON)) {
        type = parse_type(p);
    }

    // initializer
    ASTNode *initializer = NULL;
    if(match(p, TK_EQUAL)) {
        initializer = parse_expression(p);
    }

    // end
    if(!consume(p, TK_SEMICOLON)) {
        astFree(initializer);
        astFreeIdentifier(name);
        return NULL;
    }

    // TODO: add local variables.
    return AS_VARIABLE_OBJ(astNewVariableObj(OBJ_GLOBAL, locationMerge(location, previous(p).location), name, type, initializer));
}

#undef TRY_PARSE_ID
#undef CONSUME
#undef TRY_PARSE_PREC
#undef TRY_PARSE

// synchronize to declaration boundaries.
// TODO: if inside a function, synchronize to statement boundaries.
static void synchronize(Parser *p) {
    while(!is_eof(p)) {
        switch(peek(p).type) {
            case TK_FN:
            case TK_VAR:
            //case TK_IMPORT:
                return;
            default:
                advance(p);
                break;
        }
    }
}

bool parserParse(Parser *p, Scanner *s, ASTProgram *prog) {
    p->scanner = s;
    p->program = prog;

    // prime the parser
    advance(p);
    // if the scanner failed to set the source, we can't do anything.
    if(p->scanner->failed_to_set_source) {
        // The scanner already reported the error.
        p->program = NULL;
        p->scanner = NULL;
        return false;
    }

    // create the root module
    SymbolID root_module_name_id = symbolTableAddIdentifier(&prog->symbols, "___root___", 10);
    prog->root_module = astProgramAddModule(prog, astNewModule(astNewIdentifier(locationNew(0, 0, compilerGetCurrentFileID(p->compiler)), root_module_name_id)));

    // get the root module as the initial module
    ASTModule *current_module = astProgramGetModule(prog, prog->root_module);
    if(!current_module) {
        add_error(p, false, locationNew(0, 0, 0), "Failed to get root module!");
        p->program = NULL;
        p->scanner = NULL;
        return false;
    }

    bool had_error = false;
    while(!is_eof(p)) {
        if(match(p, TK_FN)) {
            ASTFunctionObj *fn = parse_function_decl(p);
            if(fn) {
                arrayPush(&current_module->objects, (void *)fn);
            } else {
                had_error = true;
                synchronize(p);
            }
        } else if(match(p, TK_VAR)) {
            ASTVariableObj *var = parse_variable_decl(p);
            if(var) {
                arrayPush(&current_module->objects, (void *)var);
            } else {
                had_error = true;
                synchronize(p);
            }
        } else {
            had_error = true;
            error_at(p, peek(p).location, stringFormat("Expected one of ['fn', 'var'], but got '%s'.", tokenTypeString(peek(p).type)));
            // advance and synchronize so we don't get stuck in an infinite loop on the same token.
            advance(p);
            synchronize(p);
        }
    }

    p->program = NULL;
    p->scanner = NULL;
    return !had_error;
}
