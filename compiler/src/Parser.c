#include <stddef.h> // NULL
#include "Ast/Module.h"
#include "Ast/Object.h"
#include "Ast/StringTable.h"
#include "Ast/Type.h"
#include "common.h"
#include "Strings.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Error.h"
#include "Token.h"
#include "Ast/Ast.h"
#include "Parser.h"

/***
 * Parser rules:
 * =============
 * - Allocate objects using ONLY astModuleNewObj().
 * - Allocate AST nodes using ONLY the allocator returned by getCurrentAllocator().
 * - Use the TRY() and TRY_CONSUME() macros as much as possible.
 * - Use the tmp_buffer functions for formatting temporary strings (such as for an error).
 ***/

static void parser_init_internal(Parser *p, Compiler *c, Scanner *s) {
    p->compiler = c;
    p->scanner = s;
    p->program = NULL;
    p->dumpTokens = false;
    p->current.module = 0;
    p->current.scope = NULL;
    p->state.current_token.type = TK_GARBAGE;
    p->state.previous_token.type = TK_GARBAGE;
    p->state.had_error = false;
    p->state.need_sync = false;
    p->state.idTypeCounter = 0;
    p->primitives.void_ = NULL;
    p->primitives.int32 = NULL;
    p->primitives.uint32 = NULL;
    p->primitives.str = NULL;
    p->primitives.boolean = NULL;
}
void parserInit(Parser *p, Compiler *c, Scanner *s) {
    parser_init_internal(p, c, s);
    p->tmp_buffer = stringNew(20); // Note: 20 is a random number that seems large enough for most short strings.
}

void parserFree(Parser *p) {
    if(p->tmp_buffer) { // This check is so stringFree() doesn't crash. there is no reason for the buffer to be NULL.
        stringFree(p->tmp_buffer);
        p->tmp_buffer = NULL;
    }
    parser_init_internal(p, NULL, NULL);
}

void parserSetDumpTokens(Parser *p, bool dumpTokens) {
    p->dumpTokens = dumpTokens;
}

/* Parser helper functions */

// if !expr, returns NULL. otherwise expands to said result.
#define TRY(type, expr) ({ \
        type _tmp = (expr); \
        if(!_tmp) { \
            return NULL; \
        } \
        _tmp;})

static bool consume(Parser *p, TokenType expected);
// if consume(p, expected) returns false, return NULL.
#define TRY_CONSUME(p, expected) TRY(bool, consume((p), (expected)))

static inline Token current(Parser *p) {
    return p->state.current_token;
}

static inline Token previous(Parser *p) {
    return p->state.previous_token;
}

static inline bool isEof(Parser *p) {
    return current(p).type == TK_EOF;
}

static Token advance(Parser *p) {
    // Skip TK_GARBAGE tokens.
    // The parser doesn't know how to handle them, and the scanner
    // has already reported errors for them anyway.
    Token tk;
    while((tk = scannerNextToken(p->scanner)).type == TK_GARBAGE) {
        if(p->dumpTokens) { tokenPrint(stdout, &tk); putchar('\n'); }
        p->state.had_error = true;
    }
    if(p->dumpTokens) { tokenPrint(stdout, &tk); putchar('\n'); }
    p->state.previous_token = p->state.current_token;
    p->state.current_token = tk;
    return tk;
}

static char *tmp_buffer_format(Parser *p, const char *format, ...) {
    if(stringLength(p->tmp_buffer) > 0) {
        stringClear(p->tmp_buffer);
    }
    va_list ap;
    va_start(ap, format);
    stringVAppend(&p->tmp_buffer, format, ap);
    va_end(ap);
    // The cast is unecessary, and used only to signify we don't want caller to use return value as a String.
    return (char *)p->tmp_buffer;
}

static char *tmp_buffer_append(Parser *p, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    stringVAppend(&p->tmp_buffer, format, ap);
    va_end(ap);
    return (char *)p->tmp_buffer; // See note above return in tmp_buffer_format().
}

static void add_error(Parser *p, bool has_location, Location loc, ErrorType type, const char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, type, has_location, loc, message);
    compilerAddError(p->compiler, err);
    p->state.had_error = true;
    p->state.need_sync = true;
}

static inline void errorAt(Parser *p, Location loc, const char *message) {
    add_error(p, true, loc, ERR_ERROR, message);
}

// Note: uses the previous token's location.
static inline void error(Parser *p, const char *message) {
    errorAt(p, previous(p).location, message);
}

static inline void hint(Parser *p, Location loc, const char *message) {
    add_error(p, true, loc, ERR_HINT, message);
}

