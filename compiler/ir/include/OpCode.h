#ifndef OPS_H
#define OPS_H

#include <stdint.h>

typedef enum op_type {
    OP_IMM, // IMM <i64>
    OP_ST, // ST <data index>
    OP_LD, // LD <data index>
    OP_STL, // STL <local index>
    OP_LDL, // LDL <local index>
    OP_ARG, // ARG <arg num>
    OP_ADJ, // ADJ <num>
    OP_ADD, //OP_SUB, // ADD/SUB
    //OP_MUL, OP_DIV, // MUL/DIV
    OP_ENT, // ENT <local byte count>
    OP_LEV, // LEV
    OP_SR, // SR
    OP_LR, // LR (push the value in the register)
    OP_CALL, // CALL <bytecode array index>
    //OP_JMP // JMP <bytecode array index>
    OP_COUNT
} OpType;

_Static_assert(OP_COUNT < 16, "Too many opcodes");

// 4 bits for opcode, 12 bits for argument
typedef uint16_t OpCode;

#define ENCODE(op) ((OpCode)((op) << 12))
#define ENCODE_ARG(op, arg) ((OpCode)(((op) << 12) + (arg)))

#define DECODE(opcode) ((OpCode)(((opcode) & 0xf000) >> 12))
#define DECODE_ARG(opcode) (((opcode) & 0x0fff))

#endif // OPS_H
