#include <stdlib.h> // strtoul
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"
#include "Error.h"
#include "Ast.h"
#include "Scanner.h"
#include "Parser.h"

void parserInit(Parser *p, Scanner *s, Compiler *c) {
    p->compiler = c;
    p->scanner = s;
    p->program = NULL;
    p->dump_tokens = false;
    p->current.module = 0;
    p->current.function = NULL;
    p->current.scope = NULL;
    p->can_assign = false;
    // set current and previous tokens to TK_GARBAGE with empty locations
    // so errors can be reported using them.
    p->previous_token.type = TK_GARBAGE;
    p->previous_token.location = locationNew(0, 0, 0);
    p->current_token.type = TK_GARBAGE;
    p->current_token.location = locationNew(0, 0, 0);
    p->had_error = false;
    p->need_synchronize = false;
}

void parserFree(Parser *p) {
    p->compiler = NULL;
    p->scanner = NULL;
    p->program = NULL;
    p->dump_tokens = false;
    p->current.module = 0;
    p->current.function = NULL;
    p->can_assign = false;
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
static void add_error(Parser *p, bool has_location, Location loc, char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, has_location, loc, message);
    compilerAddError(p->compiler, err);
    if(stringIsValid(message)) {
        stringFree(message);
    }
    p->had_error = true;
    p->need_synchronize = true;
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

static inline ScopeID enter_scope(Parser *p) {
    VERIFY(p->current.function);
    if(p->current.scope == NULL) {
        // Function scope, depth always == 1.
        p->current.scope = p->current.function->as.fn.scopes = blockScopeNew(NULL, 1);
        // FIXME: how to represent function scope.
        return (ScopeID){.depth = 1, .index = 0};
    }
    // Block scope (meaning scopes inside a function scope).
    BlockScope *child = blockScopeNew(p->current.scope, p->current.scope->depth + 1);
    ScopeID id = blockScopeAddChild(p->current.scope, child);
    p->current.scope = child;
    return id;
}

static void leave_scope(Parser *p) {
    VERIFY(p->current.scope != NULL); // depth > 0 (global/file/module scope).
    p->current.scope = p->current.scope->parent;
}

static inline void add_local_to_current_scope(Parser *p, ASTObj *local) {
    VERIFY(p->current.scope != NULL);
    VERIFY(tableSet(&p->current.scope->visible_locals, (void *)local->name, (void *)local) == NULL);
}

static ASTObj *find_local_in_current_scope(Parser *p, ASTString name) {
    VERIFY(p->current.scope != NULL);
    TableItem *i = tableGet(&p->current.scope->visible_locals, (void *)name);
    if(i) {
        return (ASTObj *)i->value;
    }
    return NULL;
}

static inline ScopeID enter_function(Parser *p, ASTObj *fn) {
    p->current.function = fn;
    return enter_scope(p);
}

static inline void leave_function(Parser *p) {
    p->current.function = NULL;
    leave_scope(p);
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
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;

/* Parse functions forward-declarations */
static ASTNode *parse_precedence(Parser *p, Precedence min_prec);
static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_number_literal_expr(Parser *p);
static ASTNode *parse_grouping_expr(Parser *p);
static ASTNode *parse_identifier_expr(Parser *p);
static ASTNode *parse_unary_expr(Parser *p);
static ASTNode *parse_binary_expr(Parser *p, ASTNode *lhs);
static ASTNode *parse_call_expr(Parser *p, ASTNode *callee);
static ASTNode *parse_property_access_expr(Parser *p, ASTNode *lhs);

static ParseRule rules[] = {
    [TK_LPAREN]        = {parse_grouping_expr, parse_call_expr, PREC_CALL},
    [TK_RPAREN]        = {NULL, NULL, PREC_LOWEST},
    [TK_LBRACE]        = {NULL, NULL, PREC_LOWEST},
    [TK_RBRACE]        = {NULL, NULL, PREC_LOWEST},
    [TK_PLUS]          = {parse_unary_expr, parse_binary_expr, PREC_TERM},
    [TK_STAR]          = {NULL, parse_binary_expr, PREC_FACTOR},
    [TK_SLASH]         = {NULL, parse_binary_expr, PREC_FACTOR},
    [TK_SEMICOLON]     = {NULL, NULL, PREC_LOWEST},
    [TK_COLON]         = {NULL, NULL, PREC_LOWEST},
    [TK_COMMA]         = {NULL, NULL, PREC_LOWEST},
    [TK_DOT]           = {NULL, parse_property_access_expr, PREC_CALL},
    [TK_MINUS]         = {parse_unary_expr, parse_binary_expr, PREC_TERM},
    [TK_ARROW]         = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL]         = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL_EQUAL]   = {NULL, parse_binary_expr, PREC_EQUALITY},
    [TK_BANG]          = {NULL, NULL, PREC_LOWEST},
    [TK_BANG_EQUAL]    = {NULL, parse_binary_expr, PREC_EQUALITY},
    [TK_LESS]          = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_LESS_EQUAL]    = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_GREATER]       = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_GREATER_EQUAL] = {NULL, parse_binary_expr, PREC_COMPARISON},
    [TK_NUMBER]        = {parse_number_literal_expr, NULL, PREC_LOWEST},
    [TK_IF]            = {NULL, NULL, PREC_LOWEST},
    [TK_ELSE]          = {NULL, NULL, PREC_LOWEST},
    [TK_WHILE]         = {NULL, NULL, PREC_LOWEST},
    [TK_FN]            = {NULL, NULL, PREC_LOWEST},
    [TK_RETURN]        = {NULL, NULL, PREC_LOWEST},
    [TK_VAR]           = {NULL, NULL, PREC_LOWEST},
    [TK_STRUCT]        = {NULL, NULL, PREC_LOWEST},
    [TK_I32]           = {NULL, NULL, PREC_LOWEST},
    [TK_U32]           = {NULL, NULL, PREC_LOWEST},
    [TK_IDENTIFIER]    = {parse_identifier_expr, NULL, PREC_LOWEST},
    [TK_GARBAGE]       = {NULL, NULL, PREC_LOWEST},
    [TK_EOF]           = {NULL, NULL, PREC_LOWEST}
};
_Static_assert(sizeof(rules)/sizeof(rules[0]) == TK_TYPE_COUNT, "Missing token type(s) in parser rule table!");

