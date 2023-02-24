#include <stdlib.h> // strtoul
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"
#include "Error.h"
#include "Token.h" // Location
#include "Ast.h"
#include "Scanner.h"
#include "Parser.h"

void parserInit(Parser *p, Scanner *s, Compiler *c) {
    p->compiler = c;
    p->scanner = s;
    p->program = NULL;
    p->dump_tokens = false;
    p->current.module = 0;
    p->current.allocator = NULL;
    p->current.function = NULL;
    p->current.scope = EMPTY_SCOPE_ID();
    p->current.block_scope_depth = 0;
    p->current.attribute = NULL;
    // set current and previous tokens to TK_GARBAGE with empty locations
    // so errors can be reported using them.
    p->previous_token.type = TK_GARBAGE;
    p->previous_token.location = EMPTY_LOCATION();
    p->current_token.type = TK_GARBAGE;
    p->current_token.location = EMPTY_LOCATION();
    p->had_error = false;
    p->need_synchronize = false;
}

void parserFree(Parser *p) {
    p->compiler = NULL;
    p->scanner = NULL;
    p->program = NULL;
    p->dump_tokens = false;
    p->current.module = 0;
    p->current.allocator = NULL;
    p->current.function = NULL;
    p->current.scope = EMPTY_SCOPE_ID();
    p->current.block_scope_depth = 0;
    if(p->current.attribute) {
        attributeFree(p->current.attribute);
        p->current.attribute = NULL;
    }
    p->previous_token.type = TK_GARBAGE;
    p->current_token.type = TK_GARBAGE;
    p->had_error = false;
    p->need_synchronize = false;
}

void parserSetDumpTokens(Parser *p, bool value) {
    p->dump_tokens = value;
}

/** Callbacks **/

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astObjFree((ASTObj *)object);
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
    while((tk = scannerNextToken(p->scanner)).type == TK_GARBAGE) {
        if(p->dump_tokens) {
            tokenPrint(stdout, &tk);
            putchar('\n');
        }
        p->had_error = true;
    }
    if(p->dump_tokens) {
        tokenPrint(stdout, &tk);
        putchar('\n');
    }
    p->previous_token = p->current_token;
    p->current_token = tk;
    return tk;
}

// NOTE: If 'message' is a valid String, it will be freed.
static void add_error(Parser *p, bool has_location, Location loc, ErrorType type, char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, type, has_location, loc, message);
    compilerAddError(p->compiler, err);
    if(stringIsValid(message)) {
        stringFree(message);
    }
    p->had_error = true;
    p->need_synchronize = true;
}

// NOTE: If 'message' is a valid String, it will be freed.
static inline void error_at(Parser *p, Location loc, char *message) {
    add_error(p, true, loc, ERR_ERROR, message);
}

// NOTES: If 'message' is a valid String, it will be freed.
// The previous tokens' location is used.
static inline void error(Parser *p, char *message) {
    error_at(p, previous(p).location, message);
}

