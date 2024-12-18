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

static inline Token current(Parser *p) {
    return p->state.current_token;
}

static inline Token previous(Parser *p) {
    return p->state.previous_token;
}

static inline bool is_eof(Parser *p) {
    return current(p).type == TK_EOF;
}

static Token advance(Parser *p) {
    // Skip TK_GARBAGE tokens.
    // The parser doesn't know how to handle them, and the scanner
    // has already reported error for them anyway.
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

static void import_primitive_types(ASTProgram *prog, ASTModule *module) {
    // We "import" the primitive types into each module.
    // In reality, we create new types each time, but since
    // primitives are equal by their TypeType, this doesn't matter.
#define DEF(type, name) {Type *ty = typeNew((type), stringTableString(&prog->strings, (name)), EMPTY_LOCATION, module); astModuleAddType(module, ty);}

    DEF(TY_VOID, "void");
    DEF(TY_I32, "i32");

#undef DEF
}


static bool parseModuleBody(Parser *p, ASTString name) {
    ASTModule *module = astProgramNewModule(p->program, name);
    // If we are parsing a module body, there shouldn't be an existing current module.
    VERIFY(p->current.module == NULL);
    VERIFY(p->current.scope == NULL);
    p->current.module = module;
    p->current.scope = module->moduleScope;
    // Import all the primitive (builtin) types into the module.
    import_primitive_types(p->program, module);

    // TODO: decl = parse_decl(p); add_decl(p->current.module, decl);
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

    // parseModuleDecl()
    //if(match(p, TK_MODULE)) {
    //    Location loc = previous(p).location;
    //    TRY_CONSUME(p, TK_IDENTIFIER);
    //    TRY_CONSUME(p, TK_AMPERSAND);
    //    errorAt(p, loc, "Module declarations are not yet unsupported.");
    //}

    // The root module represents the top level (file) scope).
    parseModuleBody(p, stringTableString(&prog->strings, "___root_module___"));


}
