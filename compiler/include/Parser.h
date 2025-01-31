#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "Strings.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Token.h"
#include "Ast/Program.h"

typedef struct parser {
    Compiler *compiler;
    Scanner *scanner;
    ASTProgram *program;

    struct {
        ModuleID module;
        Scope *scope;
    } current;

    String tmp_buffer;

    struct {
        Token current_token;
        Token previous_token;
        bool had_error;
        bool need_sync;
        TokenType prevSyncTo; // The last token we synced to (to prevent sync->fail->sync->... infinite loops)
    } state;

    // Pointers to the primitive types in the current module
    struct {
        Type *void_;
        Type *int32;
        Type *uint32;
        Type *str;
    } primitives;
} Parser;

/**
 * Initialize a Parser.
 *
 * @param p The parser to initialize.
 * @param c A Compiler to use in the parser.
 * @param s A Scanner to use in the parser.
 **/
void parserInit(Parser *p, Compiler *c, Scanner *s);

/**
 * Free a Parser.
 *
 * @param p The parser to free.
 **/
void parserFree(Parser *p);

/**
 * Parse.
 *
 * @param p A Parser to use for parsing.
 * @param prog An ASTProgram to store the resulting AST in.
 * @return true on success or false on failure (errors are in the Compiler provided to parserInit()).
 **/
bool parserParse(Parser *p, ASTProgram *prog);


#endif // PARSER_H
