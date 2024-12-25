#include <stddef.h> // NULL
#include "common.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Error.h"
#include "Token.h"
#include "Ast/Ast.h"
#include "Parser.h"

static void parser_init_internal(Parser *p, Compiler *c, Scanner *s) {
    p->compiler = c;
    p->scanner = s;
    p->program = NULL;
    p->current.module = NULL;
    p->current.scope = NULL;
    p->state.current_token.type = TK_GARBAGE;
    p->state.previous_token.type = TK_GARBAGE;
    p->state.had_error = false;
    p->state.need_sync = false;
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
        //if(p->dump_tokens) { tokenPrint(stdout, &tk); putchar('\n'); }
        p->state.had_error = true;
    }
    //if(p->dump_tokens) { tokenPrint(stdout, &tk); putchar('\n'); }
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

static char *tmp_buffer_copy(Parser *p, char *s, u32 length) {
    if(stringLength(p->tmp_buffer) > 0) {
        stringClear(p->tmp_buffer);
    }
    stringAppend(&p->tmp_buffer, "%.*s", length, s);
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

static inline Allocator *getCurrentAllocator(Parser *p) {
    return &p->current.module->ast_allocator.alloc;
}

/** Expression Parser **/

typedef enum precedence {
    PREC_LOWEST     = 0,  // lowest
    PREC_ASSIGNMENT = 1,  // infix =
    PREC_BIT_OR     = 2,  // infix |
    PREC_BIT_XOR    = 3,  // infix ^
    PREC_BIT_AND    = 4,  // infix &
    PREC_EQUALITY   = 5,  // infix == !=
    PREC_COMPARISON = 6,  // infix > >= < <=
    PREC_BIT_SHIFT  = 7,  // infix >> <<
    PREC_TERM       = 8,  // infix + -
    PREC_FACTOR     = 9,  // infix * /
    PREC_UNARY      = 10, // unary + -
    PREC_CALL       = 11, // ()
    PREC_PRIMARY    = 12  // highest
} Precedence;

typedef ASTExprNode *(*PrefixParseFn)(Parser *p);
typedef ASTExprNode *(*InfixParseFn)(Parser *p, ASTExprNode *lhs);

typedef struct parse_rule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;

/* Expression parse functions pre-declarations (for parse table) */
static ASTExprNode *parse_number_literal_expr(Parser *p);

/* Precedence/parse rule table */

static ParseRule rules[] = {
    [TK_LPAREN]         = {NULL, NULL, PREC_LOWEST},
    [TK_RPAREN]         = {NULL, NULL, PREC_LOWEST},
    [TK_LBRACKET]       = {NULL, NULL, PREC_LOWEST},
    [TK_RBRACKET]       = {NULL, NULL, PREC_LOWEST},
    [TK_LBRACE]         = {NULL, NULL, PREC_LOWEST},
    [TK_RBRACE]         = {NULL, NULL, PREC_LOWEST},
    [TK_PLUS]           = {NULL, NULL, PREC_LOWEST},
    [TK_STAR]           = {NULL, NULL, PREC_LOWEST},
    [TK_SLASH]          = {NULL, NULL, PREC_LOWEST},
    [TK_SEMICOLON]      = {NULL, NULL, PREC_LOWEST},
    [TK_COLON]          = {NULL, NULL, PREC_LOWEST},
    [TK_COMMA]          = {NULL, NULL, PREC_LOWEST},
    [TK_DOT]            = {NULL, NULL, PREC_LOWEST},
    [TK_HASH]           = {NULL, NULL, PREC_LOWEST},
    [TK_AMPERSAND]      = {NULL, NULL, PREC_LOWEST},
    [TK_MINUS]          = {NULL, NULL, PREC_LOWEST},
    [TK_ARROW]          = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL]          = {NULL, NULL, PREC_LOWEST},
    [TK_EQUAL_EQUAL]    = {NULL, NULL, PREC_LOWEST},
    [TK_BANG]           = {NULL, NULL, PREC_LOWEST},
    [TK_BANG_EQUAL]     = {NULL, NULL, PREC_LOWEST},
    [TK_LESS]           = {NULL, NULL, PREC_LOWEST},
    [TK_LESS_EQUAL]     = {NULL, NULL, PREC_LOWEST},
    [TK_GREATER]        = {NULL, NULL, PREC_LOWEST},
    [TK_GREATER_EQUAL]  = {NULL, NULL, PREC_LOWEST},
    [TK_NUMBER_LITERAL] = {parse_number_literal_expr, NULL, PREC_LOWEST},
    [TK_STRING_LITERAL] = {NULL, NULL, PREC_LOWEST},
    [TK_IF]             = {NULL, NULL, PREC_LOWEST},
    [TK_ELSE]           = {NULL, NULL, PREC_LOWEST},
    [TK_WHILE]          = {NULL, NULL, PREC_LOWEST},
    [TK_FN]             = {NULL, NULL, PREC_LOWEST},
    [TK_RETURN]         = {NULL, NULL, PREC_LOWEST},
    [TK_VAR]            = {NULL, NULL, PREC_LOWEST},
    [TK_STRUCT]         = {NULL, NULL, PREC_LOWEST},
    [TK_EXTERN]         = {NULL, NULL, PREC_LOWEST},
    [TK_DEFER]          = {NULL, NULL, PREC_LOWEST},
    [TK_MODULE]         = {NULL, NULL, PREC_LOWEST},
    [TK_VOID]           = {NULL, NULL, PREC_LOWEST},
    [TK_I32]            = {NULL, NULL, PREC_LOWEST},
    [TK_U32]            = {NULL, NULL, PREC_LOWEST},
    [TK_STR]            = {NULL, NULL, PREC_LOWEST},
    [TK_IDENTIFIER]     = {NULL, NULL, PREC_LOWEST},
    [TK_GARBAGE]        = {NULL, NULL, PREC_LOWEST},
    [TK_EOF]            = {NULL, NULL, PREC_LOWEST}
};

