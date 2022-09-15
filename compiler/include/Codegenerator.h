#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include <stdio.h> // FILE
#include <stdbool.h>
#include "common.h" // usize
#include "ast.h"
#include "Symbols.h"

typedef struct code_generator_data {
    FILE *buffer;
    void *data;
    void (*print)(struct code_generator_data *cgd, const char *format, ...);
    char *(*get_identifier)(struct code_generator_data *cgd, SymbolID id);
    DataType *(*get_type)(struct code_generator_data*cgd, SymbolID id);
    ASTProgram *_prog;
} CodeGeneratorData;

typedef struct code_generator {
    bool had_error;
    FILE *buffer;
    char *buffer_data;
    usize buffer_length;
    CodeGeneratorData cg_data;
    void (*gen_fn_callback)(ASTFunctionObj *fn, CodeGeneratorData *cg_data);
    void (*gen_pre_fn_callback)(ASTFunctionObj *fn, CodeGeneratorData *cg_data);
    void (*gen_global_callback)(ASTVariableObj *var, CodeGeneratorData *cg_data);
} CodeGenerator;

/***
 * Initialize a code generator.
 *
 * @param cg The CodeGenerator to initialize.
 ***/
void codeGeneratorInit(CodeGenerator *cg);

/***
 * Free a code generator.
 *
 * @param cg The CodeGenerator to free.
 ***/
void codeGeneratorFree(CodeGenerator *cg);

/***
 * Set the custom data to be passed to the callbacks.
 *
 * @param cg The CodeGenerator to use.
 * @param data The data.
 ***/
void codeGeneratorSetData(CodeGenerator *cg, void *data);

/***
 * Set the function callback.
 *
 * @param cg The CodeGenerator to use.
 * @param callback The callback.
 ***/
void codeGeneratorSetFnCallback(CodeGenerator *cg, void (*callback)(ASTFunctionObj *, CodeGeneratorData *));

/***
 * Set the pre-function callback.
 *
 * @param cg The CodeGenerator to use.
 * @param callback The callback.
 ***/
void codeGeneratorSetPreFnCallback(CodeGenerator *cg, void (*callback)(ASTFunctionObj *, CodeGeneratorData *));

/***
 * Set the global variable callback.
 *
 * @param cg The CodeGenerator to use.
 * @param callback The callback.
 ***/
void codeGeneratorSetGlobalCallback(CodeGenerator *cg, void (*callback)(ASTVariableObj *, CodeGeneratorData *));

/***
 * Write the output buffer to stream 'to'.
 *
 * @param cg The CodeGenerator to use.
 * @param to The stream to write to.
 ***/
void codeGeneratorWriteOutput(CodeGenerator *cg, FILE *to);

/***
 * Generate the code.
 *
 * @param cg The CodeGenerator to use.
 * @param prog A parsed and validated ASTProgram.
 ***/
bool codeGeneratorGenerate(CodeGenerator *cg, ASTProgram *prog);

#endif // CODEGENERATOR_H