static bool consume(Parser *p, TokenType expected) {
    if(current(p).type != expected) {
        errorAt(p, current(p).location, tmp_buffer_format(p, "Expected '%s' but got '%s'.", tokenTypeString(expected), tokenTypeString(current(p).type)));
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

static inline ASTModule *getCurrentModule(Parser *p) {
    return astProgramGetModule(p->program, p->current.module);
}

static inline Allocator *getCurrentAllocator(Parser *p) {
    return &getCurrentModule(p)->ast_allocator.alloc;
}

static inline Scope *getCurrentScope(Parser *p) {
    return p->current.scope;
}

// Note: depthType only for SCOPE_DEPTH_BLOCK and SCOPE_DEPTH_STRUCT
//       it represents the TYPE of the new scope, not the depth (so is it a block scope or a struct "scope"/namespace).
static Scope *enterScope(Parser *p, ScopeDepth depthType) {
    Scope *parent = getCurrentScope(p);
    ScopeDepth depth = depthType;
    switch(parent->depth) {
        case SCOPE_DEPTH_MODULE_NAMESPACE:
            // modules withing modules are not allowed, at least right now.
            VERIFY(depthType != SCOPE_DEPTH_MODULE_NAMESPACE);
            break;
        case SCOPE_DEPTH_STRUCT:
            // The only scope allowed as a child of a struct scope
            // is a block scope for a method,
            VERIFY(depthType >= SCOPE_DEPTH_BLOCK);
            depth = SCOPE_DEPTH_BLOCK;
            break;
        case SCOPE_DEPTH_BLOCK:
        default:
            VERIFY(depthType == SCOPE_DEPTH_BLOCK);
            depth = parent->depth + 1;
    }
    Scope *child = scopeNew(parent, depth);
    scopeAddChild(parent, child);
    p->current.scope = child;
    return child;
}

static void leaveScope(Parser *p) {
    p->current.scope = p->current.scope->parent;
}

/** Expression Parser **/

typedef enum precedence {
    PREC_LOWEST           = 0,  // lowest
    PREC_ASSIGNMENT       = 1,  // infix =
    PREC_BIT_OR           = 2,  // infix |
    PREC_BIT_XOR          = 3,  // infix ^
    PREC_BIT_AND          = 4,  // infix &
    PREC_EQUALITY         = 5,  // infix == !=
    PREC_COMPARISON       = 6,  // infix > >= < <=
    PREC_BIT_SHIFT        = 7,  // infix >> <<
    PREC_LOGIC_AND        = 8,  // infix &&
    PREC_LOGIC_OR         = 9,  // infix ||
    PREC_TERM             = 10, // infix + -
    PREC_FACTOR           = 1,  // infix * /
    PREC_UNARY            = 12, // unary + -
    PREC_CALL             = 13, // (), a.b
    PREC_SCOPE_RESOLUTION = 14, // A::B
    PREC_PRIMARY          = 15  // highest
} Precedence;

typedef ASTExprNode *(*PrefixParseFn)(Parser *p);
typedef ASTExprNode *(*InfixParseFn)(Parser *p, ASTExprNode *lhs);

typedef struct parse_rule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;

/* Expression parse functions pre-declarations (for parse table) */
static ASTExprNode *parseExpression(Parser *p);
static ASTExprNode *parsePrecedence(Parser *p, Precedence minPrec);
static ASTExprNode *parse_number_literal_expr(Parser *p);
static ASTExprNode *parse_string_literal_expr(Parser *p);
static ASTExprNode *parse_boolean_literal_expr(Parser *p);
static ASTExprNode *parse_binary_expr(Parser *p, ASTExprNode *lhs);
static ASTExprNode *parse_unary_expr(Parser *p);
static ASTExprNode *parse_grouping_expr(Parser *p);
static ASTExprNode *parse_call_expr(Parser *p, ASTExprNode *callee);
static ASTExprNode *parse_identifier_expr(Parser *p);
static ASTExprNode *parse_assignment_expr(Parser *p, ASTExprNode *lhs);
static ASTExprNode *parse_property_access_expr(Parser *p, ASTExprNode *lhs);

/* Precedence/parse rule table */

static ParseRule rules[] = {
    [TK_LPAREN]           = {parse_grouping_expr, parse_call_expr, PREC_CALL},
    [TK_RPAREN]           = {NULL, NULL, PREC_LOWEST},
    [TK_LBRACKET]         = {NULL, NULL, PREC_LOWEST},
    [TK_RBRACKET]         = {NULL, NULL, PREC_LOWEST},
    [TK_LBRACE]           = {NULL, NULL, PREC_LOWEST},
    [TK_RBRACE]           = {NULL, NULL, PREC_LOWEST},
    [TK_PLUS]             = {parse_unary_expr, parse_binary_expr, PREC_TERM},
    [TK_STAR]             = {parse_unary_expr, parse_binary_expr, PREC_FACTOR},
    [TK_SLASH]            = {NULL, parse_binary_expr, PREC_FACTOR},
    [TK_SEMICOLON]        = {NULL, NULL, PREC_LOWEST},
    [TK_COLON]            = {NULL, NULL, PREC_LOWEST},
    [TK_COMMA]            = {NULL, NULL, PREC_LOWEST},
    [TK_DOT]              = {NULL, parse_property_access_expr, PREC_CALL},
    [TK_HASH]             = {NULL, NULL, PREC_LOWEST},
    [TK_AMPERSAND]        = {parse_unary_expr, NULL, PREC_LOWEST},
    [TK_AND]              = {NULL, parse_binary_expr, PREC_LOGIC_AND},
    [TK_PIPE]             = {NULL, NULL, PREC_LOWEST},
    [TK_OR]               = {NULL, parse_binary_expr, PREC_LOGIC_OR},
    [TK_MINUS]            = {parse_unary_expr, parse_binary_expr, PREC_TERM},
    [TK_ARROW]            = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL]            = {NULL, parse_assignment_expr, PREC_ASSIGNMENT},
    [TK_EQUAL_EQUAL]      = {NULL, parse_binary_expr, PREC_EQUALITY},
    [TK_BANG]             = {parse_unary_expr, NULL, PREC_LOWEST},
    [TK_BANG_EQUAL]       = {NULL, parse_binary_expr, PREC_EQUALITY},
    [TK_LESS]             = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_LESS_EQUAL]       = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_GREATER]          = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_GREATER_EQUAL]    = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_SCOPE_RESOLUTION] = {NULL, NULL, PREC_LOWEST},
    [TK_NUMBER_LITERAL]   = {parse_number_literal_expr, NULL, PREC_LOWEST},
    [TK_STRING_LITERAL]   = {parse_string_literal_expr, NULL, PREC_LOWEST},
    [TK_TRUE]             = {parse_boolean_literal_expr, NULL, PREC_LOWEST},
    [TK_FALSE]            = {parse_boolean_literal_expr, NULL, PREC_LOWEST},
    [TK_IF]               = {NULL, NULL, PREC_LOWEST},
    [TK_ELSE]             = {NULL, NULL, PREC_LOWEST},
    [TK_WHILE]            = {NULL, NULL, PREC_LOWEST},
    [TK_FN]               = {NULL, NULL, PREC_LOWEST},
    [TK_RETURN]           = {NULL, NULL, PREC_LOWEST},
    [TK_VAR]              = {NULL, NULL, PREC_LOWEST},
    [TK_STRUCT]           = {NULL, NULL, PREC_LOWEST},
    [TK_EXTERN]           = {NULL, NULL, PREC_LOWEST},
    [TK_DEFER]            = {NULL, NULL, PREC_LOWEST},
    [TK_EXPECT]           = {NULL, NULL, PREC_LOWEST},
    [TK_IMPORT]           = {NULL, NULL, PREC_LOWEST},
    [TK_VOID]             = {NULL, NULL, PREC_LOWEST},
    [TK_I32]              = {NULL, NULL, PREC_LOWEST},
    [TK_U32]              = {NULL, NULL, PREC_LOWEST},
    [TK_STR]              = {NULL, NULL, PREC_LOWEST},
    [TK_BOOL]             = {NULL, NULL, PREC_LOWEST},
    [TK_IDENTIFIER]       = {parse_identifier_expr, NULL, PREC_LOWEST},
    [TK_FILE_CHANGED]     = {NULL, NULL, PREC_LOWEST},
    [TK_GARBAGE]          = {NULL, NULL, PREC_LOWEST},
    [TK_EOF]              = {NULL, NULL, PREC_LOWEST}
};

static inline ParseRule *getRule(TokenType type) {
    return &rules[(int)type];
}

/* Parse functions */

static ASTExprNode *parse_number_literal_expr(Parser *p) {
    Location loc = previous(p).location;
    // TODO: Support hex, octal, and binary literals
    u64 value = strtoul(previous(p).lexeme, NULL, 10);
    Type *postfixType = NULL;
    // Note: It isn't possible to use parseType() here as the type
    //       is optional and there isn't any way to know when there is
    //       one and when there isn't one (using parseType()) without emitting an error.
    switch(current(p).type) {
        // Valid postfix types (for number literals)
        case TK_I32:
            postfixType = p->primitives.int32;
            advance(p);
            // Have to do this here because if we merged a location with itself, fn would crash.
            loc = locationMerge(loc, previous(p).location);
            break;
        case TK_U32:
            postfixType = p->primitives.uint32;
            advance(p);
            // Have to do this here because if we merged a location with itself, fn would crash.
            loc = locationMerge(loc, previous(p).location);
            break;
        // Invalid postfix types
        case TK_VOID:
        case TK_STR:
        case TK_BOOL:
            // Consume the token anyway to suppress further errors because of it.
            advance(p);
            errorAt(p, previous(p).location, tmp_buffer_format(p, "Invalid postfix type '%s' (must be a numeric type.)", tokenTypeString(previous(p).type)));
            return NULL;
        default:
            break;
    }
    ASTConstantValueExpr *n = astConstantValueExprNew(getCurrentAllocator(p), EXPR_NUMBER_CONSTANT, loc, NULL);
    n->as.number = value;
    n->header.dataType = postfixType;
    return NODE_AS(ASTExprNode, n);
}