static inline ParseRule *getRule(TokenType type) {
    return &rules[(int)type];
}

/* Parse functions */

static ASTExprNode *parse_number_literal_expr(Parser *p) {
    Location loc = previous(p).location;
    // TODO: Support hex, octal, and binary literals
    u64 value = strtoul(previous(p).lexeme, NULL, 10);
    // TODO: parse postfix types.
    ASTConstantValueExpr *n = astConstantValueExprNew(getCurrentAllocator(p), EXPR_NUMBER_CONSTANT, loc, NULL);
    n->as.number = value;
    return NODE_AS(ASTExprNode, n);
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
        Token tk = advance(p);
        InfixParseFn infix = getRule(tk.type)->infix;
        tree = TRY(ASTExprNode *, infix(p, tree)); // Again, we assume the error was already reported.
    }
    return tree;
}

static inline ASTExprNode *parseExpression(Parser *p) {
    return parsePrecedence(p, PREC_LOWEST);
}

/** Statement and declaration parser **/

static ASTString parseIdentifier(Parser *p) {
    TRY_CONSUME(p, TK_IDENTIFIER);
    Token idTk = previous(p);
    return stringTableFormat(&p->program->strings, "%.*s", idTk.length, idTk.lexeme);
}

// typed_var = identifier (':' type)+
static ASTObj *parseTypedVariable(Parser *p) {
    ASTString name = TRY(ASTString, parseIdentifier(p));
    Type *ty = NULL;
    if(match(p, TK_COLON)) {
        LOG_ERR("Cannot parse types yet!");
        UNREACHABLE();
    }
    return astObjectNew(OBJ_VAR, name, ty);
}

// var_decl -> 'var' typed_var ('=' expression)+ ';'
static ASTVarDeclStmt *parseVarDecl(Parser *p, bool allowInitializer) {
    // Note: do NOT add to scope. we only parse!
    // Assumes 'var' has already been consumed.
    Location start = previous(p).location;
    ASTObj *obj = parseTypedVariable(p);

    ASTExprNode *initializer = NULL;
    if(match(p, TK_EQUAL)) {
        if(!allowInitializer) {
            astObjectFree(obj);
            error(p, "Variable initializer not allowed in this context.");
            return NULL; // no need to free ASTStrings, owned by program instance.
        }
        initializer = parseExpression(p);
    }
    if(!consume(p, TK_SEMICOLON)) {
        // Note: can't use TRY_CONSUME() because we have to free the object on failure.
        // TODO: Maybe we should store all objects in an arena?
        astObjectFree(obj);
        return NULL;
    }

    Location loc = locationMerge(start, previous(p).location);
    return astVarDeclStmtNew(getCurrentAllocator(p), loc, obj, initializer);
}

