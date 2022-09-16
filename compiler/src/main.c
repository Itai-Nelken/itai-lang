#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include "common.h"
#include "memory.h"
#include "Token.h"
#include "ast.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"
#include "Validator.h"
#include "codegenerators/c_codegen.h"

typedef struct options {
    bool dump_ast;
    const char *file_path;
} Options;

bool parse_arguments(Options *opts, int argc, char **argv) {
    struct option long_options[] = {
        {"help",     no_argument, 0, 'h'},
        {"dump_ast", no_argument, 0, 'd'},
        {0,          0,           0,  0}
    };
    int c;
    while((c = getopt_long(argc, argv, "hd", long_options, NULL)) != -1) {
        switch(c) {
            case 'h':
                printf("Usage: %s [-d|-h] file\n", argv[0]);
                return false;
            case 'd':
                opts->dump_ast = true;
                break;
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
    int return_value = 0;
    Compiler c;
    Scanner s;
    Parser p;
    ASTProgram prog;
    compilerInit(&c);
    scannerInit(&s, &c);
    parserInit(&p, &c);
    astInitProgram(&prog);

    Options opts = {
        .dump_ast = false,
        .file_path = "../build/test.ilc"
    };
    if(!parse_arguments(&opts, argc, argv)) {
        return_value = 1;
        goto end;
    }

    compilerAddFile(&c, opts.file_path);

    bool result = parserParse(&p, &s, &prog);
    if(!result) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Parser failed but didn't report any errors!\n", stderr);
        }
        return_value = 1;
        goto end;
    }

    result = validateAndTypecheckProgram(&c, &prog);
    if(!result) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Validator failed but didn't report any errors!\n", stderr);
        }
        return_value = 1;
        goto end;
    }

    if(opts.dump_ast) {
        astPrintProgram(stdout, &prog);
        fputc('\n', stdout);
    }

    if(!c_codegen(&prog)) {
        fputs("\x1b[1;31mError:\x1b[0m code generator failed!\n", stderr);
    }

end:
    astFreeProgram(&prog);
    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return return_value;
}