static ASTExprNode *parse_string_literal_expr(Parser *p) {
    Token tk = previous(p);
    // trim '"' from beginning and end of string.
    ASTString value = stringTableFormat(p->program->strings, "%.*s", tk.length-2, tk.lexeme+1);
    // TODO: add string type here since string literals will always be of type 'str'.
    ASTConstantValueExpr *n = astConstantValueExprNew(getCurrentAllocator(p), EXPR_STRING_CONSTANT, tk.location, NULL);
    n->as.string = value;
    return NODE_AS(ASTExprNode, n);
}

static ASTExprNode *parse_boolean_literal_expr(Parser *p) {
    Location loc = previous(p).location;
    bool value;
    switch(previous(p).type) {
        case TK_TRUE:
            value = true;
            break;
        case TK_FALSE:
            value = false;
            break;
        default:
            UNREACHABLE(); // unreachable b/c only called on TK_TRUE & TK_FALSE.
    }
    ASTConstantValueExpr *n = astConstantValueExprNew(getCurrentAllocator(p), EXPR_BOOLEAN_CONSTANT, loc, p->primitives.boolean);
    n->as.boolean = value;
    return NODE_AS(ASTExprNode, n);
}

static ASTExprNode *parse_binary_expr(Parser *p, ASTExprNode *lhs) {
    TokenType operator = previous(p).type;
    ParseRule *rule = getRule(operator);
    ASTExprNode *rhs = TRY(ASTExprNode *, parsePrecedence(p, rule->precedence));

    ASTExprType nodeType;
    switch(operator) {
        case TK_PLUS: nodeType = EXPR_ADD; break;
        case TK_MINUS: nodeType = EXPR_SUBTRACT; break;
        case TK_STAR: nodeType = EXPR_MULTIPLY; break;
        case TK_SLASH: nodeType = EXPR_DIVIDE; break;
        case TK_EQUAL_EQUAL: nodeType = EXPR_EQ; break;
        case TK_BANG_EQUAL: nodeType = EXPR_NE; break;
        case TK_LESS: nodeType = EXPR_LT; break;
        case TK_LESS_EQUAL: nodeType = EXPR_LE; break;
        case TK_GREATER: nodeType = EXPR_GT; break;
        case TK_GREATER_EQUAL: nodeType = EXPR_GE; break;
        case TK_AND: nodeType = EXPR_LOGICAL_AND; break;
        case TK_OR: nodeType = EXPR_LOGICAL_OR; break;
        default: UNREACHABLE();
    }
    return NODE_AS(ASTExprNode, astBinaryExprNew(getCurrentAllocator(p), nodeType, locationMerge(lhs->location, rhs->location), lhs->dataType, lhs, rhs));
}

static ASTExprNode *parse_unary_expr(Parser *p) {
    Token operator = previous(p);
    ASTExprNode *operand = TRY(ASTExprNode *, parsePrecedence(p, PREC_UNARY));

    ASTExprType nodeType;
    switch(operator.type) {
        case TK_PLUS: return operand; // +<expr> is the same as <expr>
        case TK_MINUS: nodeType = EXPR_NEGATE; break;
        case TK_AMPERSAND: nodeType = EXPR_ADDROF; break;
        case TK_STAR: nodeType = EXPR_DEREF; break;
        case TK_BANG: nodeType = EXPR_LOGICAL_NOT; break;
        default: UNREACHABLE();
    }
    return NODE_AS(ASTExprNode, astUnaryExprNew(getCurrentAllocator(p), nodeType, locationMerge(operator.location, operand->location), operand->dataType, operand));
}

static ASTExprNode *parse_grouping_expr(Parser *p) {
    ASTExprNode *expr = TRY(ASTExprNode *, parseExpression(p));
    TRY_CONSUME(p, TK_RPAREN);
    return expr;
}

static ASTExprNode *parse_call_expr(Parser *p, ASTExprNode *callee) {
    Array arguments;
    arrayInit(&arguments);
    if(current(p).type != TK_RPAREN) {
        do {
            ASTExprNode *arg = parseExpression(p);
            if(!arg) {
                continue;
            }
            arrayPush(&arguments, arg);
        } while(match(p, TK_COMMA));
    }
    if(!consume(p, TK_RPAREN)) {
        arrayFree(&arguments);
        return NULL;
    }
    ASTExprNode *callExpr = NODE_AS(ASTExprNode, astCallExprNew(getCurrentAllocator(p), locationMerge(callee->location, previous(p).location), NULL, callee, &arguments));
    arrayFree(&arguments);
    return callExpr;
}

static ASTExprNode *parse_identifier_expr(Parser *p) {
    Token prev = previous(p);
    Array path;
    arrayInit(&path);
    ASTString id = stringTableFormat(p->program->strings, "%.*s", prev.length, prev.lexeme);
    while(match(p, TK_SCOPE_RESOLUTION)) {
        arrayPush(&path, (void *)id);
        advance(p);
        prev = previous(p);
        id = stringTableFormat(p->program->strings, "%.*s", prev.length, prev.lexeme);
    }
    ASTExprNode *idExpr = NODE_AS(ASTExprNode, astIdentifierExprNew(getCurrentAllocator(p), prev.location, &path, id));
    arrayFree(&path);
    return idExpr;
}

static ASTExprNode *parse_property_access_expr(Parser *p, ASTExprNode *lhs) {
    TRY_CONSUME(p, TK_IDENTIFIER);
    ASTExprNode *rhs = TRY(ASTExprNode *, NODE_AS(ASTExprNode, parse_identifier_expr(p)));
    Location loc = locationMerge(lhs->location, previous(p).location);
    return NODE_AS(ASTExprNode, astBinaryExprNew(getCurrentAllocator(p), EXPR_PROPERTY_ACCESS, loc, NULL, lhs, rhs));
}

static ASTExprNode *parsePrecedence(Parser *p, Precedence minPrec) {
    advance(p);
    PrefixParseFn prefix = getRule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, tmp_buffer_format(p, "Expected an expression but got '%s'.", tokenTypeString(previous(p).type)));
        return NULL;
    }

    ASTExprNode *tree = TRY(ASTExprNode *, prefix(p)); // We assume the error was already reported.
    while(!isEof(p) && minPrec < getRule(current(p).type)->precedence) {
        advance(p);
        InfixParseFn infix = getRule(previous(p).type)->infix;
        tree = TRY(ASTExprNode *, infix(p, tree)); // Again, we assume the error was already reported.
    }
    return tree;
}

static ASTExprNode *parse_assignment_expr(Parser *p, ASTExprNode *lhs) {
    ASTExprNode *rhs = TRY(ASTExprNode *, parseExpression(p));
    return NODE_AS(ASTExprNode, astBinaryExprNew(getCurrentAllocator(p), EXPR_ASSIGN, locationMerge(lhs->location, rhs->location), NULL, lhs, rhs));
}

