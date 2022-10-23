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


#include <assert.h>
void gen_arm64(Program *p) {
    static const char *reg = "x0";
    const char *labels[10] = {
        ".L0", ".L1", ".L2", ".L3", ".L4", ".L5", ".L6", ".L7", ".L8", ".L9"
    };
    int used_labels = 0;
    #define L(l) (assert((l) < 10 && used_labels < 10), used_labels = ((l) + 1 < used_labels ? used_labels : (l) + 1))

    int pc = 0;
    printf(".text\n");
    while(pc < p->length) {
        if(pc == p->entry_point) {
            puts("ir_entry:");
        }
        OpCode op = p->code[pc];
        switch(DECODE(op)) {
            case OP_IMM: printf("mov x1, %u\nstr x1, [sp, -16]!\n", DECODE_ARG(op)); break;
            case OP_ST: L(DECODE_ARG(op)); printf("ldr x1, [sp], 16\nadrp x2, %s\nadd x2, x2, :lo12:%s\nstr x1, [x2]\n", labels[DECODE_ARG(op)], labels[DECODE_ARG(op)]); break;
            case OP_LD: L(DECODE_ARG(op)); printf("adrp x1, %s\nadd x1, x1, :lo12:%s\nldr x1, [x1]\nstr x1, [sp, -16]!\n", labels[DECODE_ARG(op)], labels[DECODE_ARG(op)]); break;
            //case OP_STL: stack[bp + 1 + DECODE_ARG(op)] = pop(); break;
            //case OP_LDL: push(stack[bp + 1 + DECODE_ARG(op)]); break;
            //case OP_ARG:
            //    //               - arg  - 1  \/ <- bp
            //    // stack frame: [args][old_bp][old_pc]
            //    // -2 instead of -1 so that the first index can be 0.
            //    push(stack[bp - 2 - DECODE_ARG(op)]);
            //    break;
            case OP_ADJ:
                printf("sub sp, sp, #%u\n", DECODE_ARG(op));
                break;
            case OP_ADD: printf("ldr x1, [sp], 16\nldr x2, [sp], 16\n add x1, x1, x2\nstr x1, [sp, -16]!\n"); break;
            case OP_ENT:
                printf("fn_%u:\n", pc);
                printf("sub sp, sp, #%u\n", DECODE_ARG(op));
                break;
            case OP_LEV:
                puts("ret");
                break;
            case OP_SR:
                printf("ldr %s, [sp], 16\n", reg);
                break;
            case OP_LR:
                printf("str %s, [sp, -16]!\n", reg);
                break;
            case OP_CALL:
                puts("stp fp, lr, [sp, -16]!");
                puts("mov fp, sp");
                printf("bl fn_%u\n", DECODE_ARG(op));
                puts("mov sp, fp");
                puts("ldp fp, lr, [sp], 16");
                break;
            default: assert(0);
        }
        pc += 1;
    }
    // generate _start
    puts("ret");
    puts(".global _start");
    puts("_start:");
    puts("bl ir_entry");
    puts("mov x8, #93\nldr x0, [sp], 16\nsvc 0");
    #undef L

    // emit data section
    puts(".data");
    for(int i = 0; i < used_labels; ++i) {
        printf("%s:\n.dword 0\n", labels[i]);
    }

}

int main(int argc, char **argv) {
    bool debug_dump = false;
    if(argc == 2 && argv[1][0] == '-' && argv[1][1] == 'd' && argv[1][2] == 0) {
        debug_dump = true;
    }
    //int result = execute(prog1.code, prog1.length, prog1.entry_point, debug_dump);
    //int result = execute(prog2.code, prog2.length, prog2.entry_point, debug_dump);
    //int result = execute(prog3.code, prog3.length, prog3.entry_point, debug_dump);
    //printf("result = %d\n", result);
    gen_arm64(&prog1);
    return 0;
}