// NOTES: If 'message' is a valid String, it will be freed.
static inline void hint(Parser *p, Location loc, char *message) {
    add_error(p, true, loc, ERR_HINT, message);
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

static inline ScopeID enter_scope(Parser *p) {
    ASTModule *module = astProgramGetModule(p->program, p->current.module);
    Scope *parent = astModuleGetScope(module, p->current.scope);
    bool is_block_scope = p->current.block_scope_depth >= FUNCTION_SCOPE_DEPTH;
    Scope *child = scopeNew(p->current.scope, is_block_scope);
    ScopeID child_id = astModuleAddScope(module, child);
    scopeAddChild(parent, child_id);
    p->current.scope = child_id;
    if(is_block_scope) {
        p->current.block_scope_depth++;
        child->depth = p->current.block_scope_depth;
    }
    return child_id;
}

static void leave_scope(Parser *p) {
    ASTModule *module = astProgramGetModule(p->program, p->current.module);
    p->current.scope = astModuleGetScope(module, p->current.scope)->parent;
    if(p->current.block_scope_depth >= FUNCTION_SCOPE_DEPTH) {
        p->current.block_scope_depth--;
    }
}

static inline void add_variable_to_current_scope(Parser *p, ASTObj *var) {
    Scope *scope = astModuleGetScope(astProgramGetModule(p->program, p->current.module), p->current.scope);
    ASTObj *prev = (ASTObj *)tableSet(&scope->variables, (void *)var->name, (void *)var);
    if(prev != NULL) {
        error_at(p, var->name_location, stringFormat("Redefinition of variable '%s'.", var->name));
        hint(p, prev->name_location, "Previous definition here.");
    }
}

static inline void add_function_to_current_scope(Parser *p, ASTObj *fn) {
    Scope *scope = astModuleGetScope(astProgramGetModule(p->program, p->current.module), p->current.scope);
    ASTObj *prev = (ASTObj *)tableSet(&scope->functions, (void *)fn->name, (void *)fn);
    if(prev != NULL) {
        error_at(p, fn->name_location, stringFormat("Redefinition of function '%s'.", fn->name));
        hint(p, prev->name_location, "Previous definition here.");
    }
}

static inline void add_structure_to_current_scope(Parser *p, ASTObj *structure) {
    Scope *scope = astModuleGetScope(astProgramGetModule(p->program, p->current.module), p->current.scope);
    ASTObj *prev = (ASTObj *)tableSet(&scope->structures, (void *)structure->name, (void *)structure);
    if(prev != NULL) {
        error_at(p, structure->name_location, stringFormat("Redefinition of struct '%s'.", structure->name));
        hint(p, prev->name_location, "Previous definition here.");
    }
}

static ASTObj *find_variable_in_current_scope(Parser *p, ASTString name) {
    Scope *scope = astModuleGetScope(astProgramGetModule(p->program, p->current.module), p->current.scope);
    TableItem *i = tableGet(&scope->variables, (void *)name);
    if(i) {
        return (ASTObj *)i->value;
    }
    return NULL;
}

static inline ScopeID enter_function(Parser *p, ASTObj *fn) {
    p->current.function = fn;
    p->current.block_scope_depth = FUNCTION_SCOPE_DEPTH; // FIXME: this won't work with closures.
    return enter_scope(p);
}

static inline void leave_function(Parser *p) {
    // FIXME: find a better way to represent function_scope_depth + 1.
    VERIFY(p->current.block_scope_depth == FUNCTION_SCOPE_DEPTH + 1); // FIXME: this won't work with closures.
    leave_scope(p);
    p->current.function = NULL;
    p->current.block_scope_depth = 0;
}

static void store_attribute(Parser *p, Attribute *attr) {
    VERIFY(p->current.attribute == NULL);
    p->current.attribute = attr;
}

static Attribute *take_attribute(Parser *p) {
    VERIFY(p->current.attribute);
    Attribute *attr = p->current.attribute;
    p->current.attribute = NULL;
    return attr;
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

typedef ASTNode *(*PrefixParseFn)(Parser *p);
typedef ASTNode *(*InfixParseFn)(Parser *p, ASTNode *lhs);

typedef struct parse_rule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;

/* Forward-declarations */
static ASTNode *parse_function_body(Parser *p);
static ASTNode *parse_statement(Parser *p);

/* Parse functions */
static ASTNode *parse_precedence(Parser *p, Precedence min_prec);
static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_number_literal_expr(Parser *p);
static ASTNode *parse_string_literal_expr(Parser *p);
static ASTNode *parse_grouping_expr(Parser *p);
static ASTNode *parse_identifier_expr(Parser *p);
static ASTNode *parse_unary_expr(Parser *p);
static ASTNode *parse_binary_expr(Parser *p, ASTNode *lhs);
static ASTNode *parse_assignment(Parser *p, ASTNode *lhs);
static ASTNode *parse_call_expr(Parser *p, ASTNode *callee);
static ASTNode *parse_property_access_expr(Parser *p, ASTNode *lhs);

static ParseRule rules[] = {
    [TK_LPAREN]         = {parse_grouping_expr, parse_call_expr, PREC_CALL},
    [TK_RPAREN]         = {NULL, NULL, PREC_LOWEST},
    [TK_LBRACKET]       = {NULL, NULL, PREC_LOWEST},
    [TK_RBRACKET]       = {NULL, NULL, PREC_LOWEST},
    [TK_LBRACE]         = {NULL, NULL, PREC_LOWEST},
    [TK_RBRACE]         = {NULL, NULL, PREC_LOWEST},
    [TK_PLUS]           = {parse_unary_expr, parse_binary_expr, PREC_TERM},
    [TK_STAR]           = {parse_unary_expr, parse_binary_expr, PREC_FACTOR},
    [TK_SLASH]          = {NULL, parse_binary_expr, PREC_FACTOR},
    [TK_SEMICOLON]      = {NULL, NULL, PREC_LOWEST},
    [TK_COLON]          = {NULL, NULL, PREC_LOWEST},
    [TK_COMMA]          = {NULL, NULL, PREC_LOWEST},
    [TK_DOT]            = {NULL, parse_property_access_expr, PREC_CALL},
    [TK_HASH]           = {NULL, NULL, PREC_LOWEST},
    [TK_AMPERSAND]      = {parse_unary_expr, NULL, PREC_LOWEST},
    [TK_MINUS]          = {parse_unary_expr, parse_binary_expr, PREC_TERM},
    [TK_ARROW]          = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL]          = {NULL, parse_assignment, PREC_ASSIGNMENT},
    [TK_EQUAL_EQUAL]    = {NULL, parse_binary_expr, PREC_EQUALITY},
    [TK_BANG]           = {NULL, NULL, PREC_LOWEST},
    [TK_BANG_EQUAL]     = {NULL, parse_binary_expr, PREC_EQUALITY},
    [TK_LESS]           = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_LESS_EQUAL]     = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_GREATER]        = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_GREATER_EQUAL]  = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_NUMBER_LITERAL] = {parse_number_literal_expr, NULL, PREC_LOWEST},
    [TK_STRING_LITERAL] = {parse_string_literal_expr, NULL, PREC_LOWEST},
    [TK_IF]             = {NULL, NULL, PREC_LOWEST},
    [TK_ELSE]           = {NULL, NULL, PREC_LOWEST},
    [TK_WHILE]          = {NULL, NULL, PREC_LOWEST},
    [TK_FN]             = {NULL, NULL, PREC_LOWEST},
    [TK_RETURN]         = {NULL, NULL, PREC_LOWEST},
    [TK_VAR]            = {NULL, NULL, PREC_LOWEST},
    [TK_STRUCT]         = {NULL, NULL, PREC_LOWEST},
    [TK_EXTERN]         = {NULL, NULL, PREC_LOWEST},
    [TK_DEFER]          = {NULL, NULL, PREC_LOWEST},
    [TK_VOID]           = {NULL, NULL, PREC_LOWEST},
    [TK_I32]            = {NULL, NULL, PREC_LOWEST},
    [TK_U32]            = {NULL, NULL, PREC_LOWEST},
    [TK_STR]            = {NULL, NULL, PREC_LOWEST},
    [TK_IDENTIFIER]     = {parse_identifier_expr, NULL, PREC_LOWEST},
    [TK_GARBAGE]        = {NULL, NULL, PREC_LOWEST},
    [TK_EOF]            = {NULL, NULL, PREC_LOWEST}
};
_Static_assert(sizeof(rules)/sizeof(rules[0]) == TK_TYPE_COUNT, "Missing token type(s) in parser rule table!");