static inline ASTExprNode *parseExpression(Parser *p) {
    return parsePrecedence(p, PREC_LOWEST);
}

/** Statement and declaration parser **/
static Type *parseType(Parser *p);
static ASTVarDeclStmt *parseVarDecl(Parser *p, bool allowInitializer);
static ASTStmtNode *parseStatement(Parser *p);

static void synchronizeInBlock(Parser *p) {
    // synchronize to statement boundaries
    // (statements also include variable declarations in this context).
    while(!isEof(p) && current(p).type != TK_RBRACE) {
        TokenType c = current(p).type;
        if(c == TK_VAR || c == TK_RETURN || c == TK_IF || c == TK_WHILE || c == TK_DEFER) {
            break;
        } else {
            advance(p);
        }
    }
}

// block(parseCallback) -> '{' parseCallback* '}'
static ASTBlockStmt *parseBlockStmt(Parser *p, Scope *scope, ASTStmtNode *(*parseCallback)(Parser *p)) {
    // Assumes '{' was already consumed
    Location loc = previous(p).location;
    Array nodes; // Array<ASTStmtNode *>
    arrayInit(&nodes);
    bool hasUnreachableCode = false;
    while(!isEof(p) && current(p).type != TK_RBRACE) {
        ASTStmtNode *stmt = parseCallback(p);
        if(stmt) {
            // If we get a return in the toplevel scope,
            // any code under it is unreachable.
            // FIXME: this won't report anything if next stmt is invalid.
            if(NODE_IS(stmt, STMT_RETURN)) {
                hasUnreachableCode = true;
            } else if(hasUnreachableCode) {
                errorAt(p, stmt->location, "Unreachable code.");
                arrayFree(&nodes);
                return NULL;
            }
            arrayPush(&nodes, (void *)stmt);
        } else {
            synchronizeInBlock(p);
        }
    }
    if(!consume(p, TK_RBRACE)) {
        arrayFree(&nodes);
        return NULL;
    }

    loc = locationMerge(loc, previous(p).location);
    ASTBlockStmt *n = astBlockStmtNew(getCurrentAllocator(p), loc, scope, &nodes);
    arrayFree(&nodes);
    return n;
}

// function_body -> variable_decl | statement
static ASTStmtNode *parseFunctionBodyStatements(Parser *p) {
    ASTStmtNode *n = NULL;
    Scope *sc = getCurrentScope(p);
    if(match(p, TK_VAR)) {
        ASTVarDeclStmt *vdecl = parseVarDecl(p, true);
        if(vdecl) {
            if(scopeHasObject(sc, vdecl->variable->name)) {
                ASTObj *prevDecl = scopeGetAnyObject(sc, vdecl->variable->name);
                errorAt(p, vdecl->variable->location, tmp_buffer_format(p, "Redeclaration of symbol '%s'.", vdecl->variable->name));
                hint(p, prevDecl->location, "Previous declaration was here.");
            } else {
                scopeAddObject(sc, vdecl->variable);
            }
            n = NODE_AS(ASTStmtNode, vdecl);
        }
    } else {
        n = parseStatement(p);
    }
    return n;
}

static ASTStmtNode *parseExpressionStmt(Parser *p) {
    ASTExprNode *expr = TRY(ASTExprNode *, parseExpression(p));
    TRY_CONSUME(p, TK_SEMICOLON);
    return NODE_AS(ASTStmtNode, astExprStmtNew(getCurrentAllocator(p), STMT_EXPR, locationMerge(expr->location, previous(p).location), expr));
}

// if_stmt -> 'if' expression block(function_body) ('else' if_stmt | block(function_body))?
static ASTStmtNode *parseIfStmt(Parser *p) {
    // Assumes 'if' was already consumed.
    Location loc = previous(p).location;
    ASTExprNode *condition = TRY(ASTExprNode *, parseExpression(p));
    Scope *thenScope = enterScope(p, SCOPE_DEPTH_BLOCK);
    TRY_CONSUME(p, TK_LBRACE);
    ASTStmtNode *then = TRY(ASTStmtNode *, (ASTStmtNode *)parseBlockStmt(p, thenScope, parseFunctionBodyStatements));
    leaveScope(p);
    ASTStmtNode *else_ = NULL;
    if(match(p, TK_ELSE)) {
        if(match(p, TK_IF)) {
            else_ = TRY(ASTStmtNode *, parseIfStmt(p));
        } else {
            TRY_CONSUME(p, TK_LBRACE);
            Scope *elseScope = enterScope(p, SCOPE_DEPTH_BLOCK);
            else_ = TRY(ASTStmtNode *, (ASTStmtNode *)parseBlockStmt(p, elseScope, parseFunctionBodyStatements));
            leaveScope(p);
        }
    }
    loc = locationMerge(loc, previous(p).location);
    return NODE_AS(ASTStmtNode, astConditionalStmtNew(getCurrentAllocator(p), STMT_IF, loc, condition, then, else_));
}

// return_stmt -> 'return' expression ';'
static ASTStmtNode *parseReturnStmt(Parser *p) {
    // Assumes 'return' was already consumed.
    Location loc = previous(p).location;
    ASTExprNode *operand = NULL;
    if(current(p).type != TK_SEMICOLON) {
        operand = TRY(ASTExprNode *, parseExpression(p));
    }
    TRY_CONSUME(p, TK_SEMICOLON);
    return (ASTStmtNode *)astExprStmtNew(getCurrentAllocator(p), STMT_RETURN, locationMerge(loc, previous(p).location), operand);
}

// while_loop -> 'while' expression block(function_body)
static ASTStmtNode *parseWhileLoop(Parser *p) {
    // Assumes 'while' was already consumed.
    Location loc = previous(p).location;
    ASTExprNode *condition = TRY(ASTExprNode *, parseExpression(p));
    TRY_CONSUME(p, TK_LBRACE);
    Scope *bodyScope = enterScope(p, SCOPE_DEPTH_BLOCK);
    ASTBlockStmt *body = parseBlockStmt(p, bodyScope, parseFunctionBodyStatements);
    leaveScope(p);
    // Leave scope before failing (on error.)
    if(!body) {
        return NULL;
    }
    return NODE_AS(ASTStmtNode, astLoopStmtNew(getCurrentAllocator(p), locationMerge(loc, previous(p).location), NULL, condition, NULL, body));
}

// defer_operand -> block(expression_stmt) | expression_stmt
static ASTStmtNode *parseDeferOperand(Parser *p) {
    ASTStmtNode *result = NULL;
    if(match(p, TK_LBRACE)) {
        // Note: Because variable declarations are not allowed in defer blocks,
        //       a scope isn't needed. But it isn't possible to use the current scope
        //       because then it would be exited twice which isn't possible.
        //       Because of that, a new scope is entered altough it isn't needed.
        Scope *scope = enterScope(p, SCOPE_DEPTH_BLOCK);
        result = NODE_AS(ASTStmtNode, parseBlockStmt(p, scope, parseExpressionStmt));
        leaveScope(p);
        // No semicolon needed after a block.
    } else {
        // Semicolon is consumed by parseExpressionStmt().
        result = parseExpressionStmt(p);
    }
    return result;
}

