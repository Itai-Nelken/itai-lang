#ifndef CODEGEN_H
#define CODEGEN_H

/**
 * 'Codegen' is a generic front-end for the code generator in this compiler.
 * Back-ends provide a list of requests, detailing in what order and how many
 * times the different AST objects will be provided.
 * For example, a C transpiler backend will likely ask for types, then structs, then
 * functions (to predeclare them) then functions again (to define them) and so on.
 * Modules are always first, however once inside a module, the request list is followed.
 *
 * Note that the requests list is for the module scope only.
 * Once inside a function, statements and expressions are generated.
 * Once inside a struct, "variable declarations" (the fields) are emmited (and functions when methods are added.)
 **/

#include "Ast/Ast.h"

typedef enum codegen_request {
    CGREQ_TYPES,
    CGREQ_VARIABLES,
    CGREQ_FUNCTIONS,
    CGREQ_STRUCTS,
    //CGREQ_ENUMS,
    CGREQ_END_OF_LIST // end of list marker.
} CGRequest;

typedef struct codegen_interface {
    // TODO: once I know what data will be needed and what state metadata I have to keep,
    //       give functions IDs/names only and then provide CG functions to get specific data.
    void (*beginModule)(ASTModule *m, void *cl);
    void (*endModule)(ASTModule *m, void *cl);
    void (*declType)(Type *ty, void *cl);
    void (*declVar)(ASTObj *var, void *cl);
    void (*beginStruct)(ASTObj *st, void *cl);
    void (*endStruct)(ASTObj *st, void *cl);
    void (*beginFn)(ASTObj *fn, void *cl);
    void (*endFn)(ASTObj *fn, void *cl);
    // TODO: make these more specific maybe? for now this is ok since this front-end is more
    //       of a proof of concept that isn't really needed for a simple C transpiler. Rather,
    //       this will become useful for IR and ASM codegenerators.
    void (*expr)(ASTExprNode *expr, void *cl);
    void (*stmt)(ASTStmtNode *stmt, void *cl);

    void *cl;

    struct {
        ASTProgram *prog;
        struct {
            ASTModule *module;
            Scope *scope;
        } current;
    } data;
} CGInterface;

/**
 * Given a CGInterface and a list of CGRequests, generate output from an ASTProgram.
 *
 * @param cg The CGInterface to use to generate the output.
 * @param requests An array of CGRequests specifying in what order the ASTProgram data is given to the interface.
 * @param prog The ASTProgram to generate output from.
 **/
void cgGenerate(CGInterface *cg, CGRequest requests[], ASTProgram *prog);

#endif // CODEGEN_H
