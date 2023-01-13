#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "Token.h"
#include "Ast.h"
#include "Scanner.h"
#include "memory.h" // Allocator

typedef struct parser {
    // input/output/errors
    Compiler *compiler;
    Scanner *scanner;
    ASTProgram *program;

    // options
    bool dump_tokens;

    // current module/function
    struct {
        ModuleID module; // TODO: should be a stack (when multiple modules support is added).
        Allocator *allocator; // TODO: change with modules (also using a stack>)
        ASTObj *function; // NOTE: should be a stack when closures are supported?
        BlockScope *scope;
        Attribute *attribute; // TODO: should be an array once support for multiple attributes is needed.
    } current;

    // state
    bool can_assign; // If changed, the old value has to be saved and restored.
    Token previous_token, current_token;
    bool had_error, need_synchronize;
} Parser;

/***
 * Initialize a parser.
 *
 * @param p The parser to initialize.
 * @param s A Scanner.
 * @param c A Compiler.
 ***/
void parserInit(Parser *p, Scanner *s, Compiler *c);

/***
 * Free a parser.
 *
 * @param p The parser to free.
 ***/
void parserFree(Parser *p);

/***
 * Set the dump_tokens flag (print out tokens if set).
 *
 * @param p A Parser.
 * @param value The value to set the flag to.
 ***/
void parserSetDumpTokens(Parser *p, bool value);

/***
 * Parse.
 *
 * @param p A Parser.
 * @param prog An ASTProgram.
 * @return true on success or false on failure.
 ***/
bool parserParse(Parser *p, ASTProgram *prog);

#endif // PARSER_H