// defer_stmt -> 'defer' defer_operand
static ASTStmtNode *parseDeferStmt(Parser *p) {
    // Assumes 'defer' was already consumed
    Location loc = previous(p).location;
    if(getCurrentScope(p)->depth > SCOPE_DEPTH_BLOCK) {
        errorAt(p, loc, "'defer' only allowed in function scope.");
        return NULL;
    }
    ASTStmtNode *operand = TRY(ASTStmtNode *, parseDeferOperand(p));
    return NODE_AS(ASTStmtNode, astDeferStmtNew(getCurrentAllocator(p), locationMerge(loc, previous(p).location), operand));
}

// expect_stmt -> 'expect' expression (';' | ('else' block(statement)))
static ASTStmtNode *parseExpectStmt(Parser *p) {
    // Assumes 'expect' was already consumed.
    Location loc = previous(p).location;
    ASTExprNode *condition = TRY(ASTExprNode *, parseExpression(p));
    loc = locationMerge(loc, previous(p).location);
    condition = NODE_AS(ASTExprNode, astUnaryExprNew(getCurrentAllocator(p), EXPR_LOGICAL_NOT, loc, p->primitives.boolean, condition));
    if(match(p, TK_SEMICOLON)) {
        return NODE_AS(ASTStmtNode, astConditionalStmtNew(getCurrentAllocator(p), STMT_EXPECT, locationMerge(loc, previous(p).location), condition, NULL, NULL));
    }
    ASTStmtNode *else_ = NULL;
    if(match(p, TK_ELSE)) {
        TRY_CONSUME(p, TK_LBRACE);
        Scope *sc = enterScope(p, SCOPE_DEPTH_BLOCK);
        else_ = NODE_AS(ASTStmtNode, parseBlockStmt(p, sc, parseFunctionBodyStatements));
        leaveScope(p);
        // Leave scope before returning on error.
        if(else_ == NULL) {
            return NULL;
        }
    } else {
        errorAt(p, current(p).location, tmp_buffer_format(p, "Expected 'else' but got '%s'.", tokenTypeString(current(p).type)));
        return NULL;
    }
    return NODE_AS(ASTStmtNode, astConditionalStmtNew(getCurrentAllocator(p), STMT_EXPECT, locationMerge(loc, previous(p).location), condition, else_, NULL));
}

// statement -> block(statement) | return_stmt | if_stmt | while_loop_stmt | defer_stmt | expression_stmt
static ASTStmtNode *parseStatement(Parser *p) {
    ASTStmtNode *result = NULL;
    if(match(p, TK_IF)) {
        result = parseIfStmt(p);
    } else if(match(p, TK_WHILE)) {
        result = parseWhileLoop(p);
    } else if(match(p, TK_RETURN)) {
        result = parseReturnStmt(p);
    } else if(match(p, TK_DEFER)) {
        result = parseDeferStmt(p);
    } else if(match(p, TK_EXPECT)) {
        result = parseExpectStmt(p);
    } else if(match(p, TK_LBRACE)) {
        Scope *sc = enterScope(p, SCOPE_DEPTH_BLOCK);
        result = NODE_AS(ASTStmtNode, parseBlockStmt(p, sc, parseFunctionBodyStatements));
        leaveScope(p);
    } else {
        result = parseExpressionStmt(p);
    }
    return result;
}

// identifier -> TK_IDENTIFIER
static ASTString parseIdentifier(Parser *p) {
    TRY_CONSUME(p, TK_IDENTIFIER);
    Token idTk = previous(p);
    return stringTableFormat(p->program->strings, "%.*s", idTk.length, idTk.lexeme);
}

// identifier_type -> identifier
static Type *parseIdentifierType(Parser *p) {
    // An identifier type is an unkown type that could be a struct or a type alias.
    ASTString ident = TRY(ASTString, parseIdentifier(p));
    Location loc = previous(p).location;
    ASTString name = stringTableFormat(p->program->strings, "%s%u", ident, p->state.idTypeCounter++);
    Type *ty = typeNew(TY_IDENTIFIER, name, loc, p->current.module);
    ty->as.id.actualName = ident;
    astModuleAddType(getCurrentModule(p), ty);
    return ty;
    // TODO: <thoughts>
    //        This function should only parse, not add to the module maybe?
    //        Think about TY_IDENTIFIER. Should it be TY_UNKNOWN maybe? might be clearer.
    //          * Then there would be TY_STRUCT and TY_ALIAS for the two possible options an id type can be.
}

// parameters: Array<Type *>
// C.R.E for returnType to be NULL.
static Type *makeFunctionTypeWithParameterTypes(Parser *p, Array parameterTypes, Type *returnType) {
    VERIFY(returnType);
    Type *ty = typeNew(TY_FUNCTION, "", EMPTY_LOCATION, p->current.module);
    ty->as.fn.returnType = returnType;
    tmp_buffer_format(p, "fn(");
    ARRAY_FOR(i, parameterTypes) {
        Type *paramType = ARRAY_GET_AS(Type *, &parameterTypes, i);
        tmp_buffer_append(p, "%s", paramType->name);
        if(i + 1 < arrayLength(&parameterTypes)) {
            tmp_buffer_append(p, ", ");
        }
        arrayPush(&ty->as.fn.parameterTypes, paramType);
    }
    char *name = tmp_buffer_append(p, ")->%s", returnType->name);
    ASTString typename = stringTableString(p->program->strings, name);
    ty->name = typename;
    Type *existingType = astModuleGetType(getCurrentModule(p), typename);
    if(existingType) {
        typeFree(ty);
        return existingType;
    }
    astModuleAddType(getCurrentModule(p), ty);
    return ty;
}

// parameters: Array<ASTObj *> (OBJ_VAR)
// C.R.E for returnType to be NULL.
static Type *makeFunctionType(Parser *p, Array parameters, Type *returnType) {
    Array parameterTypes;
    arrayInitSized(&parameterTypes, arrayLength(&parameters));
    ARRAY_FOR(i, parameters) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &parameters, i);
        VERIFY(param->dataType);
        arrayPush(&parameterTypes, (void *)param->dataType);
    }
    Type *ty = makeFunctionTypeWithParameterTypes(p, parameterTypes, returnType);
    arrayFree(&parameterTypes);
    return ty;
}

// fn_type -> 'fn' '(' type* ')' ('->' type)?
static Type *parseFunctionType(Parser *p) {
    // Assumes 'fn' was already consumed.
    TRY_CONSUME(p, TK_LPAREN);
    Array parameterTypes; // Array<Type *>
    arrayInit(&parameterTypes);
    bool hadError = false;
    if(current(p).type != TK_RPAREN) { // if there are any parameter types, parse them.
        do {
            Type *ty = parseType(p);
            if(ty) {
                arrayPush(&parameterTypes, (void *)ty);
            } else {
                hadError = true;
            }
        } while(match(p, TK_COMMA));
    }
    if(hadError || !consume(p, TK_RPAREN)) {
        // Note: the parsed types were already added to the type table, so we don't have to free them.
        arrayFree(&parameterTypes);
        return NULL;
    }

    Type *returnType = astModuleGetType(getCurrentModule(p), "void"); // default return type is void.
    if(match(p, TK_ARROW)) {
        Type *parsedReturnType = parseType(p);
        if(!parsedReturnType) {
            arrayFree(&parameterTypes);
            return NULL;
        }
        returnType = parsedReturnType;
    }

    Type *ty = makeFunctionTypeWithParameterTypes(p, parameterTypes, returnType);
    arrayFree(&parameterTypes);
    return ty;
}