static ParseRule *get_rule(TokenType type) {
    return &rules[type];
}

static ASTString parse_identifier(Parser *p) {
    TRY_CONSUME(p, TK_IDENTIFIER, 0);
    return astProgramAddString(p->program, stringNCopy(previous(p).lexeme, previous(p).length));
}

static ASTNode *parse_identifier_expr(Parser *p) {
    ASTString str = astProgramAddString(p->program, stringNCopy(previous(p).lexeme, previous(p).length));
    ASTNode *id_node = astNewIdentifierNode(previous(p).location, str);
    if(p->can_assign && match(p, TK_EQUAL)) {
        ASTNode *rhs = TRY(ASTNode *, parse_expression(p), id_node);
        return astNewBinaryNode(ND_ASSIGN, locationMerge(id_node->location, rhs->location), id_node, rhs);
    }
    return id_node;
}

static ASTNode *parse_number_literal_expr(Parser *p) {
    // TODO: Support hex, octal & binary.
    u64 value = strtoul(previous(p).lexeme, NULL, 10);
    // TODO: parse postfix typese (e.g. 123u32)
    return astNewLiteralValueNode(ND_NUMBER_LITERAL, previous(p).location, LITERAL_VALUE(LIT_NUMBER, number, value));
}

static ASTNode *parse_grouping_expr(Parser *p) {
    ASTNode *expr = TRY(ASTNode *, parse_expression(p), 0);
    TRY_CONSUME(p, TK_RPAREN, expr);
    return expr;
}

static ASTNode *parse_unary_expr(Parser *p) {
    Token operator = previous(p);
    ASTNode *operand = TRY(ASTNode *, parse_precedence(p, PREC_UNARY), 0);

    switch(operator.type) {
        case TK_PLUS:
            return operand;
        case TK_MINUS:
            return astNewUnaryNode(ND_NEGATE, locationMerge(operator.location, operand->location), operand);
        default: UNREACHABLE();
    }
}

