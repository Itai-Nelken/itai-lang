#include <stdio.h>
#include <string.h> // memset()
#include <stdarg.h>
#include <stdbool.h>
#include "common.h"
#include "Array.h"
#include "ast.h"
#include "Symbols.h"
#include "Codegenerator.h"

static void print_callback(CodeGeneratorData *cgd, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(cgd->buffer, format, ap);
    va_end(ap);
}

static char *get_identifier_callback(CodeGeneratorData *cgd, SymbolID id) {
    VERIFY(cgd->_prog);
    char *s = (char *)symbolTableGetIdentifier(&cgd->_prog->symbols, id);
    VERIFY(s);
    return s;
}

static DataType *get_type_callback(CodeGeneratorData *cgd, SymbolID id) {
    VERIFY(cgd->_prog);
    DataType *ty = symbolTableGetType(&cgd->_prog->symbols, id);
    VERIFY(ty);
    return ty;
}

void codeGeneratorInit(CodeGenerator *cg) {
    memset(cg, 0, sizeof(*cg));
    cg->buffer = open_memstream(&cg->buffer_data, &cg->buffer_length);
    VERIFY(cg->buffer);
    cg->cg_data.buffer = cg->buffer;
    cg->cg_data.print = print_callback;
    cg->cg_data.get_identifier = get_identifier_callback;
    cg->cg_data.get_type = get_type_callback;
}

void codeGeneratorFree(CodeGenerator *cg) {
    fclose(cg->buffer);
    free(cg->buffer_data);
    memset(cg, 0, sizeof(*cg));
}

void codeGeneratorSetData(CodeGenerator *cg, void *data) {
    cg->cg_data.data = data;
}

void codeGeneratorSetFnCallback(CodeGenerator *cg, void (*callback)(ASTFunctionObj *, CodeGeneratorData *)) {
    cg->gen_fn_callback = callback;
}

void codeGeneratorSetPreFnCallback(CodeGenerator *cg, void (*callback)(ASTFunctionObj *, CodeGeneratorData *)) {
    cg->gen_pre_fn_callback = callback;
}

void codeGeneratorSetGlobalCallback(CodeGenerator *cg, void (*callback)(ASTVariableObj *, CodeGeneratorData *)) {
    cg->gen_global_callback = callback;
}

static void gen_object_callback(void *object, void *codegenerator) {
    CodeGenerator *cg = (CodeGenerator *)codegenerator;
    ASTObj *obj = AS_OBJ(object);
    switch(obj->type) {
        case OBJ_FUNCTION:
            cg->gen_fn_callback(AS_FUNCTION_OBJ(obj), &cg->cg_data);
            break;
        case OBJ_GLOBAL:
            cg->gen_global_callback(AS_VARIABLE_OBJ(obj), &cg->cg_data);
            break;
        default:
            UNREACHABLE();
    }
}

static void gen_pre_fn_callback(void *object, void *codegenerator) {
    CodeGenerator *cg = (CodeGenerator *)codegenerator;
    ASTObj *obj = AS_OBJ(object);
    if(obj->type == OBJ_FUNCTION) {
        cg->gen_pre_fn_callback(AS_FUNCTION_OBJ(obj), &cg->cg_data);
    }
}

static void gen_module_callback(void *module, void *codegenerator) {
    ASTModule *mod = (ASTModule *)module;
    arrayMap(&mod->objects, gen_pre_fn_callback, codegenerator);
    arrayMap(&mod->objects, gen_object_callback, codegenerator);
}

void codeGeneratorWriteOutput(CodeGenerator *cg, FILE *to) {
    fflush(cg->buffer);
    fprintf(to, "%s", cg->buffer_data);
}

bool codeGeneratorGenerate(CodeGenerator *cg, ASTProgram *prog) {
    VERIFY(cg->gen_fn_callback);
    VERIFY(cg->gen_global_callback);
    VERIFY(cg->gen_pre_fn_callback);
    cg->cg_data._prog = prog;

    arrayMap(&prog->modules, gen_module_callback, (void *)cg);

    cg->cg_data._prog = NULL;
    return !cg->had_error;
}
