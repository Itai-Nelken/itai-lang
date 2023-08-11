#include <stddef.h> // NULL
#include <stdbool.h>
#include <string.h> // memset
#include <stdarg.h>
#include "common.h"
#include "Error.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Ast/ParsedAst.h"
#include "Token.h"
#include "Types/ParsedType.h"
#include "Parser.h"


// For use for block scope representation only.
#define FUNCTION_SCOPE_DEPTH 1

void parserInit(Parser *p, Compiler *c, Scanner *s) {
    memset(p, 0, sizeof(*p));
    p->compiler = c;
    p->scanner = s;
    p->current_token.type = TK_GARBAGE;
    p->current_token.location = EMPTY_LOCATION;
    p->previous_token.type = TK_GARBAGE;
    p->previous_token.location = EMPTY_LOCATION;
}

void parserFree(Parser *p) {
    if(p->tmp_buffer != NULL) {
        stringFree(p->tmp_buffer);
    }
    parserInit(p, NULL, NULL);
}

void parserSetDumpTokens(Parser *p, bool value) {
    p->dump_tokens = value;
}


/* Parser helpers */

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

static char *tmp_buffer_format(Parser *p, const char *format, ...) {
    if(p->tmp_buffer) {
        stringClear(p->tmp_buffer);
    }
    va_list ap;
    va_start(ap, format);
    stringVAppend(&p->tmp_buffer, format, ap);
    va_end(ap);
    return (char *)p->tmp_buffer;
}

static char *tmp_buffer_copy(Parser *p, char *s, uint32_t length) {
    if(p->tmp_buffer) {
        stringClear(p->tmp_buffer);
    }
    stringAppend(&p->tmp_buffer, "%.*s", length, s);
    return (char *)p->tmp_buffer;
}

// Note: ownership of [message] is taken.
static void add_error(Parser *p, bool has_location, Location loc, ErrorType type, String message) {
    Error *err;
    NEW0(err);
    errorInit(err, type, has_location, loc, message);
    compilerAddError(p->compiler, err);
    stringFree(message);
    p->had_error = true;
    p->need_synchronize = true;
}

// Note: ownership of [message] is taken.
static inline void error_at(Parser *p, Location loc, String message) {
    add_error(p, true, loc, ERR_ERROR, message);
}

// Note: ownership of [message] is taken.
// The previous token's location is used.
static inline void error(Parser *p, String message) {
    error_at(p, previous(p).location, message);
}

// NOTES: If 'message' is a valid String, it will be freed.
static inline void hint(Parser *p, Location loc, String message) {
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
    ASTParsedModule *module = astParsedProgramGetModule(p->program, p->current.module);
    ParsedScope *parent = astParsedModuleGetScope(module, p->current.scope);
    // FIXME: A child scope may be a block scope even if the parent isn't.
    ParsedScope *child = parsedScopeNew(p->current.scope, parent->is_block_scope);
    ScopeID child_id = astParsedModuleAddScope(module, child);
    parsedScopeAddChild(parent, child_id);
    p->current.scope = child_id;
    if(child->is_block_scope) {
        p->current.block_scope_depth++;
        //TODO (note): is this needed? child->depth = p->current.block_scope_depth;
    }
    return child_id;
}

static void leave_scope(Parser *p) {
    ASTParsedModule *module = astParsedProgramGetModule(p->program, p->current.module);
    p->current.scope = astParsedModuleGetScope(module, p->current.scope)->parent;
    if(p->current.block_scope_depth >= FUNCTION_SCOPE_DEPTH) {
        p->current.block_scope_depth--;
    }
}

static inline void add_variable_to_current_scope(Parser *p, ASTParsedObj *var) {
    ParsedScope *scope = astParsedModuleGetScope(astParsedProgramGetModule(p->program, p->current.module), p->current.scope);
    ASTParsedObj *prev = (ASTParsedObj *)tableSet(&scope->variables, (void *)var->name.data, (void *)var);
    if(prev != NULL) {
        error_at(p, var->name.location, stringFormat("Redefinition of variable '%s'.", var->name));
        hint(p, prev->name.location, "Previous definition was here.");
    }
}


/* Helper macros */

#define TRY(type, result) ({ \
 type _tmp = (result);\
 if(!_tmp) { \
    return NULL; \
 } \
_tmp;})

#define TRY_CONSUME(parser, expected) TRY(bool, consume(parser, expected))