static ASTNode *parse_binary_expr(Parser *p, ASTNode *lhs) {
    TokenType op = previous(p).type;
    ParseRule *rule = get_rule(op);
    ASTNode *rhs = TRY(ASTNode *, parse_precedence(p, rule->precedence), lhs);

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
    return astNewBinaryNode(node_type, locationMerge(lhs->location, rhs->location), lhs, rhs);
}

static ASTNode *parse_call_expr(Parser *p, ASTNode *callee) {
    // FIXME:How to represent an empty ScopeID?
    ASTListNode *arguments = AS_LIST_NODE(astNewListNode(ND_ARGS, current(p).location, (ScopeID){0, 0}));
    if(current(p).type != TK_RPAREN) {
        do {
            ASTNode *arg = parse_expression(p);
            if(!arg) {
                continue;
            }
            arrayPush(&arguments->nodes, (void *)arg);
            arguments->header.location = locationMerge(arguments->header.location, arg->location);
        } while(match(p, TK_COMMA));
    }
    TRY_CONSUME(p, TK_RPAREN, callee);
    return astNewBinaryNode(ND_CALL, locationMerge(callee->location, previous(p).location), callee, AS_NODE(arguments));
}

static ASTNode *parse_property_access_expr(Parser *p, ASTNode *lhs) {
    ASTString property_name = TRY(ASTString, parse_identifier(p), lhs);
    Location property_name_loc = previous(p).location;

    ASTNode *n = astNewBinaryNode(ND_PROPERTY_ACCESS, locationMerge(lhs->location, property_name_loc), lhs, astNewIdentifierNode(property_name_loc, property_name));
    if(p->can_assign && match(p, TK_EQUAL)) {
        ASTNode *value = TRY(ASTNode *, parse_expression(p), n);
        n = astNewBinaryNode(ND_ASSIGN, locationMerge(n->location, previous(p).location), n, value);
    }
    return n;
}

static ASTNode *parse_precedence(Parser *p, Precedence min_prec) {
    advance(p);
    PrefixParseFn prefix = get_rule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, stringFormat("Expected an expression but got '%s'.", tokenTypeString(previous(p).type)));
        return NULL;
    }
    bool old_can_assign = p->can_assign;
    p->can_assign = min_prec <= PREC_ASSIGNMENT;
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
            // tree is already NULL, so we will return NULL after the label.
            goto parse_precedence_end;
        }
    }

    if(p->can_assign && match(p, TK_EQUAL)) {
        error_at(p, tree->location, "Invalid assignmet target.");
        astNodeFree(tree);
        // Don't return here as we need to restore 'can_assign'.
        tree = NULL;
    }
parse_precedence_end:
    p->can_assign = old_can_assign;

    return tree;
}

static inline ASTNode *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_LOWEST);
}

static ControlFlow statement_control_flow(ASTNode *stmt) {
    switch(stmt->node_type) {
        case ND_RETURN:
            return CF_ALWAYS_RETURNS;
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
        case ND_BLOCK: return AS_LIST_NODE(stmt)->control_flow;
        default:
            break;
    }
    return CF_NEVER_RETURNS;
}