// complex_type -> fn_type | identifier_type (:: identifier_type)*
static Type *parseComplexType(Parser *p) {
    if(match(p, TK_FN)) {
        return parseFunctionType(p);
    } else if(current(p).type == TK_IDENTIFIER) {
        Type *ty = TRY(Type *, parseIdentifierType(p));
        Type *scResTy = NULL, *prev = ty;
        while(match(p, TK_SCOPE_RESOLUTION)) {
            if(scResTy == NULL) {
                scResTy = typeNew(TY_SCOPE_RESOLUTION, stringTableString(p->program->strings, ty->as.id.actualName), ty->declLocation, p->current.module);
                astModuleAddType(getCurrentModule(p), scResTy);
            }
            arrayPush(&scResTy->as.scopeResolution.path, (void *)prev);
            ty = TRY(Type *, parseIdentifierType(p));
            ASTString newName = stringTableFormat(p->program->strings, "%s::%s", prev->as.id.actualName, ty->as.id.actualName);
            scResTy->name = newName;
            prev = ty;
            // Note: parseIdentifierType() adds the path identifiers to the type owner in the module.
        }
        if(scResTy != NULL) {
            scResTy->as.scopeResolution.ty = ty;
            return scResTy;
        }
        return ty;
    }
    errorAt(p, current(p).location, "Expected typename.");
    return NULL;
}

// primitive_type -> void | i32 | u32 | str | bool
static Type *parsePrimitiveType(Parser *p) {
    Type *ty = NULL;
    switch(current(p).type) {
        case TK_VOID:
            ty = p->primitives.void_;
            break;
        case TK_I32:
            ty = p->primitives.int32;
            break;
        case TK_U32:
            ty = p->primitives.uint32;
            break;
        case TK_STR:
            ty = p->primitives.str;
            break;
        case TK_BOOL:
            ty = p->primitives.boolean;
            break;
        default:
            break;
    }
    if(ty) { // Note: to be able to use a switch statement, we can't use match().
        advance(p);
    }
    return ty;
}

// type -> '&'? (primitive_type | complex_type)
static Type *parseType(Parser *p) {
    bool isPointer = false;
    if(match(p, TK_AMPERSAND)) {
        isPointer = true;
    }
    Type *ty = parsePrimitiveType(p);
    if(!ty) {
        ty = TRY(Type *, parseComplexType(p));
    }
    if(isPointer) {
        // Note: using p.current.module because we need the ModuleID, not the module itself.
        Type *pointee = ty;
        ASTString ptrName = stringTableFormat(p->program->strings, "&%s", ty->type == TY_IDENTIFIER ? ty->as.id.actualName : ty->name);
        ty = typeNew(TY_POINTER, ptrName, EMPTY_LOCATION, p->current.module);
        ty->as.ptr.innerType = pointee;
        astModuleAddType(getCurrentModule(p), ty);
    }
    return ty;
}

// typed_var = identifier (':' type)?
static ASTObj *parseTypedVariable(Parser *p) {
    ASTString name = TRY(ASTString, parseIdentifier(p));
    Location loc = previous(p).location;
    Type *ty = NULL;
    if(match(p, TK_COLON)) {
        ty = TRY(Type *, parseType(p));
    }
    loc = locationMerge(loc, previous(p).location);
    return astModuleNewObj(getCurrentModule(p), OBJ_VAR, loc, name, ty);
}

// var_decl -> 'var' typed_var ('=' expression)+ ';'
static ASTVarDeclStmt *parseVarDecl(Parser *p, bool allowInitializer) {
    // Note: do NOT add to scope. we only parse!
    // Assumes 'var' has already been consumed.
    Location start = previous(p).location;
    ASTObj *obj = TRY(ASTObj *, parseTypedVariable(p));

    ASTExprNode *initializer = NULL;
    if(match(p, TK_EQUAL)) {
        if(!allowInitializer) {
            error(p, "Variable initializer not allowed in this context.");
            return NULL; // no need to free ASTStrings, owned by program instance.
        }
        initializer = parseExpression(p);
    }
    TRY_CONSUME(p, TK_SEMICOLON);

    Location loc = locationMerge(start, previous(p).location);
    return astVarDeclStmtNew(getCurrentAllocator(p), loc, obj, initializer);
}

// parameter_list -> typed_var+ (',' typed_var)* [requires typed_var to have a type]
// parameters: Array<ASTObj *>
static bool parseParameterList(Parser *p, Array *parameters) {
    bool hadError = false;
    do {
        ASTObj *paramVar = parseTypedVariable(p);
        if(!paramVar) {
            hadError = true;
            continue;
        }
        arrayPush(parameters, (void *)paramVar);
        if(paramVar->dataType == NULL) {
            error(p, tmp_buffer_format(p, "Parameter '%s' is missing a type.", paramVar->name));
            hadError = true;
        }
    } while(match(p, TK_COMMA));

    return !hadError;
}

// function_decl -> 'fn' identifier '(' parameter_list ')' ('->' type)+ block
// structName: For methods ONLY. otherwise set to NULL.
static ASTObj *parseFunctionDecl(Parser *p, ASTString structName) {
    // Assumes 'fn' was already consumed.
    Location loc = previous(p).location;
    ASTString name = TRY(ASTString, parseIdentifier(p));
    TRY_CONSUME(p, TK_LPAREN);
    Array parameters; // Array<ASTObj *>
    arrayInit(&parameters);
    // If this is a method, parse the "this" parameter if it exists.
    // "&this" is syntactic sugar for "this: &StructType", which means that the latter will also
    // make the function a non-static bound method.
    if(structName && match(p, TK_AMPERSAND)) {
        ASTString thisIdentifier = parseIdentifier(p);
        // FIXME: stringEqual() will cause undefined behaiviour when it
        //        tries to check if the string literal "this" is a valid 'String'...
        //        See FIXME in Strings.h.
        if(!stringEqual(thisIdentifier, "this")) {
            // Uses previous location (the identifier) which is ok.
            error(p, "Expected 'this' after '&' in first parameter.");
            arrayFree(&parameters);
            return NULL;
        }

        // Get (or create if it doesn't exist yet) the pointer type for the enclosing struct.
        Type *thisType = NULL;
        if((thisType = astModuleGetType(getCurrentModule(p), tmp_buffer_format(p, "&%s", structName))) == NULL) {
            thisType = typeNew(TY_POINTER, stringTableFormat(p->program->strings, "&%s", structName), EMPTY_LOCATION, p->current.module);
            thisType->as.ptr.innerType = astModuleGetType(getCurrentModule(p), structName);
            astModuleAddType(getCurrentModule(p), thisType);
        }

        // Create the "this" parameter.
        ASTObj *thisParam = astModuleNewObj(getCurrentModule(p), OBJ_VAR, EMPTY_LOCATION, thisIdentifier, thisType);
        arrayPush(&parameters, (void *)thisParam);

        // If there are more parameters, consume a comma.
        if(current(p).type != TK_RPAREN) {
            TRY_CONSUME(p, TK_COMMA);
        }
    }
    // Note: if current is ')', then there are no parameters.
    if(current(p).type != TK_RPAREN && !parseParameterList(p, &parameters)) {
        arrayFree(&parameters);
        return NULL;
    }
    if(!consume(p, TK_RPAREN)) {
        arrayFree(&parameters);
        return NULL;
    }

    // Default function return type is 'void'.
    Type *returnType = astModuleGetType(getCurrentModule(p), "void");
    VERIFY(returnType != NULL); // If this fails, there is a bug with the type management system.
    if(match(p, TK_ARROW)) {
        Type *parsedReturnType = parseType(p);
        if(!parsedReturnType) {
            arrayFree(&parameters);
            return NULL;
        }
        returnType = parsedReturnType;
    }
    loc = locationMerge(loc, previous(p).location);

    if(!consume(p, TK_LBRACE)) {
        arrayFree(&parameters);
        return NULL;
    }
    Scope *scope = enterScope(p, SCOPE_DEPTH_BLOCK);
    ARRAY_FOR(i, parameters) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &parameters, i);
        scopeAddObject(scope, param);
    }
    ASTBlockStmt *body = parseBlockStmt(p, scope, parseFunctionBodyStatements);
    leaveScope(p);
    if(!body) {
        arrayFree(&parameters);
        return NULL;
    }
    Type *fnType = makeFunctionType(p, parameters, returnType);
    ASTObj *fnObj = astModuleNewObj(getCurrentModule(p), OBJ_FN, loc, name, fnType);
    fnObj->as.fn.body = body;
    fnObj->as.fn.returnType = returnType;
    arrayCopy(&fnObj->as.fn.parameters, &parameters);
    arrayFree(&parameters);
    return fnObj;
}

