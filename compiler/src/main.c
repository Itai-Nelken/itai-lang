#include <stdio.h>
#include <getopt.h>
#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Ast/Program.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"
#include "Validator.h"
#include "Typechecker.h"
#include "Codegen.h"

enum return_values {
    RET_SUCCESS = 0,
    RET_ARG_PARSE_FAILURE,
    RET_PARSE_FAILURE,
    RET_VALIDATE_FAILURE,
    RET_TYPECHECK_FAILURE,
    RET_CODEGEN_FAILURE
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
    StringTable stringTable;
    ASTProgram parsedProgram, checkedProgram;
    Compiler c;
    Scanner s;
    Parser p;
    Validator v;
    Typechecker typ;
    stringTableInit(&stringTable);
    astProgramInit(&parsedProgram, &stringTable);
    astProgramInit(&checkedProgram, &stringTable);
    compilerInit(&c);
    scannerInit(&s, &c);
    parserInit(&p, &c, &s);
    validatorInit(&v, &c);
    typecheckerInit(&typ, &c);

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

    if(opts.dump_tokens) {
        parserSetDumpTokens(&p, true);
    }

    compilerAddFile(&c, opts.file_path);

    if(!parserParse(&p, &parsedProgram)) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Parser failed with no errors!\n", stderr);
        }
        return_value = RET_PARSE_FAILURE;
        goto end;
    }

    if(opts.dump_parsed_ast) {
        printf("====== PARSED AST DUMP for '%s' ======\n", opts.file_path);
        astProgramPrint(stdout, &parsedProgram);
        puts("\n====== END ======"); // prints newline.
    }

    if(!validatorValidate(&v, &parsedProgram, &checkedProgram)) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Validator failed with no errors!\n", stderr);
        }
        return_value = RET_VALIDATE_FAILURE;
        goto end;
    }

    if(!typecheckerTypecheck(&typ, &checkedProgram)) {
        if(compilerHadError(&c)) {
            compilerPrintErrors(&c);
        } else {
            fputs("\x1b[1;31mError:\x1b[0m Typechecker failed with no errors!\n", stderr);
        }
        return_value = RET_TYPECHECK_FAILURE;
        goto end;
    }

    if(opts.dump_checked_ast) {
        printf("====== CHECKED AST DUMP for '%s' ======\n", opts.file_path);
        astProgramPrint(stdout, &checkedProgram);
        puts("\n====== END ======"); // prints newline.
    }

    codegenGenerate(stdout, &checkedProgram);

end:
    typecheckerFree(&typ);
    validatorFree(&v);
    parserFree(&p);
    scannerFree(&s);
    compilerFree(&c);
    astProgramFree(&checkedProgram);
    astProgramFree(&parsedProgram);
    stringTableFree(&stringTable);
    return return_value;
}
