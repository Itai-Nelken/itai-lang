#include <stdio.h>
#include <assert.h>
#include "OpCode.h"

static int globals[256];
static int stack[256];
static int sp = 0;
static int bp = 0;

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

static int get_global(int idx) { assert(idx >= 0 && idx < 256); return globals[idx]; }
static void set_global(int idx, int value) { assert(idx >= 0 && idx < 256); globals[idx] = value; }

int execute(OpCode program[], int length, int entry_point) {
    int pc = entry_point;
    while(pc < length) {
        //printf("pc: %d\nbp: %d\n", pc, bp);
        //print_stack();
        OpCode op = program[pc];
        switch(DECODE(op)) {
            #define PA(name) printf("> " name " %d\n", DECODE_ARG(op))
            #define P(name) puts("> " name)
            case OP_IMM: PA("imm"); push(DECODE_ARG(op)); break;
            case OP_ST: PA("st"); set_global(DECODE_ARG(op), pop()); break;
            case OP_LD: PA("ld"); push(get_global(DECODE_ARG(op))); break;
            case OP_ADD: P("add"); push(pop() + pop()); break;
            case OP_ENT: PA("ent"); sp += DECODE_ARG(op); break;
            case OP_LEV:
                P("lev");
                // restore pc and sp
                pc = stack[bp];
                sp = bp; // also pops the saved pc.
                bp = pop(); // restore bp
                break;
            case OP_RET: {
                P("ret");
                // restore pc and sp
                int ret_val = pop();
                pc = stack[bp];
                sp = bp;
                bp = pop(bp); // restore bp
                push(ret_val);
                break;
            }
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
        pc += 1;
    }
    return pop();
}