static Type *makeStructType(Parser *p, ASTString name, Location declLoc, ModuleID declModule, Array *fields) {
    Type *ty = typeNew(TY_STRUCT, name, declLoc, declModule);

    Array *fieldTypes = &ty->as.structure.fieldTypes;
    ARRAY_FOR(i, *fields) {
        void *f = arrayGet(fields, i);
        VERIFY(f);
        arrayPush(fieldTypes, f);
    }

    // FIXME: What if there is a type with the same name that isn't a struct?
    Type *existingType = astModuleGetType(getCurrentModule(p), name);
    if(existingType) {
        typeFree(ty);
        return existingType;
    }
    astModuleAddType(getCurrentModule(p), ty);
    return ty;
}

// struct_field -> typed_var ';'
// fieldTypes: Array<Type *>
// structRef: The ASTObj* refering to the struct being parsed. NOT MODIFIED!
static bool parse_struct_field(Parser *p, Array *fieldTypes, ASTObj *structRef) {
    ASTObj *field = parseTypedVariable(p);
    bool hadError = false;
    if(field) {
        field->parent = structRef;
        if(field->dataType == NULL) {
            error(p, tmp_buffer_format(p, "Field '%s' has no type.", field->name));
            // Assume the syntax is correct apart from the missing type.
            // This makes sure we don't emit a cascading error about expecting an identifier
            // (the next field) but getting a semicolon.
            TRY_CONSUME(p, TK_SEMICOLON);
            return false; // Following checks depend on type existing, so return early.
        }
        if(field->dataType->type == TY_VOID) {
            errorAt(p, field->location, "A struct field cannot have the type 'void'.");
            hadError = true;
        } else if(field->dataType->type == TY_POINTER) {
            errorAt(p, field->location, "A struct field may not be a pointer.");
            hadError = true;
        }
        if(scopeHasObject(getCurrentScope(p), field->name)) {
            ASTObj *prefField = scopeGetAnyObject(getCurrentScope(p), field->name);
            errorAt(p, field->location, tmp_buffer_format(p, "Redefinition of struct field '%s'.", field->name));
            hint(p, prefField->location, "Previous definition was here.");
        } else {
            scopeAddObject(getCurrentScope(p), field);
            arrayPush(fieldTypes, (void *)field->dataType);
        }
    }
    // Always parse ';'
    TRY_CONSUME(p, TK_SEMICOLON);
    return field == NULL || hadError ? false : true;
}

// TODO: Remove this function since "this" is now parsed parseFunctionDecl().
// method_decl -> function_decl (with extra 'this' parameter.)
static bool parse_method_decl(Parser *p, ASTObj *structure) {
    VERIFY(consume(p, TK_FN)); // MUST be true if this function is called by parseSTructDecl().
    ASTObj *method = parseFunctionDecl(p, structure->name);
    if(!method) {
        return false;
    }
    method->parent = structure;
    scopeAddObject(getCurrentScope(p), method);
    return true;
}

// struct_decl -> 'struct' identifier '{' struct_field* method_decl* '}'
static ASTObj *parseStructDecl(Parser *p) {
    // Assumes 'struct' was already consumed
    Location loc = previous(p).location;
    ASTString name = TRY(ASTString, parseIdentifier(p));
    TRY_CONSUME(p, TK_LBRACE);
    // WARNING: struct location is INCOMPLETE until the end of this function!
    ASTObj *st = astModuleNewObj(getCurrentModule(p), OBJ_STRUCT, loc, name, NULL);


    Array fieldTypes; // Array<Type *>
    arrayInit(&fieldTypes);
    Scope *sc = enterScope(p, SCOPE_DEPTH_STRUCT);
    bool hadError = false;
    // First parse any fields.
    while(current(p).type != TK_RBRACE && current(p).type != TK_FN) {
        if(current(p).type != TK_IDENTIFIER) {
            // Advance anyway to prevent infinite loop here.
            advance(p);
            error(p, tmp_buffer_format(p, "Expected field declaration but got '%s'.", tokenTypeString(previous(p).type)));
            continue;
        }
        if(!parse_struct_field(p, &fieldTypes, st)) {
            hadError = true;
        }
    }
    loc = locationMerge(loc, previous(p).location);
    if(hadError) {
        arrayFree(&fieldTypes);
        return NULL;
    }
    // Need to make the type now so it can be used in the 'this' parameter passed to methods.
    // Note: using p.current.module since we need the ModuleID.
    Type *type = makeStructType(p, name, loc, p->current.module, &fieldTypes);
    arrayFree(&fieldTypes);
    // Now parse any methods.
    while(current(p).type != TK_RBRACE) {
        if(current(p).type != TK_FN) {
            // Advance anyway to prevent infinite loop here.
            advance(p);
            error(p, tmp_buffer_format(p, "Expected method declaration but got '%s'.", tokenTypeString(previous(p).type)));
            continue;
        }
        if(!parse_method_decl(p, st)) {
            hadError = true;
            // Synchronize to bound function/struct boundaries.
            // TODO: when the 'public' keyword is added, also sync to it.
            while(!isEof(p) && current(p).type != TK_FN && current(p).type != TK_RBRACE) {
                advance(p);
            }
        }
    }
    TRY_CONSUME(p, TK_RBRACE);
    leaveScope(p);
    st->as.structure.scope = sc;
    st->dataType = type;
    st->location = loc;
    return st;
}

