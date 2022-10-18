#ifndef OPS_H
#define OPS_H

#include <stdint.h>

typedef enum op_type {
    OP_IMM, // IMM <i64>
    OP_ST, // ST <global index>
    OP_LD, // LD <global index>
    OP_ARG, // ARG <arg num>
    OP_ADJ, // ADJ <num>
    OP_ADD, //OP_SUB, // ADD/SUB
    //OP_MUL, OP_DIV, // MUL/DIV
    OP_ENT, // ENT <local byte count>
    OP_LEV, // LEV
    OP_SR, // SR <value>
    OP_LR, // LR (push the value in the register)
    OP_CALL, // CALL <global index>
    OP_POP, // POP
    //OP_JMP // JMP <bytecode array idx>
} OpType;

// 4 bits for opcode, 12 bits for argument
typedef uint16_t OpCode;

#define ENCODE(op) ((OpCode)((op) << 12))
#define ENCODE_ARG(op, arg) ((OpCode)(((op) << 12) + (arg)))

#define DECODE(opcode) ((OpCode)(((opcode) & 0xf000) >> 12))
#define DECODE_ARG(opcode) (((opcode) & 0x0fff))

#endif // OPS_H