static ParseRule *get_rule(TokenType type) {
    return &rules[type];
}

static ASTString parse_identifier(Parser *p) {
    TRY_CONSUME(p, TK_IDENTIFIER);
    return astProgramAddString(p->program, stringNCopy(previous(p).lexeme, previous(p).length));
}

static ASTNode *parse_identifier_expr(Parser *p) {
    ASTString str = astProgramAddString(p->program, stringNCopy(previous(p).lexeme, previous(p).length));
    return astNewIdentifierNode(p->current.allocator, previous(p).location, str);
}

static ASTNode *parse_number_literal_expr(Parser *p) {
    // TODO: Support hex, octal & binary.
    Location loc = previous(p).location;
    u64 value = strtoul(previous(p).lexeme, NULL, 10);
    Type *postfix_type = NULL;
    // Note: It isn't possible to use parse_type() here as the type
    //       is optional and there isn't any way to know when there is
    //       one and when there isn't.
    switch(current(p).type) {
        case TK_I32:
            postfix_type = p->program->primitives.int32;
            loc = locationMerge(loc, current(p).location);
            advance(p);
            break;
        case TK_U32:
            postfix_type = p->program->primitives.uint32;
            loc = locationMerge(loc, current(p).location);
            advance(p);
            break;
        case TK_STR:
        case TK_VOID:
            // Consume the type anyway to suppress further errors because of it.
            advance(p);
            error_at(p, locationMerge(loc, previous(p).location), stringFormat("Invalid suffix '%s' (suffix must be a numeric type).", tokenTypeString(previous(p).type)));
            return NULL;
        default:
            break;
    }
    return astNewLiteralValueNode(p->current.allocator, ND_NUMBER_LITERAL, loc, LITERAL_VALUE(LIT_NUMBER, number, value), postfix_type);
}

static ASTNode *parse_string_literal_expr(Parser *p) {
    Token tk = previous(p);
    // lexeme + 1 to trim the opening '"', and length - 2 to trim both '"'.
    ASTString str = astProgramAddString(p->program, stringNCopy(tk.lexeme + 1, tk.length - 2));
    // TODO: proccess escapes (e.g. \x1b[..., \033[..., \e[..., \27[... etc.).
    return astNewLiteralValueNode(p->current.allocator, ND_STRING_LITERAL, tk.location, LITERAL_VALUE(LIT_STRING, str, str), NULL);
}

static ASTNode *parse_grouping_expr(Parser *p) {
    ASTNode *expr = TRY(ASTNode *, parse_expression(p));
    TRY_CONSUME(p, TK_RPAREN);
    return expr;
}

static ASTNode *parse_unary_expr(Parser *p) {
    Token operator = previous(p);
    ASTNode *operand = TRY(ASTNode *, parse_precedence(p, PREC_UNARY));

    switch(operator.type) {
        case TK_PLUS:
            return operand;
        case TK_MINUS:
            return astNewUnaryNode(p->current.allocator, ND_NEGATE, locationMerge(operator.location, operand->location), operand);
        case TK_AMPERSAND:
            return astNewUnaryNode(p->current.allocator, ND_ADDROF, locationMerge(operator.location, operand->location), operand);
        case TK_STAR:
            return astNewUnaryNode(p->current.allocator, ND_DEREF, locationMerge(operator.location, operand->location), operand);
        default: UNREACHABLE();
    }
}

static ASTNode *parse_binary_expr(Parser *p, ASTNode *lhs) {
    TokenType op = previous(p).type;
    ParseRule *rule = get_rule(op);
    ASTNode *rhs = TRY(ASTNode *, parse_precedence(p, rule->precedence));

    ASTNodeType node_type;
    switch(op) {
        case TK_PLUS: node_type = ND_ADD; break;
        case TK_MINUS: node_type = ND_SUBTRACT; break;
        case TK_STAR: node_type = ND_MULTIPLY; break;
        case TK_SLASH: node_type = ND_DIVIDE; break;
        case TK_EQUAL_EQUAL: node_type = ND_EQ; break;
        case TK_BANG_EQUAL: node_type = ND_NE; break;
        case TK_LESS: node_type = ND_LT; break;
        case TK_LESS_EQUAL: node_type = ND_LE; break;
        case TK_GREATER: node_type = ND_GT; break;
        case TK_GREATER_EQUAL: node_type = ND_GE; break;
        default: UNREACHABLE();
    }
    return astNewBinaryNode(p->current.allocator, node_type, locationMerge(lhs->location, rhs->location), lhs, rhs);
}

static ASTNode *parse_assignment(Parser *p, ASTNode *lhs) {
    ASTNode *rhs = TRY(ASTNode *, parse_expression(p));
    return astNewBinaryNode(p->current.allocator, ND_ASSIGN, locationMerge(lhs->location, rhs->location), lhs, rhs);
}