static ASTNode *parse_block(Parser *p, ScopeID scope, ASTNode *(*parse_callback)(Parser *p)) {
    // Assume '{' was already consumed.
    ASTListNode *n = AS_LIST_NODE(astNewListNode(ND_BLOCK, locationNew(0, 0, 0), scope));
    Location start = previous(p).location;
    ControlFlow cf = CF_NEVER_RETURNS;

    while(!is_eof(p) && current(p).type != TK_RBRACE) {
        ASTNode *node = parse_callback(p);
        if(node) {
            arrayPush(&n->nodes, (void *)node);
            cf = controlFlowUpdate(cf, statement_control_flow(node));
        } else {
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
    }
    TRY_CONSUME(p, TK_RBRACE, AS_NODE(n));

    n->header.location = locationMerge(start, previous(p).location);
    n->control_flow = cf;
    return AS_NODE(n);
}

static ASTNode *parse_expression_stmt(Parser *p) {
    ASTNode *expr = TRY(ASTNode *, parse_expression(p), 0);
    TRY_CONSUME(p, TK_SEMICOLON, expr);
    return expr;
}

static ASTNode *parse_return_stmt(Parser *p) {
    // Assume 'return' is already consumed.
    Location start = previous(p).location;
    ASTNode *operand = NULL;
    if(current(p).type != TK_SEMICOLON) {
        operand = TRY(ASTNode *, parse_expression(p), 0);
    }
    TRY_CONSUME(p, TK_SEMICOLON, operand);

    return astNewUnaryNode(ND_RETURN, locationMerge(start, previous(p).location), operand);
}

static ASTNode *parse_function_body(Parser *p);
static ASTNode *parse_if_stmt(Parser *p) {
    // Assume 'if' is already consumed.
    Location start = previous(p).location;
    ASTNode *condition = TRY(ASTNode *, parse_expression(p), 0);
    TRY_CONSUME(p, TK_LBRACE, condition);
    ScopeID scope = enter_scope(p);
    ASTNode *body = TRY(ASTNode *, parse_block(p, scope, parse_function_body), condition);
    leave_scope(p);

    ASTNode *else_ = NULL;
    if(match(p, TK_ELSE)) {
        if(match(p, TK_IF)) {
            if((else_ = parse_if_stmt(p)) == NULL) {
                astNodeFree(condition);
                astNodeFree(body);
                return NULL;
            }
        } else {
            if(!consume(p, TK_LBRACE)) {
                astNodeFree(condition);
                astNodeFree(body);
                return NULL;
            }
            scope = enter_scope(p);
            if((else_ = parse_block(p, scope, parse_function_body)) == NULL) {
                astNodeFree(condition);
                astNodeFree(body);
                return NULL;
            }
            leave_scope(p);
        }
    }

    return astNewConditionalNode(ND_IF, locationMerge(start, previous(p).location), condition, body, else_);
}

static ASTNode *parse_while_loop_stmt(Parser *p) {
    // Assume 'while' is already consumed.
    Location start = previous(p).location;
    ASTNode *condition = TRY(ASTNode *, parse_expression(p), 0);
    TRY_CONSUME(p, TK_LBRACE, condition);
    ScopeID scope = enter_scope(p);
    ASTNode *body = TRY(ASTNode *, parse_block(p, scope, parse_function_body), condition);
    leave_scope(p);
    return astNewLoopNode(locationMerge(start, previous(p).location), NULL, condition, NULL, body);
}

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
    } else {
        result = parse_expression_stmt(p);
    }
    return result;
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

// parameters: Array<ASTObj *> (OBJ_VAR)
static Type *new_fn_type(Parser *p, Type *return_type, Array parameters) {
    Type *ty;
    NEW0(ty);
    // FIXME: What should be the size of a function type?
    //        zero because functions cannot be stored (copied/cloned)?
    //        but what about closures/lambdas? the size of the implementing struct?
    typeInit(ty, TY_FN, NULL, 0);
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

    return astModuleAddType(astProgramGetModule(p->program, p->current.module), ty);
}

// fields: Array<ASTObj *> (OBJ_VAR)
static Type *new_struct_type(Parser *p, ASTString name, Array fields) {
    Type *ty;
    NEW0(ty);
    // FIXME: calculate the actual size of the struct.
    typeInit(ty, TY_STRUCT, name, 0);
    Array *field_types = &ty->as.structure.field_types;
    for(usize i = 0; i < fields.used; ++i) {
        ASTObj *member = ARRAY_GET_AS(ASTObj *, &fields, i);
        arrayPush(field_types, (void *)member->data_type);
    }

    return astModuleAddType(astProgramGetModule(p->program, p->current.module), ty);
}

static Type *parse_type(Parser *p);

