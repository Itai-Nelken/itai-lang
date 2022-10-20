#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "OpCode.h"

static int data[256];
static int stack[256];
static int sp = 0;
static int bp = 0;
static int reg = 0;

static void push(int value) { assert(sp < 256); stack[sp++] = value; }
static int pop() { assert(sp > 0); return stack[--sp]; }

static void print_stack() {
    printf("stack: ");
    if(sp == 0) {
        printf("[<empty>]");
    }
    for(int i = 0; i < sp; ++i) {
        printf("[%d]", stack[i]);
    }
    putchar('\n');
}

static int get_data(int idx) { assert(idx >= 0 && idx < 256); return data[idx]; }
static void set_data(int idx, int value) { assert(idx >= 0 && idx < 256); data[idx] = value; }

int execute(OpCode program[], int length, int entry_point, bool debug_dump) {
    int pc = entry_point;
    if(debug_dump) printf("pc: %d\n", pc);
    while(pc < length) {
        OpCode op = program[pc];
        switch(DECODE(op)) {
            #define PA(name) printf("> " name " %d\n", DECODE_ARG(op))
            #define P(name) puts("> " name)
            case OP_IMM: PA("imm"); push(DECODE_ARG(op)); break;
            case OP_ST: PA("st"); set_data(DECODE_ARG(op), pop()); break;
            case OP_LD: PA("ld"); push(get_data(DECODE_ARG(op))); break;
            case OP_STL: PA("stl"); stack[bp + 1 + DECODE_ARG(op)] = pop(); break;
            case OP_LDL: PA("ldl"); push(stack[bp + 1 + DECODE_ARG(op)]); break;
            case OP_ARG:
                PA("arg");
                //               - arg  - 1  \/ <- bp
                // stack frame: [args][old_bp][old_pc]
                // -2 instead of -1 so that the first index can be 0.
                push(stack[bp - 2 - DECODE_ARG(op)]);
                break;
            case OP_ADJ:
                PA("adj");
                sp -= DECODE_ARG(op);
                break;
            case OP_ADD: P("add"); push(pop() + pop()); break;
            case OP_ENT: PA("ent"); sp += DECODE_ARG(op); break;
            case OP_LEV:
                P("lev");
                // restore pc and sp
                pc = stack[bp];
                sp = bp; // also pops the saved pc.
                bp = pop(); // restore bp
                break;
            case OP_SR:
                P("sr");
                reg = pop();
                break;
            case OP_LR:
                P("lr");
                push(reg);
                break;
            case OP_CALL:
                PA("call");
                // set up a call frame
                push(bp); // save bp
                bp = sp;
                push(pc);
                // jump
                pc = DECODE_ARG(op) - 1;
                break;
            default: assert(0);
            #undef P
            #undef PA
        }
        if(debug_dump) {
            printf("pc: %d\nbp: %d\nr: %d\n", pc, bp, reg);
            print_stack();
        }
        pc += 1;
    }
    return pop();
}
