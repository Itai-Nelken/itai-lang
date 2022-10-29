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
    p->current.module = 0;
    p->current.function = NULL;
    p->current.scope = NULL;
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
    p->current.module = 0;
    p->current.function = NULL;
    p->previous_token.type = TK_GARBAGE;
    p->current_token.type = TK_GARBAGE;
    p->had_error = false;
    p->need_synchronize = false;
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
        p->had_error = true;
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

static inline void enter_scope(Parser *p) {
    VERIFY(p->current.function);
    p->current.function->as.fn.scopes = blockScopeNew(p->current.function->as.fn.scopes);
    p->current.scope = p->current.function->as.fn.scopes;
}

static void leave_scope(Parser *p) {
    VERIFY(p->current.scope != NULL); // depth > 0 (global/file/module scope).
    p->current.scope = p->current.scope->parent;
}

static inline void add_local_to_current_scope(Parser *p, ASTObj *local) {
    VERIFY(p->current.scope != NULL);
    VERIFY(tableSet(&p->current.scope->visible_locals, (void *)local->as.var.name, (void *)local) == NULL);
}

static ASTObj *find_local(Parser *p, ASTString name) {
    VERIFY(p->current.scope != NULL);
    BlockScope *scope = p->current.scope;
    while(scope) {
        TableItem *i = tableGet(&scope->visible_locals, (void *)name);
        if(i) {
            return (ASTObj *)i->value;
        }
        scope = scope->parent;
    }
    return NULL;
}

static inline void enter_function(Parser *p, ASTObj *fn) {
    p->current.function = fn;
    enter_scope(p);
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

// DOES consume the '{'.
static ASTNode *parse_block(Parser *p, ASTNode *(*parse_callback)(Parser *p)) {
    TRY_CONSUME(p, TK_LBRACE, 0);
    ASTListNode *n = AS_LIST_NODE(astNewListNode(ND_BLOCK, locationNew(0, 0, 0)));
    Location start = previous(p).location;

    while(!is_eof(p) && current(p).type != TK_RBRACE) {
        ASTNode *node = parse_callback(p);
        if(node) {
            arrayPush(&n->nodes, (void *)node);
        } else {
            // synchronize to statement boundaries
            // (statements also include variable declarations in this context).
            while(!is_eof(p) && current(p).type != TK_RBRACE) {
                TokenType c = current(p).type;
                if(c == TK_VAR) {
                    break;
                } else {
                    advance(p);
                }
            }
        }
    }
    TRY_CONSUME(p, TK_RBRACE, AS_NODE(n));

    n->header.location = locationMerge(start, previous(p).location);
    return AS_NODE(n);
}

static ASTNode *parse_expression_stmt(Parser *p) {
    ASTNode *expr = TRY(ASTNode *, parse_expression(p), 0);
    TRY_CONSUME(p, TK_SEMICOLON, expr);
    return expr;
}

static ASTNode *parse_statement(Parser *p) {
    ASTNode *result = NULL;
    result = TRY(ASTNode *, parse_expression_stmt(p), 0);
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

// complex_type -> arrays, structs, enums, functions, custom types
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
    // NOTE: The object is being pushed to the array
    //       only AFTER all the parsing is done.
    //       This is so that "ghost" objects that belong to half-parsed
    //       variables won't be in the array and cause weird problems later on.
    arrayPush(obj_array, (void *)var);

    // includes everything from the 'var' to the ';'.
    Location full_loc = locationMerge(var_loc, previous(p).location);
    ASTNode *var_node = astNewObjNode(ND_VARIABLE, locationMerge(var_loc, name_loc), var);
    if(initializer != NULL) {
        return astNewBinaryNode(ND_ASSIGN, full_loc, var_node, initializer);
    }
    return var_node;
}

static ASTNode *parse_function_body(Parser *p) {
    VERIFY(p->current.function);

    ASTNode *result = NULL;
    if(match(p, TK_VAR)) {
        ASTNode *var_node = TRY(ASTNode *, parse_variable_decl(p, &p->current.function->as.fn.locals), 0);
        // Get the name of the variable to push to check
        // for redefinitions and to save in the current scope.
        ASTObj *var_obj = ARRAY_GET_AS(ASTObj *, &p->current.function->as.fn.locals, arrayLength(&p->current.function->as.fn.locals) - 1);
        VERIFY(var_obj->type == OBJ_VAR);
        ASTObj *existing_obj = find_local(p, var_obj->as.var.name);
        if(existing_obj) {
            // TODO: emit hint of previous declaration using 'exisiting_obj.location'.
            error(p, stringFormat("Redfinition of local variable '%s'.", var_obj->as.var.name));
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

// function_decl -> 'fn' identifier '(' parameter_list? ')' ('->' type)? block(fn_body)
static ASTObj *parse_function_decl(Parser *p) {
    // Assumes 'fn' was already consumed.
    Location location = previous(p).location;

    ASTString name = TRY(ASTString, parse_identifier(p), 0);

    TRY_CONSUME(p, TK_LPAREN, 0);
    // TODO: parameters.
    TRY_CONSUME(p, TK_RPAREN, 0);

    Type *return_type = NULL;
    if(match(p, TK_ARROW)) {
        return_type = TRY(Type *, parse_type(p), 0);
    }
    location = locationMerge(location, previous(p).location);

    ASTObj *fn = astNewObj(OBJ_FN, location);
    fn->as.fn.name = name;
    fn->as.fn.return_type = return_type;

    enter_function(p, fn);
    fn->as.fn.body = AS_LIST_NODE(parse_block(p, parse_function_body));
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
// TODO: if inside a function, synchronize to statement boundaries.
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
            ASTNode *global = parse_variable_decl(p, &root_module->objects);
            if(global != NULL) {
                arrayPush(&root_module->globals, (void *)global);
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