// declaration -> var_decl | function_decl | struct_decl | extern_decl
static ASTObj *parseDeclaration(Parser *p) {
    // Notes: * Add nothing to scope. we only parse!
    //        * No need to TRY() since if 'result' is NULL, we will return NULL.
    //        * Variable declarations are handled in parseModuleBody().
    ASTObj *result = NULL;
    if(match(p, TK_FN)) {
        result = parseFunctionDecl(p, NULL);
    } else if(match(p, TK_STRUCT)) {
        result = parseStructDecl(p);
//    } else if(match(p, TK_EXTERN)) {
//
    } else {
        errorAt(p, current(p).location, "Expected declaration.");
    }

    return result;
}


// Sync to declaration boundaries
static void synchronizeToDecl(Parser *p) {
    // Unwind the scope tree/list to the module scope.
    ScopeDepth scopeDepth = getCurrentScope(p)->depth;
    while(getCurrentScope(p)->depth != SCOPE_DEPTH_MODULE_NAMESPACE) {
        VERIFY(p->current.scope->parent != NULL); // Should never happen.
        p->current.scope = p->current.scope->parent;
    }
    while(!isEof(p)) {
        switch(current(p).type) {
            case TK_FN:
            // If we were parsing a struct when we failed, we want to exit it completely
            // since we unwound the scope tree to the module scope, meaning that the state
            // of the parser is not inside a struct anymore.
            if(scopeDepth == SCOPE_DEPTH_STRUCT) {
                break;
            }
            case TK_VAR:
            case TK_STRUCT:
            //case TK_EXTERN:
            case TK_IMPORT:
                return;
            default:
                break;
        }
        advance(p);
    }
}

static void synchronizeToFnStructDecl(Parser *p) {
    while(!isEof(p)) {
        switch(current(p).type) {
            case TK_FN:
            case TK_STRUCT:
            //case TK_EXTERN: // TODO: re-add once we parse extern structs (also in synchronizeToDecl().)
                return;
            default:
                break;
        }
        advance(p);
    }
}

static void import_primitive_types(Parser *p, ModuleID mID) {
    ASTProgram *prog = p->program;
    ASTModule *module = astProgramGetModule(prog, mID);
    // We "import" the primitive types into each module.
    // In reality, we create new types each time, but since
    // primitives are equal by their TypeType, this doesn't matter.
#define DEF(type, typenameInParser, name) {Type *ty = typeNew((type), stringTableString(prog->strings, (name)), EMPTY_LOCATION, mID); astModuleAddType(module, ty); p->primitives.typenameInParser = ty;}

    DEF(TY_VOID, void_, "void");
    DEF(TY_I32, int32, "i32");
    DEF(TY_U32, uint32, "u32");
    DEF(TY_STR, str, "str");
    DEF(TY_BOOL, boolean, "bool");

#undef DEF
}

// Returns true on successful parse, or false on failure.
// module_body -> import* declaration*
// import -> 'import' <str> ';'
static bool parseModuleBody(Parser *p, ASTString name) {
    ModuleID mID = astProgramNewModule(p->program, name);
    ASTModule *module = astProgramGetModule(p->program, mID);
    // If we are parsing a module body, there shouldn't be an existing current module.
    VERIFY(p->current.module == 0);
    VERIFY(getCurrentScope(p) == NULL);
    p->current.module = mID;
    p->current.scope = module->moduleScope;
    // Import all the primitive (builtin) types into the module.
    import_primitive_types(p, mID);

    bool failedInFunctionDecl = false;
    while(!isEof(p)) {
        if(match(p, TK_VAR)) {
            ASTVarDeclStmt *varDecl = parseVarDecl(p, true);
            if(varDecl) {
                if(scopeHasObject(getCurrentScope(p), varDecl->variable->name)) {
                    ASTObj *prevDecl = scopeGetAnyObject(getCurrentScope(p), varDecl->variable->name);
                    errorAt(p, varDecl->variable->location, tmp_buffer_format(p, "Redeclaration of symbol '%s'.", varDecl->variable->name));
                    hint(p, prevDecl->location, "Previous declaration was here.");
                } else {
                    scopeAddObject(getCurrentScope(p), varDecl->variable);
                    arrayPush(&getCurrentModule(p)->variableDecls, (void *)varDecl);
                }
            }
        } else if(match(p, TK_IMPORT)) {
            if(!consume(p, TK_STRING_LITERAL)) {
                continue;
            }
            Token importStrToken = previous(p);
            if(!consume(p, TK_SEMICOLON)) {
                continue;
            }
            // Trim '"'s from beginning and end of string.
            ASTString importStr = stringTableFormat(p->program->strings, "%.*s", importStrToken.length-2, importStrToken.lexeme+1);
            // TODO: replace "." with PATH variable of sorts (MODULE_PATH/IMPORT_PATH etc.)
            if(!doesFileExist(".", tmp_buffer_format(p, "%s.ilc", importStr))) {
                errorAt(p, importStrToken.location, tmp_buffer_format(p, "Cannot find module '%s'.", importStr));
                hint(p, importStrToken.location, tmp_buffer_format(p, "Is the filename of requested module '%s.ilc'?", importStr));
                continue;
            }
            compilerAddFile(p->compiler, stringTableFormat(p->program->strings, "%s.ilc", importStr));
            // ModuleID is added in validator. for now, set it to -1 just to have a value that is unlikely to occur naturaly.
            astModuleAddImport(getCurrentModule(p), importStr, (ModuleID)-1);
        } else if(match(p, TK_FILE_CHANGED)) {
            // End of module. exit.
            break;
        } else {
            ASTObj *obj = parseDeclaration(p);
            if(obj) {
                if(scopeHasObject(getCurrentScope(p), obj->name)) {
                    ASTObj *prevDecl = scopeGetAnyObject(getCurrentScope(p), obj->name);
                    errorAt(p, obj->location, tmp_buffer_format(p, "Redeclaration of symbol '%s'.", obj->name));
                    hint(p, prevDecl->location, "Previous declaration was here.");
                } else {
                    scopeAddObject(getCurrentScope(p), obj);
                }
            } else {
                failedInFunctionDecl = true;
            }
        }

        if(p->state.need_sync) {
            p->state.need_sync = false;
            if(failedInFunctionDecl) {
                synchronizeToFnStructDecl(p);
            } else {
                synchronizeToDecl(p);
            }
        }
    }

    p->current.scope = NULL;
    return !p->state.had_error;
}

bool parserParse(Parser *p, ASTProgram *prog) {
    p->program = prog;

    // Get the first token.
    advance(p);
    // If the scanner failed to set the source file, we can't do anything.
    if(p->scanner->failed_to_set_source) {
        // The scanner has alreaedy reported the error.
        p->program = NULL;
        return false;
    }

    while(!isEof(p)) {
        File *file = compilerGetFile(p->compiler, compilerGetCurrentFileID(p->compiler));
        ASTString moduleName = stringTableString(p->program->strings, file->fileNameNoExtension);

        if(!parseModuleBody(p, moduleName)) {
            // Errors have already been reported
            p->program = NULL;
            return false;
        }
    }

    return true;
}

#undef TRY