/* Expression Parser */

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

typedef ASTParsedExprNode *(*PrefixParseFn)(Parser *p);
typedef ASTParsedExprNode *(*InfixParseFn)(Parser *p, ASTParsedExprNode *lhs);

typedef struct parse_rule {
    PrefixParseFn prefix;
    InfixParseFn infix;
    Precedence precedence;
} ParseRule;


/** Parse functions pre-declarations **/

static ASTParsedExprNode *parse_identifier_expr(Parser *p);


/** Parse rule table **/

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
    [TK_NUMBER_LITERAL] = {NULL, NULL, PREC_LOWEST},
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
    [TK_VOID]           = {NULL, NULL, PREC_LOWEST},
    [TK_I32]            = {NULL, NULL, PREC_LOWEST},
    [TK_U32]            = {NULL, NULL, PREC_LOWEST},
    [TK_STR]            = {NULL, NULL, PREC_LOWEST},
    [TK_IDENTIFIER]     = {parse_identifier_expr, NULL, PREC_LOWEST},
    [TK_GARBAGE]        = {NULL, NULL, PREC_LOWEST},
    [TK_EOF]            = {NULL, NULL, PREC_LOWEST}
};

static ParseRule *get_rule(TokenType type) {
    return &rules[type];
}

/** Parse functions **/

static ASTInternedString parse_identifier(Parser *p) {
    TRY_CONSUME(p, TK_IDENTIFIER);
    return astParsedProgramAddString(p->program, tmp_buffer_copy(p, previous(p).lexeme, previous(p).length));
}

static ASTParsedExprNode *parse_identifier_expr(Parser *p) {
    Token prev = previous(p);
    ASTInternedString str = astParsedProgramAddString(p->program, stringNCopy(prev.lexeme, prev.length));
    return astNewParsedIdentifierExpr(p->current.allocator, prev.location, AST_STRING(str, prev.location));
}


static ASTParsedExprNode *parse_precedence(Parser *p, Precedence min_prec) {
    advance(p);
    PrefixParseFn prefix = get_rule(previous(p).type)->prefix;
    if(prefix == NULL) {
        error(p, stringFormat("Expected an expression but got '%s'.", tokenTypeString(previous(p).type)));
        return NULL;
    }
    ASTParsedExprNode *tree = prefix(p);
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

static inline ASTParsedExprNode *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_LOWEST);
}


/* Type parser */

// FIXME: when adding parse_function_type, the function obj is needed which is bad as we don't always have it...)
static ParsedType *parse_function_type(Parser *p) {
    UNUSED(p);
    LOG_ERR("parse_function_type() is unimplemented!");
    UNREACHABLE();
}

// identifier_type -> identifier (The identifier has to be a type of a struct, enum, or type alias).
static ParsedType *parse_type_from_identifier(Parser *p) {
    ASTInternedString name = TRY(ASTInternedString, parse_identifier(p));
    Location loc = previous(p).location;
    ParsedType *ty;
    NEW0(ty);
    parsedTypeInit(ty, TY_ID, AST_STRING(name, loc), p->current.module);
    ty->decl_location = loc;
    return parsedScopeAddType(astParsedProgramGetModule(p->program, p->current.module)->module_scope, ty);
}

// complex_type -> fn_type | identifier_type
static ParsedType *parse_complex_type(Parser *p) {
    if(match(p, TK_FN)) {
        return TRY(ParsedType *, parse_function_type(p));
    } else if(current(p).type == TK_IDENTIFIER) {
        return TRY(ParsedType *, parse_type_from_identifier(p));
    }
    error_at(p, current(p).location, "Expected typename.");
    return NULL;
}