static ASTNode *parse_call_expr(Parser *p, ASTNode *callee) {
    Location loc = current(p).location;
    Array args;
    arrayInit(&args);
    if(current(p).type != TK_RPAREN) {
        do {
            ASTNode *arg = parse_expression(p);
            if(!arg) {
                continue;
            }
            arrayPush(&args, (void *)arg);
            loc = locationMerge(loc, previous(p).location);
        } while(match(p, TK_COMMA));
    }
    if(!consume(p, TK_RPAREN)) {
        arrayFree(&args);
        return NULL;
    }
    ASTListNode *arguments = AS_LIST_NODE(astNewListNode(p->current.allocator, ND_ARGS, loc, EMPTY_SCOPE_ID(), arrayLength(&args)));
    arrayCopy(&arguments->nodes, &args);
    arrayFree(&args);
    return astNewBinaryNode(p->current.allocator, ND_CALL, locationMerge(callee->location, previous(p).location), callee, AS_NODE(arguments));
}

static ASTNode *parse_property_access_expr(Parser *p, ASTNode *lhs) {
    ASTString property_name = TRY(ASTString, parse_identifier(p));
    Location property_name_loc = previous(p).location;

    return astNewBinaryNode(p->current.allocator, ND_PROPERTY_ACCESS, locationMerge(lhs->location, property_name_loc), lhs, astNewIdentifierNode(p->current.allocator, property_name_loc, property_name));
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

static ControlFlow statement_control_flow(ASTNode *stmt) {
    switch(stmt->node_type) {
        case ND_RETURN: return CF_ALWAYS_RETURNS;
        case ND_IF: {
            ControlFlow then_block = statement_control_flow(AS_CONDITIONAL_NODE(stmt)->body);
            if(AS_CONDITIONAL_NODE(stmt)->else_) {
                ControlFlow else_stmt = statement_control_flow(AS_CONDITIONAL_NODE(stmt)->else_);
                if((then_block == CF_NEVER_RETURNS && else_stmt == CF_ALWAYS_RETURNS) ||
                   (then_block == CF_ALWAYS_RETURNS && else_stmt == CF_NEVER_RETURNS)) {
                    return CF_MAY_RETURN;
                } else if(then_block == else_stmt) {
                    return then_block;
                }
            }
            return then_block;
        }
        case ND_WHILE_LOOP: return statement_control_flow(AS_LOOP_NODE(stmt)->body);
        case ND_BLOCK: return AS_LIST_NODE(stmt)->control_flow;
        default:
            break;
    }
    return CF_NEVER_RETURNS;
}

static void synchronize_in_block(Parser *p) {
    // synchronize to statement boundaries
    // (statements also include variable declarations in this context).
    while(!is_eof(p) && current(p).type != TK_RBRACE) {
        TokenType c = current(p).type;
        if(c == TK_VAR || c == TK_RETURN || c == TK_IF) {
            break;
        } else {
            advance(p);
        }
    }
}

// block(parse_callback) -> parse_callback* '}' /* Note: the opening brace is consumed by the caller. */
static ASTNode *parse_block(Parser *p, ScopeID scope, ASTNode *(*parse_callback)(Parser *p)) {
    // Assume '{' was already consumed.
    Location start = previous(p).location;
    ControlFlow cf = CF_NEVER_RETURNS;
    Array nodes;
    arrayInit(&nodes);

    bool unreachable = false;
    while(!is_eof(p) && current(p).type != TK_RBRACE) {
        ASTNode *node = parse_callback(p);
        if(unreachable) {
            error_at(p, node->location, "Unreachable code.");
            synchronize_in_block(p);
            arrayFree(&nodes);
            return NULL;
        }
        if(node) {
            arrayPush(&nodes, (void *)node);
            // FIXME: find a better way to represent function_scope_depth + 1.
            if(NODE_IS(node, ND_RETURN) && p->current.block_scope_depth == FUNCTION_SCOPE_DEPTH + 1) {
                cf = CF_ALWAYS_RETURNS;
                unreachable = true;
            } else {
                cf = controlFlowUpdate(cf, statement_control_flow(node));
            }
        } else {
            synchronize_in_block(p);
        }
    }
    if(!consume(p, TK_RBRACE)) {
        arrayFree(&nodes);
        return NULL;
    }

    ASTListNode *n = AS_LIST_NODE(astNewListNode(p->current.allocator, ND_BLOCK, locationMerge(start, previous(p).location), scope, arrayLength(&nodes)));
    arrayCopy(&n->nodes, &nodes);
    arrayFree(&nodes);
    n->control_flow = cf;
    return AS_NODE(n);
}

// expression_stmt -> expression ';'
static ASTNode *parse_expression_stmt(Parser *p) {
    ASTNode *expr = TRY(ASTNode *, parse_expression(p));
    TRY_CONSUME(p, TK_SEMICOLON);
    return expr;
}

// return_stmt -> 'return' expression? ';'
static ASTNode *parse_return_stmt(Parser *p) {
    // Assume 'return' is already consumed.
    Location start = previous(p).location;
    ASTNode *operand = NULL;
    if(current(p).type != TK_SEMICOLON) {
        operand = TRY(ASTNode *, parse_expression(p));
    }
    TRY_CONSUME(p, TK_SEMICOLON);

    return astNewUnaryNode(p->current.allocator, ND_RETURN, locationMerge(start, previous(p).location), operand);
}

// if_stmt -> 'if' expression '{' block(function_body) ('else' (if_stmt | '{' block(function_body)))?
static ASTNode *parse_if_stmt(Parser *p) {
    // Assume 'if' is already consumed.
    Location start = previous(p).location;
    ASTNode *condition = TRY(ASTNode *, parse_expression(p));
    TRY_CONSUME(p, TK_LBRACE);
    ScopeID scope = enter_scope(p);
    ASTNode *body = TRY(ASTNode *, parse_block(p, scope, parse_function_body));
    leave_scope(p);

    ASTNode *else_ = NULL;
    if(match(p, TK_ELSE)) {
        if(match(p, TK_IF)) {
            else_ = TRY(ASTNode *, parse_if_stmt(p));
        } else {
            TRY_CONSUME(p, TK_LBRACE);
            scope = enter_scope(p);
            else_ = TRY(ASTNode *, parse_block(p, scope, parse_function_body));
            leave_scope(p);
        }
    }

    return astNewConditionalNode(p->current.allocator, ND_IF, locationMerge(start, previous(p).location), condition, body, else_);
}

static ASTNode *parse_while_loop_stmt(Parser *p) {
    // Assume 'while' is already consumed.
    Location start = previous(p).location;
    ASTNode *condition = TRY(ASTNode *, parse_expression(p));
    TRY_CONSUME(p, TK_LBRACE);
    ScopeID scope = enter_scope(p);
    ASTNode *body = TRY(ASTNode *, parse_block(p, scope, parse_function_body));
    leave_scope(p);
    return astNewLoopNode(p->current.allocator, locationMerge(start, previous(p).location), NULL, condition, NULL, body);
}

// defer_operand -> '{' block(expression_stmt) | expression
static ASTNode *parse_defer_operand(Parser *p) {
    ASTNode *result = NULL;
    if(match(p, TK_LBRACE)) {
        // FIXME: As variable declarations are not allowed, there is no need to enter a new scope,
        //        but it isn't possible to get the current ScopeID so a new scope has to be entered
        //        so it can be saved in the block.
        ScopeID scope = enter_scope(p);
        result = parse_block(p, scope, parse_expression_stmt);
        leave_scope(p);
    } else {
        result = parse_expression(p);
    }
    return result;
}

// defer_stmt -> 'defer' defer_body ';'
static ASTNode *parse_defer_stmt(Parser *p) {
    // Assume 'defer' is already consumed.
    Location start = previous(p).location;
    ASTNode *operand = TRY(ASTNode *, parse_defer_operand(p));
    TRY_CONSUME(p, TK_SEMICOLON);
    return astNewUnaryNode(p->current.allocator, ND_DEFER, locationMerge(start, previous(p).location), operand);
}

// statement -> '{' block(function_body) | return_stmt | if_stmt | while_loop_stmt | defer_stmt | expression_stmt
static ASTNode *parse_statement(Parser *p) {
    ASTNode *result = NULL;
    if(match(p, TK_LBRACE)) {
        ScopeID scope = enter_scope(p);
        result = parse_block(p, scope, parse_function_body);
        leave_scope(p);
    } else if(match(p, TK_RETURN)) {
        result = parse_return_stmt(p);
    } else if(match(p, TK_IF)) {
        result = parse_if_stmt(p);
    } else if(match(p, TK_WHILE)) {
        result = parse_while_loop_stmt(p);
    } else if(match(p, TK_DEFER)) {
        result = parse_defer_stmt(p);
    } else {
        result = parse_expression_stmt(p);
    }
    return result;
}

// primitive_type -> i32 | u32
static Type *parse_primitive_type(Parser *p) {
    if(match(p, TK_VOID)) {
        return p->program->primitives.void_;
    } else if(match(p, TK_I32)) {
        return p->program->primitives.int32;
    } else if(match(p, TK_U32)) {
        return p->program->primitives.uint32;
    } else if(match(p, TK_STR)) {
        return p->program->primitives.str;
    }
    return NULL;
}

// parameters: Array<ASTObj *> (OBJ_VAR)
static Type *new_fn_type(Parser *p, Type *return_type, Array parameters) {
    Type *ty;
    NEW0(ty);
    typeInit(ty, TY_FN, NULL, p->current.module);
    ty->as.fn.return_type = return_type;
    
    String name = stringCopy("fn(");
    for(usize i = 0; i < parameters.used; ++i) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &parameters, i);
        stringAppend(&name, "%s", param->data_type->name);
        if(i + 1 < parameters.used) {
            stringAppend(&name, ", ");
        }
        arrayPush(&ty->as.fn.parameter_types, param->data_type);
    }
    stringAppend(&name, ") -> %s", return_type ? return_type->name : "void");
    ty->name = astProgramAddString(p->program, name);

    return scopeAddType(astProgramGetModule(p->program, p->current.module)->module_scope, ty);
}

