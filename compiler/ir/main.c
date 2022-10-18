#include <stdio.h>
#include "OpCode.h"
#include "vm.h"

OpCode program[] = {
    // fn two() -> i32 { return 2; }
    ENCODE_ARG(OP_ENT, 0), // idx: 0
    ENCODE_ARG(OP_IMM, 2),
    ENCODE(OP_RET),

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
const int length = sizeof(program)/sizeof(program[0]);
const int entry_point = 9;


int main(void) {
    int result = execute(program, length, entry_point);
    printf("result = %d\n", result);
    return 0;
}
