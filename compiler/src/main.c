#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Ast.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"
#include "Validator.h"

enum return_values {
    RET_SUCCESS = 0,
    RET_ARG_PARSE_FAILURE,
    RET_PARSE_FAILURE,
    RET_VALIDATE_ERROR
};

typedef struct options {
    const char *file_path;
    bool dump_ast;
} Options;

bool parse_arguments(Options *opts, int argc, char **argv) {
    struct option long_options[] = {
        {"help",     no_argument, 0, 'h'},
        {"dump-ast", no_argument, 0, 'd'},
        {0,          0,           0,  0}
    };
    int c;
    while((c = getopt_long(argc, argv, "hd", long_options, NULL)) != -1) {
        switch(c) {
            case 'h':
                printf("Usage: %s [options] file\n", argv[0]);
                printf("Options:\n");
                printf("\t--help,     -h    Print this help.\n");
                printf("\t--dump-ast, -d    Dump the parsed AST.\n");
                return false;
            case 'd':
                opts->dump_ast = true;
                break;
            default:
                return false;
        }
    }
        if(optind >= argc) {
            if(opts->file_path) {
                return true;
            }
            fprintf(stderr, "Expected file name!\n");
            return false;
        } else {
            opts->file_path = argv[optind];
        }
    return true;
}


int main(int argc, char **argv) {
    int return_value = RET_SUCCESS;
    Compiler c;
    Scanner s;
    Parser p;
    Validator v;
    ASTProgram prog;
    compilerInit(&c);
    scannerInit(&s, &c);
    parserInit(&p, &s, &c);
    validatorInit(&v, &c);
    astProgramInit(&prog);

    Options opts = {
        .file_path = "./test.ilc",
        .dump_ast = false
    };
    if(!parse_arguments(&opts, argc, argv)) {
        return_value = RET_ARG_PARSE_FAILURE;
        goto end;
    }

    compilerAddFile(&c, opts.file_path);

    //Token tk;
    //for(tk = scannerNextToken(&s); tk.type != TK_EOF; tk = scannerNextToken(&s)) {
    //    tokenPrint(stdout, &tk);
    //    putc('\n', stdout);
    //}

    if(!parserParse(&p, &prog)) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Parser failed with no errors!\n", stderr);
        }
        return_value = RET_PARSE_FAILURE;
        goto end;
    }

    if(!validatorValidate(&v, &prog)) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError: Validator failed with no errors!\n", stderr);
        }
        return_value = RET_VALIDATE_ERROR;
        goto end;
    }

    if(opts.dump_ast) {
        astProgramPrint(stdout, &prog);
        fputc('\n', stdout);
    }

end:
    astProgramFree(&prog);
    validatorFree(&v);
    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return return_value;
}
