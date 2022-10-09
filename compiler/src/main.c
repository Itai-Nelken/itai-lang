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
                printf("Usage: %s [-h] file\n", argv[0]);
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
    parserInit(&p, &s, &c);
    astProgramInit(&prog);

    Options opts = {
        .file_path = "./test.ilc",
        .dump_ast = false
    };
    if(!parse_arguments(&opts, argc, argv)) {
        return_value = 1;
        goto end;
    }

    compilerAddFile(&c, opts.file_path);

    //Token tk;
    //for(tk = scannerNextToken(&s); tk.type != TK_EOF; tk = scannerNextToken(&s)) {
    //    tokenPrint(stdout, &tk);
    //    putc('\n', stdout);
    //}

    parserParse(&p, &prog);

    if(compilerHadError(&c)) {
        compilerPrintErrors(&c);
        return_value = 1;
    } else {
        if(opts.dump_ast) {
            astProgramPrint(stdout, &prog);
            fputc('\n', stdout);
        }
    }

end:
    astProgramFree(&prog);
    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    return return_value;
}