// function_type -> 'fn' '(' (type)* ')' ('->' type)?
static Type *parse_function_type(Parser *p) {
    // Assume 'fn' was already consumed.
    Location start = previous(p).location;
    Type *ty = NULL;

    TRY_CONSUME(p, TK_LPAREN, 0);
    Array parameters; // Array<ASTObj *>
    arrayInit(&parameters);
    if(current(p).type != TK_RPAREN) {
        do {
            Type *ty = parse_type(p);
            if(!ty) {
                continue;
            }
            // Wrap the types in an ASTObj because of new_fn_type().
            arrayPush(&parameters, (void *)astNewObj(OBJ_VAR, locationNew(0, 0, 0), "", ty));
        } while(match(p, TK_COMMA));
    }
    if(!consume(p, TK_RPAREN)) {
        goto free_param_array;
    }
    Type *return_type = NULL;
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
    ASTString name = TRY(ASTString, parse_identifier(p), 0);
    Location loc = previous(p).location;
    Type *ty;
    NEW0(ty);
    typeInit(ty, TY_ID, name, 0);
    ty->decl_location = loc;
    return astModuleAddType(astProgramGetModule(p->program, p->current.module), ty);
}

// complex_type -> fn_type | identifier_type
static Type *parse_complex_type(Parser *p) {
    if(match(p, TK_FN)) {
        return TRY(Type *, parse_function_type(p), 0);
    } else if(current(p).type == TK_IDENTIFIER) {
        return TRY(Type *, parse_type_from_identifier(p), 0);
    }
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
// NOTE: the semicolon at the end isn't consumed.
static ASTNode *parse_variable_decl(Parser *p, bool allow_initializer, Array *obj_array) {
    // Assumes 'var' was already consumed.

    ASTString name = TRY(ASTString, parse_identifier(p), 0);
    Location name_loc = previous(p).location;

    Type *type = NULL;
    if(match(p, TK_COLON)) {
        type = TRY(Type *, parse_type(p), 0);
    }

    ASTNode *initializer = NULL;
    if(allow_initializer && match(p, TK_EQUAL)) {
        initializer = TRY(ASTNode *, parse_expression(p), 0);
    }

    ASTObj *var = astNewObj(OBJ_VAR, name_loc, name, type);

    // Add the variable object.
    // NOTE: The object is being pushed to the array
    //       only AFTER all the parsing is done.
    //       This is so that "ghost" objects that belong to half-parsed
    //       variables won't be in the array and cause weird problems later on.
    arrayPush(obj_array, (void *)var);

    // includes everything from the 'var' to the ';'.
    Location full_loc = locationMerge(name_loc, previous(p).location);
    ASTNode *var_node = astNewObjNode(ND_VARIABLE, name_loc, var);
    if(initializer != NULL) {
        return astNewBinaryNode(ND_ASSIGN, full_loc, var_node, initializer);
    }
    return var_node;
}

// struct_decl -> 'struct' identifier '{' (identifier: type ';')* '}'
static ASTObj *parse_struct_decl(Parser *p) {
    // Assumes 'struct' was already consumed.

    ASTString name = TRY(ASTString, parse_identifier(p), 0);
    Location name_loc = previous(p).location;

    ASTObj *structure = astNewObj(OBJ_STRUCT, name_loc, name, NULL);

    if(!consume(p, TK_LBRACE)) {
        astObjFree(structure);
        return NULL;
    }
    while(current(p).type != TK_RBRACE) {
        ASTNode *member = parse_variable_decl(p, false, &structure->as.structure.fields);
        astNodeFree(member);
        consume(p, TK_SEMICOLON);
    }
    if(!consume(p, TK_RBRACE)) {
        astObjFree(structure);
        return NULL;
    }

    structure->data_type = new_struct_type(p,structure->name, structure->as.structure.fields);

    structure->location = locationMerge(name_loc, previous(p).location);
    return structure;
}

static ASTNode *parse_function_body(Parser *p) {
    VERIFY(p->current.function);

    ASTNode *result = NULL;
    if(match(p, TK_VAR)) {
        ASTNode *var_node = TRY(ASTNode *, parse_variable_decl(p, true, &p->current.function->as.fn.locals), 0);
        TRY_CONSUME(p, TK_SEMICOLON, var_node);
        // Get the name of the variable to push to check
        // for redefinitions and to save in the current scope.
        ASTObj *var_obj = ARRAY_GET_AS(ASTObj *, &p->current.function->as.fn.locals, arrayLength(&p->current.function->as.fn.locals) - 1);
        VERIFY(var_obj->type == OBJ_VAR);
        ASTObj *existing_obj = find_local_in_current_scope(p, var_obj->name);
        if(existing_obj) {
            // TODO: emit hint of previous declaration using 'exisiting_obj.location'.
            error_at(p, var_node->location, stringFormat("Redfinition of local variable '%s'.", var_obj->name));
            astNodeFree(var_node);
            // NOTE: arrayPop() is used even though we already have a reference
            //       to the object we want to free because we also want to remove
            //       the object from the array.
            astObjFree((ASTObj *)arrayPop(&p->current.function->as.fn.locals));
            return NULL;
        }
        add_local_to_current_scope(p, var_obj);
        result = var_node;
    } else {
        // no need for TRY() here as nothing is done with the result node
        // other than return it.
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
        ASTNode *param_var = parse_variable_decl(p, false, parameters);
        if(!param_var) {
            had_error = true;
        }
        astNodeFree(param_var);
    } while(match(p, TK_COMMA));
    if(!consume(p, TK_RPAREN)) {
        return false;
    }
    return !had_error;
}

// function_decl -> 'fn' identifier '(' parameter_list? ')' ('->' type)? block(fn_body)
static ASTObj *parse_function_decl(Parser *p) {
    // Assumes 'fn' was already consumed.
    Location location = previous(p).location;

    ASTString name = TRY(ASTString, parse_identifier(p), 0);

    Array parameters; // Array<ASTObj *>
    arrayInit(&parameters);
    TRY_CONSUME(p, TK_LPAREN, 0);
    if(!parse_parameter_list(p, &parameters)) {
        arrayMap(&parameters, free_object_callback, NULL);
        arrayFree(&parameters);
        return NULL;
    }

    Type *return_type = NULL;
    if(match(p, TK_ARROW)) {
        return_type = parse_type(p);
        if(!return_type) {
            arrayMap(&parameters, free_object_callback, NULL);
            arrayFree(&parameters);
            return NULL;
        }
    }
    location = locationMerge(location, previous(p).location);

    ASTObj *fn = astNewObj(OBJ_FN, location, name, new_fn_type(p, return_type, parameters));
    fn->as.fn.return_type = return_type;
    arrayCopy(&fn->as.fn.parameters, &parameters);
    arrayFree(&parameters); // No need to free the contens of the array as they are owned by the fn now.

    if(!consume(p, TK_LBRACE)) {
        astObjFree(fn);
        return NULL;
    }
    ScopeID scope = enter_function(p, fn);
    // Add all parameters as locals.
    for(usize i = 0; i < fn->as.fn.parameters.used; ++i) {
        add_local_to_current_scope(p, ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i));
    }
    fn->as.fn.body = AS_LIST_NODE(parse_block(p, scope, parse_function_body));
    leave_function(p);
    if(!fn->as.fn.body) {
        astObjFree(fn);
        return NULL;
    }

    return fn;
}