// fields: Array<ASTObj *> (OBJ_VAR)
static Type *new_struct_type(Parser *p, ASTString name, Array fields) {
    Type *ty;
    NEW0(ty);
    typeInit(ty, TY_STRUCT, name, p->current.module);
    Array *field_types = &ty->as.structure.field_types;
    for(usize i = 0; i < fields.used; ++i) {
        ASTObj *member = ARRAY_GET_AS(ASTObj *, &fields, i);
        arrayPush(field_types, (void *)member->data_type);
    }

    return scopeAddType(astProgramGetModule(p->program, p->current.module)->module_scope, ty);
}

static Type *parse_type(Parser *p);

// function_type -> 'fn' '(' (type)* ')' ('->' type)?
static Type *parse_function_type(Parser *p) {
    // Assume 'fn' was already consumed.
    Location start = previous(p).location;
    Type *ty = NULL;

    TRY_CONSUME(p, TK_LPAREN);
    Array parameters; // Array<ASTObj *>
    arrayInit(&parameters);
    if(current(p).type != TK_RPAREN) {
        do {
            Type *ty = parse_type(p);
            if(!ty) {
                continue;
            }
            // Wrap the types in an ASTObj because of new_fn_type().
            arrayPush(&parameters, (void *)astNewObj(OBJ_VAR, EMPTY_LOCATION(), EMPTY_LOCATION(), "", ty));
        } while(match(p, TK_COMMA));
    }
    if(!consume(p, TK_RPAREN)) {
        goto free_param_array;
    }
    Type *return_type = p->program->primitives.void_;
    if(match(p, TK_ARROW)) {
        if((return_type = parse_type(p)) == NULL) {
            goto free_param_array;
        }
    }

    ty = new_fn_type(p, return_type, parameters);
    ty->decl_location = locationMerge(start, previous(p).location);
free_param_array:
    arrayMap(&parameters, free_object_callback, NULL);
    arrayFree(&parameters);
    return ty;
}

