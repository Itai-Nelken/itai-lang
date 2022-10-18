#include <stdio.h>
#include "OpCode.h"
#include "vm.h"

typedef struct program {
    OpCode *code;
    int length;
    int entry_point;
} Program;

OpCode program1[] = {
    // fn two() -> i32 { return 2; }
    ENCODE_ARG(OP_ENT, 0), // idx: 0
    ENCODE_ARG(OP_IMM, 2),
    ENCODE(OP_SR),
    ENCODE(OP_LEV),

    // fn test() { a += two(); }
    ENCODE_ARG(OP_ENT, 0), // idx: 3
    ENCODE_ARG(OP_LD, 0),
    ENCODE_ARG(OP_CALL, 0),
    ENCODE(OP_ADD),
    ENCODE_ARG(OP_ST, 0),
    ENCODE(OP_LEV),

    // start (idx: 9)
    ENCODE_ARG(OP_IMM, 40),
    ENCODE_ARG(OP_ST, 0),
    ENCODE_ARG(OP_CALL, 3),
    ENCODE_ARG(OP_LD, 0)
};
Program prog1 = {
    .code = program1,
    .length = sizeof(program1)/sizeof(program1[0]),
    .entry_point = 9
};


OpCode program2[] = {
    // fn add1(x: i32) -> i32 { return x + 1; }
    ENCODE_ARG(OP_ENT, 0),
    ENCODE_ARG(OP_ARG, 1),
    ENCODE_ARG(OP_IMM, 1),
    ENCODE(OP_ADD),
    ENCODE(OP_SR),
    ENCODE(OP_LEV),

    // start:
    ENCODE_ARG(OP_IMM, 41),
    ENCODE_ARG(OP_CALL, 0),
    ENCODE_ARG(OP_ADJ, 1),
    ENCODE(OP_LR)
};
Program prog2 = {
    .code = program2,
    .length = sizeof(program2)/sizeof(program2[0]),
    .entry_point = 6
};

int main(int argc, char **argv) {
    bool debug_dump = false;
    if(argc == 2 && argv[1][0] == '-' && argv[1][1] == 'd' && argv[1][2] == 0) {
        debug_dump = true;
    }
    int result = execute(prog2.code, prog2.length, prog2.entry_point, debug_dump);
    printf("result = %d\n", result);
    return 0;
}