// declaration -> var_decl | fn_decl | struct_decl | extern_decl
static ASTObj *parseDeclaration(Parser *p) {
    // Notes: * Add nothing to scope. we only parse!
    //        * No need to TRY() since if 'result' is NULL, we will return NULL.
    ASTObj *result = NULL;
    if(match(p, TK_FN)) {
        UNREACHABLE();
//
//    } else if(match(p, TK_STRUCT)) {

//    } else if(match(p, TK_EXTERN)) {
//
    } else {
        errorAt(p, current(p).location, "Expected declaration.");
    }

    return result;
}

// Sync to declaration boundaries
static void synchronizeToDecl(Parser *p) {
    while(!isEof(p)) {
        switch(current(p).type) {
            case TK_VAR:
            case TK_FN:
            case TK_STRUCT:
            case TK_EXTERN:
                return;
            default:
                break;
        }
        advance(p);
    }
}

static void import_primitive_types(ASTProgram *prog, ASTModule *module) {
    // We "import" the primitive types into each module.
    // In reality, we create new types each time, but since
    // primitives are equal by their TypeType, this doesn't matter.
#define DEF(type, name) {Type *ty = typeNew((type), stringTableString(&prog->strings, (name)), EMPTY_LOCATION, module); astModuleAddType(module, ty);}

    DEF(TY_VOID, "void");
    DEF(TY_I32, "i32");

#undef DEF
}

// Returns true on successful parse, or false on failure.
// module_body -> declaration*
static bool parseModuleBody(Parser *p, ASTString name) {
    ASTModule *module = astProgramNewModule(p->program, name);
    // If we are parsing a module body, there shouldn't be an existing current module.
    VERIFY(p->current.module == NULL);
    VERIFY(p->current.scope == NULL);
    p->current.module = module;
    p->current.scope = module->moduleScope;
    // Import all the primitive (builtin) types into the module.
    import_primitive_types(p->program, module);

    while(!isEof(p)) {
        if(match(p, TK_VAR)) {
            ASTVarDeclStmt *varDecl = parseVarDecl(p, true);
            if(varDecl) {
                scopeAddObject(p->current.scope, varDecl->variable);
                arrayPush(&p->current.module->variableDecls, (void *)varDecl);
            }
        } else {
            ASTObj *obj = parseDeclaration(p);
            if(obj) {
                scopeAddObject(p->current.scope, obj);
            }
        }

        if(p->state.need_sync) {
            p->state.need_sync = false;
            synchronizeToDecl(p);
        }
    }

    // FIXME: return false if any errors occured.
    return true;
}

bool parserParse(Parser *p, ASTProgram *prog) {
    UNUSED(tmp_buffer_copy);
    p->program = prog;

    // Get the first token.
    advance(p);
    // If the scanner failed to set the source file, we can't do anything.
    if(p->scanner->failed_to_set_source) {
        // The scanner has alreaedy reported the error.
        p->program = NULL;
        return false;
    }

    // TODO: create function parseModuleDecl() for this.
    // module_decl -> ('module' identifier ';')+
    if(match(p, TK_MODULE)) {
        Location loc = previous(p).location;
        TRY_CONSUME(p, TK_IDENTIFIER);
        TRY_CONSUME(p, TK_AMPERSAND);
        errorAt(p, loc, "Module declarations are not yet unsupported.");
        p->program = NULL;
        return false;
    }

    // The root module represents the top level (file) scope).
    if(!parseModuleBody(p, stringTableString(&prog->strings, "___root_module___"))) {
        // Errors have already been reported
        p->program = NULL;
        return false;
    }

    return true;
}