// identifier_type -> identifier (The identifier has to be a type of a struct, enum, or type alias).
static Type *parse_type_from_identifier(Parser *p) {
    ASTString name = TRY(ASTString, parse_identifier(p));
    Location loc = previous(p).location;
    Type *ty;
    NEW0(ty);
    typeInit(ty, TY_ID, name, p->current.module);
    ty->decl_location = loc;
    return scopeAddType(astProgramGetModule(p->program, p->current.module)->module_scope, ty);
}

// complex_type -> fn_type | identifier_type
static Type *parse_complex_type(Parser *p) {
    if(match(p, TK_FN)) {
        return TRY(Type *, parse_function_type(p));
    } else if(current(p).type == TK_IDENTIFIER) {
        return TRY(Type *, parse_type_from_identifier(p));
    }
    error_at(p, current(p).location, "Expected typename.");
    return NULL;
}

// type -> primitive_type | complex_type
static Type *parse_type(Parser *p) {
    bool is_pointer = false;
    if(match(p, TK_AMPERSAND)) {
        is_pointer = true;
    }
    Type *ty = parse_primitive_type(p);
    if(!ty) {
        ty = TRY(Type *, parse_complex_type(p));
    }
    if(is_pointer) {
        ASTString ptr_name = astProgramAddString(p->program, stringFormat("&%s", ty->name));
        Type *ptr;
        NEW0(ptr);
        typeInit(ptr, TY_PTR, ptr_name, p->current.module);
        ptr->as.ptr.inner_type = ty;
        ty = scopeAddType(astProgramGetModule(p->program, p->current.module)->module_scope, ptr);
    }
    return ty;
}

// Notes: 1) The semicolon at the end isn't consumed.
//        2) [var_loc] may be NULL.
// variable_decl -> 'var' identifier (':' type)? ('=' expression)? ';'
static ASTNode *parse_variable_decl(Parser *p, bool allow_initializer, bool add_to_scope, Array *obj_array, Location *var_loc) {
    // Assumes 'var' was already consumed.

    ASTString name = TRY(ASTString, parse_identifier(p));
    Location name_loc = previous(p).location;

    Type *type = NULL;
    if(match(p, TK_COLON)) {
        type = TRY(Type *, parse_type(p));
    }

    ASTNode *initializer = NULL;
    if(allow_initializer && match(p, TK_EQUAL)) {
        initializer = TRY(ASTNode *, parse_expression(p));
    }

    ASTObj *var = astNewObj(OBJ_VAR, name_loc, name_loc, name, type);

    // Add the variable object.
    // NOTE: The object is being pushed to the array
    //       only AFTER all the parsing is done.
    //       This is so that "ghost" objects that belong to half-parsed
    //       variables won't be in the array and cause weird problems later on.
    arrayPush(obj_array, (void *)var);
    if(add_to_scope) {
        add_variable_to_current_scope(p, var);
    }

    // includes everything from the 'var' to the ';'.
    Location full_loc = locationMerge(name_loc, previous(p).location);
    if(var_loc) {
        full_loc = locationMerge(*var_loc, full_loc);
    }
    ASTNode *var_node = astNewObjNode(p->current.allocator, ND_VAR_DECL, name_loc, var);
    if(initializer != NULL) {
        return astNewBinaryNode(p->current.allocator, ND_ASSIGN, full_loc, var_node, initializer);
    }
    return var_node;
}

// struct_decl -> 'struct' identifier '{' (identifier: type ';')* '}'
static ASTObj *parse_struct_decl(Parser *p) {
    // Assumes 'struct' was already consumed.

    ASTString name = TRY(ASTString, parse_identifier(p));
    Location name_loc = previous(p).location;

    ASTObj *structure = astNewObj(OBJ_STRUCT, name_loc, name_loc, name, NULL);
    structure->as.structure.scope = enter_scope(p);
    Scope *scope = astModuleGetScope(astProgramGetModule(p->program, p->current.module), structure->as.structure.scope);

    if(!consume(p, TK_LBRACE)) {
        leave_scope(p);
        astObjFree(structure);
        return NULL;
    }
    Array fields;
    arrayInit(&fields);
    while(current(p).type != TK_RBRACE) {
        parse_variable_decl(p, false, false, &fields, NULL);
        ASTObj *field = ARRAY_GET_AS(ASTObj *, &fields, arrayLength(&fields) - 1);
        arrayPush(&scope->objects, (void *)field);
        add_variable_to_current_scope(p, field);
        consume(p, TK_SEMICOLON);
    }
    leave_scope(p);
    if(!consume(p, TK_RBRACE)) {
        arrayFree(&fields);
        astObjFree(structure);
        return NULL;
    }

    structure->data_type = new_struct_type(p,structure->name, fields);
    arrayFree(&fields);

    structure->location = locationMerge(name_loc, previous(p).location);
    add_structure_to_current_scope(p, structure);
    return structure;
}