// primitive_type -> i32 | u32
static ParsedType *parse_primitive_type(Parser *p) {
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

// type -> primitive_type | complex_type
static ParsedType *parse_type(Parser *p) {
    bool is_pointer = false;
    if(match(p, TK_AMPERSAND)) {
        is_pointer = true;
    }
    ParsedType *ty = parse_primitive_type(p);
    if(!ty) {
        ty = TRY(ParsedType *, parse_complex_type(p));
    }
    if(is_pointer) {
        ASTInternedString ptr_name = astParsedProgramAddString(p->program, tmp_buffer_format(p, "&%s", ty->name.data));
        ParsedType *ptr;
        NEW0(ptr);
        // FIXME: Add ty->name.location?
        parsedTypeInit(ptr, TY_PTR, AST_STRING(ptr_name, EMPTY_LOCATION), p->current.module);
        ptr->as.ptr.inner_type = ty;
        ty = parsedScopeAddType(astParsedProgramGetModule(p->program, p->current.module)->module_scope, ptr);
    }
    return ty;
}


/* Statement & Declaration parser */

// Notes: 1) The semicolon at the end isn't consumed.
//        2) [var_loc] may be NULL.
// variable_decl -> 'var' identifier (':' type)? ('=' expression)? ';'
static ASTParsedStmtNode *parse_variable_decl(Parser *p, bool allow_initializer, bool add_to_scope, Array *obj_array, Location *var_loc) {
    // Assumes 'var' was already consumed.

    ASTInternedString name_str = TRY(ASTInternedString, parse_identifier(p));
    Location name_loc = previous(p).location;
    ASTString name = AST_STRING(name_str, name_loc);

    // For variables, the type is NULL until it is parsed or infered.
    ParsedType *type = NULL;
    if(match(p, TK_COLON)) {
        type = TRY(ParsedType *, parse_type(p));
    }

    ASTParsedExprNode *initializer = NULL;
    if(allow_initializer && match(p, TK_EQUAL)) {
        initializer = TRY(ASTParsedExprNode *, parse_expression(p));
    }

    ASTParsedObj *var = astNewParsedObj(OBJ_VAR, name_loc, name, type);

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
    return astNewParsedVarDeclStmt(p->current.allocator, full_loc, var, initializer);
}

#undef TRY_CONSUME
#undef TRY

// synchronize to declaration boundaries.
static void synchronize(Parser *p) {
    // TODO: add back: Get rid of the current attribute.
    //if(p->current.attribute) {
    //    attributeFree(take_attribute(p));
    //}
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

static void add_primitive_types(ASTParsedProgram *prog, ASTParsedModule *root_module) {
#define DEF(typename, type, name) {ParsedType *ty; NEW0(ty); parsedTypeInit(ty, (type), AST_STRING(astParsedProgramAddString(prog, (name)), EMPTY_LOCATION), root_module->id); prog->primitives.typename = parsedScopeAddType(root_module->module_scope, ty);}

    // FIXME: do we need to add the typenames to the string table (because they are string literals)?
    // NOTE: Update IS_PRIMITIVE() in Types.h when adding new primitives.
    DEF(void_, TY_VOID, "void");
    DEF(int32, TY_I32, "i32");
    DEF(uint32, TY_U32, "u32");
    DEF(str, TY_STR, "str");

#undef DEF
}

bool parserParse(Parser *p, ASTParsedProgram *prog) {
    p->program = prog;

    // Get the first token.
    advance(p);
    // If the scanner failed to set the source, we can't do anything.
    if(p->scanner->failed_to_set_source) {
        // The scanner has already reported the error.
        p->program = NULL;
        return false;
    }

    // Create the root module (A special module that represents the toplevel scope (file scope)).
    ASTParsedModule *root_module = astNewParsedModule(AST_STRING(astParsedProgramAddString(prog, "___root_module___"), EMPTY_LOCATION));
    p->current.module = astParsedProgramAddModule(prog, root_module);
    root_module->id = p->current.module;
    p->current.scope = astParsedModuleGetModuleScopeID(root_module);
    add_primitive_types(prog, root_module);
    p->current.allocator = &root_module->ast_allocator.alloc;


    // TODO: move this to parse_module_body() when module suppport is added.
    while(!is_eof(p)) {
        if(match(p, TK_FN)) {
            UNREACHABLE();
        } else if(match(p, TK_VAR)) {
            Location var_loc = previous(p).location;
            ASTParsedStmtNode *var = parse_variable_decl(p, true, true, &root_module->module_scope->objects, &var_loc);
            if(!consume(p, TK_SEMICOLON)) {
                continue;
            }
            if(var != NULL) {
                // TODO: Add astModuleAddVariable(module, variable).
                arrayPush(&root_module->globals, (void *)var);
            }
        } else {
            error_at(p, current(p).location, stringFormat("Expected one of ['fn', 'var'], but got '%s'.", tokenTypeString(current(p).type)));
        }

        if(p->need_synchronize) {
            p->need_synchronize = false;
            synchronize(p);
        }
    }


    // Clean up & return
    p->current.allocator = NULL;
    p->program = NULL;
    return !p->had_error;
}
