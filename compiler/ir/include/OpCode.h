#ifndef OPCODE_H
#define OPCODE_H


typedef enum op_type {
    OP_START_FUNCTION, // arg1: <function> (temp stack effect implementation defined)
    OP_END_FUNCTION, // no args, (temp stack effect dependent on OP_START_FUNCTION)
    OP_RETURN, // arg1: OpArg, pops 1 temp
    OP_CALL, // arg1: <function>, pops <function>.parameters temp

    OP_STORE_GLOBAL, // arg1: <global>, arg2: <offset>, pops 1 temp
    OP_LOAD_GLOBAL, // arg1: <global>, arg2: <offset>, pushes 1 temp
    OP_STORE_LOCAL, // arg1: <local>, arg2: <offset>, pops 1 temp
    OP_LOAD_LOCAL, // arg1: <local>, arg2: <offset>, pushes 1 temp

    OP_ADD, // no args, pops 2 temp pushes 1 temp

    OP_IMM, // arg1: <number literal>, pushes 1 temp (//TODO: support large literals (larger than 255 (UINT8_MAX)))
} OpCode;

typedef enum op_arg {
    ARG_VALUE,
    ARG_NONE
} OpArg;

#endif // OPCODE_H