// function_body -> ('var' variable_decl ';' | statement)
static ASTNode *parse_function_body(Parser *p) {
    VERIFY(p->current.function);

    Scope *current_scope = astModuleGetScope(astProgramGetModule(p->program, p->current.module), p->current.scope);
    ASTNode *result = NULL;
    if(match(p, TK_VAR)) {
        Location var_loc = previous(p).location;
        ASTNode *var_node = TRY(ASTNode *, parse_variable_decl(p, true, false, &current_scope->objects, &var_loc));
        TRY_CONSUME(p, TK_SEMICOLON);
        // Get the name of the variable to push to check
        // for redefinitions and to save in the current scope.
        ASTObj *var_obj = ARRAY_GET_AS(ASTObj *, &current_scope->objects, arrayLength(&current_scope->objects) - 1);
        VERIFY(var_obj->type == OBJ_VAR);
        ASTObj *existing_obj = find_variable_in_current_scope(p, var_obj->name);
        if(existing_obj) {
            error_at(p, var_node->location, stringFormat("Redeclaration of local variable '%s'.", var_obj->name));
            hint(p, existing_obj->location, "Previous declaration here.");
            // NOTE: arrayPop() is used even though we already have a reference
            //       to the object we want to free because we also want to remove
            //       the object from the array.
            astObjFree(ARRAY_POP_AS(ASTObj *, &current_scope->objects));
            return NULL;
        }
        add_variable_to_current_scope(p, var_obj);
        result = var_node;
    } else {
        // no need for TRY() here as nothing is done with the result node
        // other than to return it.
        result = parse_statement(p);
    }
    return result;
}

// parameter_list -> '(' (identifier ':' typename)+ (',' identifier ':' typename)* ')'
// parameters: Array<ASTObj *>
static bool parse_parameter_list(Parser *p, Array *parameters) {
    // Assume '(' already consumed.
    bool had_error = false;
    if(match(p, TK_RPAREN)) {
        return true;
    }
    do {
        ASTNode *param_var = parse_variable_decl(p, false, false, parameters, NULL);
        if(!param_var) {
            had_error = true;
        } else {
            ASTObj *param = ARRAY_GET_AS(ASTObj *, parameters, arrayLength(parameters) - 1);
            if(param->data_type == NULL) {
                error(p, stringFormat("Parameter '%s' has no type!", param->name));
                had_error = true;
            }
        }
    } while(match(p, TK_COMMA));
    if(!consume(p, TK_RPAREN)) {
        return false;
    }
    return !had_error;
}

// Note: the body is not parsed if [parse_body] is set to false.
// function_decl -> 'fn' identifier '(' parameter_list? ')' ('->' type)? block(fn_body)
static ASTObj *parse_function_decl(Parser *p, bool parse_body) {
    // Assumes 'fn' was already consumed.
    Location location = previous(p).location;

    ASTString name = TRY(ASTString, parse_identifier(p));
    Location name_loc = previous(p).location;

    TRY_CONSUME(p, TK_LPAREN);
    Array parameters; // Array<ASTObj *>
    arrayInit(&parameters);
    if(!parse_parameter_list(p, &parameters)) {
        arrayMap(&parameters, free_object_callback, NULL);
        arrayFree(&parameters);
        return NULL;
    }

    Type *return_type = p->program->primitives.void_;
    if(match(p, TK_ARROW)) {
        return_type = parse_type(p);
        if(!return_type) {
            arrayMap(&parameters, free_object_callback, NULL);
            arrayFree(&parameters);
            return NULL;
        }
    }
    location = locationMerge(location, previous(p).location);

    ASTObj *fn = astNewObj(OBJ_FN, location, name_loc, name, new_fn_type(p, return_type, parameters));
    fn->as.fn.return_type = return_type;
    arrayCopy(&fn->as.fn.parameters, &parameters);
    arrayFree(&parameters); // No need to free the contents of the array as they are owned by the fn now.
    add_function_to_current_scope(p, fn);

    if(!parse_body) {
        return fn;
    }
    if(!consume(p, TK_LBRACE)) {
        astObjFree(fn);
        return NULL;
    }
    ScopeID scope = enter_function(p, fn);
    // Add all parameters as locals to the function scope.
    for(usize i = 0; i < fn->as.fn.parameters.used; ++i) {
        add_variable_to_current_scope(p, ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i));
    }
    fn->as.fn.body = AS_LIST_NODE(parse_block(p, scope, parse_function_body));
    leave_function(p);
    if(!fn->as.fn.body) {
        astObjFree(fn);
        return NULL;
    }

    return fn;
}

