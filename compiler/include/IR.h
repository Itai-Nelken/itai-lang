#ifndef IR_H
#define IR_H

#include <stddef.h> // for size_t
#include "Array.h"
#include "types.h"

typedef enum ir {
    OP_NUM_LIT,
    OP_NEG,
    OP_ADD, OP_SUB,
    OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE,
    OP_GT, OP_GE,
    OP_LT, OP_LE,
    OP_OR, OP_XOR,
    OP_AND,
    OP_RSHIFT, OP_LSHIFT
} IR;

typedef enum ir_literal_type {
    LIT_NUM32
} IRLiteralType;

typedef struct ir_literal {
    IRLiteralType type;
    union {
        i32 int32;
    } as;
} IRLiteral;

typedef struct ir_array {
    int *data;
    size_t capacity, used;
} IRArray;

typedef struct ir_builder {
    Array literals; // Array<IRLiteral *>
    IRArray ir;
} IRBuilder;

void initIRArray(IRArray *a);
void freeIRArray(IRArray *a);

void IRArrayPush(IRArray *a, int value);
int IRArrayGet(IRArray *a, size_t index);

void initIRBuilder(IRBuilder *builder);
void freeIRBuilder(IRBuilder *builder);

void IRBuilderWriteByte(IRBuilder *builder, int byte);
void IRBuilderWriteBytes(IRBuilder *builder, int byte1, int byte2);

int IRBuilderAddInt32Literal(IRBuilder *builder, i32 value);

#endif // IR_H
