#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Compiler.h"
#include "Scanner.h"

enum return_values {
    RET_SUCCESS = 0,
    RET_ARG_PARSE_FAILURE,
    RET_PARSE_FAILURE,
    RET_VALIDATE_ERROR,
    RET_CODEGEN_ERROR
};

typedef struct options {
    const char *file_path;
    bool dump_parsed_ast;
    bool dump_checked_ast;
    bool dump_tokens;
} Options;

bool parse_arguments(Options *opts, int argc, char **argv) {
    struct option long_options[] = {
        {"help",             no_argument, 0, 'h'},
        {"dump-parsed-ast",  no_argument, 0, 'p'},
        {"dump-checked-ast", no_argument, 0, 'd'},
        {"dump-tokens",      no_argument, 0, 't'},
        {0,                  0,           0,  0}
    };
    int c;
    while((c = getopt_long(argc, argv, "hpdt", long_options, NULL)) != -1) {
        switch(c) {
            case 'h':
                printf("Usage: %s [options] file\n", argv[0]);
                printf("Options:\n");
                printf("\t--help,             -h    Print this help.\n");
                printf("\t--dump-parsed-ast,  -p    Dump the parsed AST.\n");
                printf("\t--dump-checked-ast, -d    Dump the parsed, validated & typechecked AST.\n");
                printf("\t--dump-tokens,      -t    Dump the scanned tokens.\n");
                return false;
            case 'p':
                opts->dump_parsed_ast = true;
                break;
            case 'd':
                opts->dump_checked_ast = true;
                break;
            case 't':
                opts->dump_tokens = true;
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
    compilerInit(&c);
    scannerInit(&s, &c);

    Options opts = {
        .file_path = "./test.ilc",
        .dump_parsed_ast = false,
        .dump_checked_ast = false,
        .dump_tokens = false
    };
    if(!parse_arguments(&opts, argc, argv)) {
        return_value = RET_ARG_PARSE_FAILURE;
        goto end;
    }

    //if(opts.dump_tokens) {
    //    parserSetDumpTokens(&p, true);
    //}

    compilerAddFile(&c, opts.file_path);

    //if(!parserParse(&p, &parsed_prog)) {
    //    if(compilerHadError(&c)) {
    //        compilerPrintErrors(&c);
    //    } else {
    //        fputs("\x1b[1;31mError:\x1b[0m Parser failed with no errors!\n", stderr);
    //    }
    //    return_value = RET_PARSE_FAILURE;
    //    goto end;
    //}

    //if(opts.dump_parsed_ast) {
    //    printf("====== PARSED AST DUMP for '%s' ======\n", opts.file_path);
    //    astParsedProgramPrint(stdout, &parsed_prog);
    //    puts("\n====== END ======"); // prints newline.
    //}

    //if(!validatorValidate(&v, &checked_prog, &parsed_prog)) {
    //    if(compilerHadError(&c)) {
    //        compilerPrintErrors(&c);
    //    } else {
    //        fputs("\x1b[1;31mError:\x1b[0m Validator failed with no errors!\n", stderr);
    //    }
    //    return_value = RET_VALIDATE_ERROR;
    //    goto end;
    //}

    //if(opts.dump_checked_ast) {
    //    printf("====== CHECKED AST DUMP for '%s' ======\n", opts.file_path);
    //    astCheckedProgramPrint(stdout, &checked_prog);
    //    puts("\n====== END ======"); // prints newline.
    //}

    //if(!codegenGenerate(stdout, &prog)) {
    //    fputs("\x1b[1;31mError: Codegenerator failed!\x1b[0m\n", stderr);
    //    return_value = RET_CODEGEN_ERROR;
    //    goto end;
    //}

end:
    scannerFree(&s);
    compilerFree(&c);
    return return_value;
}