static ASTObj *parse_extern_decl(Parser *p) {
    // Assumes 'extern' was already consumed.
    ASTObj *result = NULL;
    if(match(p, TK_FN)) {
        result = parse_function_decl(p, false);
        // Note: [result] was already added to the current scope by parse_function_decl().
        if(!consume(p, TK_SEMICOLON)) {
            astObjFree(result);
            return NULL;
        }
        ASTObj *extern_fn = astNewObj(OBJ_EXTERN_FN, result->location, result->name_location, result->name, result->data_type);
        arrayCopy(&extern_fn->as.extern_fn.parameters, &result->as.fn.parameters);
        arrayClear(&result->as.fn.parameters);
        extern_fn->as.extern_fn.return_type = result->as.fn.return_type;
        if(p->current.attribute) {
            extern_fn->as.extern_fn.source_attr = take_attribute(p);
        }
        astObjFree(result);
        result = extern_fn;
    } else {
        error_at(p, current(p).location, "Invalid extern declaration (expected 'fn').");
    }
    return result;
}

static Attribute *parse_attribute(Parser *p) {
    // Assumes '#' was already consumed.
    Location start = previous(p).location;
    TRY_CONSUME(p, TK_LBRACKET);

    ASTString name = TRY(ASTString, parse_identifier(p));
    Location name_loc = previous(p).location;
    Attribute *attr = NULL;
    if(stringEqual(name, "source")) {
        TRY_CONSUME(p, TK_LPAREN);
        TRY_CONSUME(p, TK_STRING_LITERAL);
        ASTNode *arg = TRY(ASTNode *, parse_string_literal_expr(p));
        TRY_CONSUME(p, TK_RPAREN);
        // The Location is constructed later once the parsing of the attribute is done.
        attr = attributeNew(ATTR_SOURCE, EMPTY_LOCATION());
        attr->as.source = AS_LITERAL_NODE(arg)->value.as.str;
    } else {
        error_at(p, name_loc, stringFormat("Invalid attribute '%s'.", name));
        return NULL;
    }

    if(!consume(p, TK_RBRACKET)) {
        attributeFree(attr);
        return NULL;
    }
    attr->location = locationMerge(start, previous(p).location);
    return attr;
}

#undef TRY_CONSUME
#undef TRY

// synchronize to declaration boundaries.
static void synchronize(Parser *p) {
    // Get rid of the current attribute.
    if(p->current.attribute) {
        attributeFree(take_attribute(p));
    }
    while(!is_eof(p)) {
        switch(current(p).type) {
            case TK_FN:
            case TK_VAR:
            case TK_EXTERN:
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
#define DEF(typename, type, name) {Type *ty; NEW0(ty); typeInit(ty, (type), astProgramAddString(prog, (name)), root_module->id); prog->primitives.typename = scopeAddType(root_module->module_scope, ty);}

    // FIXME: do we need to add the typenames to the string table (because they are string literals)?
    // NOTE: Update IS_PRIMITIVE() in Types.h when adding new primitives.
    DEF(void_, TY_VOID, "void");
    DEF(int32, TY_I32, "i32");
    DEF(uint32, TY_U32, "u32");
    DEF(str, TY_STR, "str");

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
    p->current.module = astProgramAddModule(prog, root_module);
    root_module->id = p->current.module;
    p->current.scope = astModuleGetModuleScopeID(root_module);
    init_primitive_types(prog, root_module);
    p->current.allocator = &root_module->ast_allocator.alloc;

    // TODO: move this to parse_module_body() when module suppport is added.
    while(!is_eof(p)) {
        if(match(p, TK_FN)) {
            ASTObj *fn = parse_function_decl(p, true);
            if(fn != NULL) {
                arrayPush(&root_module->module_scope->objects, (void *)fn);
                tableSet(&root_module->module_scope->functions, (void *)fn->name, (void *)fn);
            }
        } else if(match(p, TK_VAR)) {
            Location var_loc = previous(p).location;
            ASTNode *global = parse_variable_decl(p, true, true, &root_module->module_scope->objects, &var_loc);
            if(!consume(p, TK_SEMICOLON)) {
                continue;
            }
            if(global != NULL) {
                arrayPush(&root_module->globals, (void *)global);
                // The object will be the last element in the scope's object array.
                ASTObj *var = ARRAY_GET_AS(ASTObj *, &root_module->module_scope->objects, arrayLength(&root_module->module_scope->objects) - 1);
                tableSet(&root_module->module_scope->variables, (void *)var->name, (void *)var);
            }
        } else if(match(p, TK_STRUCT)) {
            ASTObj *structure = parse_struct_decl(p);
            if(structure != NULL) {
                arrayPush(&root_module->module_scope->objects, (void *)structure);
                tableSet(&root_module->module_scope->structures, (void *)structure->name, (void *)structure);
            }
        } else if(match(p, TK_EXTERN)) {
            ASTObj *extern_decl = parse_extern_decl(p);
            if(extern_decl != NULL) {
                arrayPush(&root_module->module_scope->objects, (void *)extern_decl);
                tableSet(&root_module->module_scope->functions, (void *)extern_decl->name, (void *)extern_decl);
            }
        } else if(match(p, TK_HASH)) {
            Attribute *attr = parse_attribute(p);
            if(attr) {
                if(p->current.attribute) {
                    error_at(p, attr->location, "Too many attributes.");
                    hint(p, p->current.attribute->location, "Previous attribute defined here.");
                    attributeFree(attr);
                } else {
                    store_attribute(p, attr);
                }
            }
        } else {
            error_at(p, current(p).location, stringFormat("Expected one of ['fn', 'var'], but got '%s'.", tokenTypeString(current(p).type)));
        }
        if(p->need_synchronize) {
            p->need_synchronize = false;
            synchronize(p);
        }
    }

    p->current.allocator = NULL;
    p->program = NULL;
    return !p->had_error;
}