#undef TRY_CONSUME
#undef TRY

// synchronize to declaration boundaries.
static void synchronize(Parser *p) {
    while(!is_eof(p)) {
        switch(current(p).type) {
            case TK_FN:
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

    // NOTE: Update IS_PRIMITIVE() in Types.h when adding new primitives.
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
    p->current.module = astProgramAddModule(prog, root_module);
    init_primitive_types(prog, root_module);

    while(!is_eof(p)) {
        if(match(p, TK_FN)) {
            ASTObj *fn = parse_function_decl(p);
            if(fn != NULL) {
                arrayPush(&root_module->objects, (void *)fn);
            }
        } else if(match(p, TK_VAR)) {
            ASTNode *global = parse_variable_decl(p, true, &root_module->objects);
            if(!consume(p, TK_SEMICOLON)) {
                astNodeFree(global);
                continue;
            }
            if(global != NULL) {
                arrayPush(&root_module->globals, (void *)global);
            }
        } else if(match(p, TK_STRUCT)) {
            ASTObj *structure = parse_struct_decl(p);
            if(structure != NULL) {
                arrayPush(&root_module->objects, (void *)structure);
            }
        } else {
            error_at(p, current(p).location, stringFormat("Expected one of ['fn', 'var'], but got '%s'.", tokenTypeString(current(p).type)));
        }
        if(p->need_synchronize) {
            p->need_synchronize = false;
            synchronize(p);
        }
    }

    p->program = NULL;
    return !p->had_error;
}
