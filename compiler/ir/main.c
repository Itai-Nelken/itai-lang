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
    ENCODE_ARG(OP_ENT, 0), // idx: 4
    ENCODE_ARG(OP_CALL, 0),
    ENCODE(OP_LR),
    ENCODE_ARG(OP_LD, 0),
    ENCODE(OP_ADD),
    ENCODE_ARG(OP_ST, 0),
    ENCODE(OP_LEV),

    // start (idx: 11)
    ENCODE_ARG(OP_IMM, 40),
    ENCODE_ARG(OP_ST, 0),
    ENCODE_ARG(OP_CALL, 4),
    ENCODE_ARG(OP_LD, 0)
};
Program prog1 = {
    .code = program1,
    .length = sizeof(program1)/sizeof(program1[0]),
    .entry_point = 11
};


OpCode program2[] = {
    // fn add1(x: i32) -> i32 { return x + 1; }
    ENCODE_ARG(OP_ENT, 0),
    ENCODE_ARG(OP_ARG, 0),
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

OpCode program3[] = {
    // fn sum(a: i32, b: i32) -> i32 { return a + b; }
    ENCODE_ARG(OP_ENT, 0), // idx: 0
    ENCODE_ARG(OP_ARG, 0),
    ENCODE_ARG(OP_ARG, 1),
    ENCODE(OP_ADD),
    ENCODE(OP_SR),
    ENCODE(OP_LEV),

    // fn test() -> i32 { var a = 2; var b = 40; return sum(a, b); }
    ENCODE_ARG(OP_ENT, 2), // idx: 6
    ENCODE_ARG(OP_IMM, 2),
    ENCODE_ARG(OP_STL, 0),
    ENCODE_ARG(OP_IMM, 40),
    ENCODE_ARG(OP_STL, 1),
    ENCODE_ARG(OP_LDL, 0),
    ENCODE_ARG(OP_LDL, 1),
    ENCODE_ARG(OP_CALL, 0), // return value is alread in the register.
    ENCODE_ARG(OP_ADJ, 2),
    ENCODE(OP_LEV),

    // start:
    // test()
    ENCODE_ARG(OP_CALL, 6), // idx: 16
    ENCODE(OP_LR)
};
Program prog3 = {
    .code = program3,
    .length = sizeof(program3)/sizeof(program3[0]),
    .entry_point = 16
};

int main(int argc, char **argv) {
    bool debug_dump = false;
    if(argc == 2 && argv[1][0] == '-' && argv[1][1] == 'd' && argv[1][2] == 0) {
        debug_dump = true;
    }
    //int result = execute(prog1.code, prog1.length, prog1.entry_point, debug_dump);
    //int result = execute(prog2.code, prog2.length, prog2.entry_point, debug_dump);
    int result = execute(prog3.code, prog3.length, prog3.entry_point, debug_dump);
    printf("result = %d\n", result);
    return 0;
}
