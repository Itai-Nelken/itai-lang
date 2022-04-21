#include <assert.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "IR.h"

void initIRArray(IRArray *a) {
    a->capacity = 16;
    a->used = 0;
    a->data = CALLOC(a->capacity, sizeof(*a->data));
}

void freeIRArray(IRArray *a) {
    FREE(a->data);
    a->capacity = a->used = 0;
}

void IRArrayPush(IRArray *a, int value) {
    if(a->used + 1 > a->capacity) {
        a->capacity *= 2;
        a->data = REALLOC(a->data, a->capacity);
    }
    a->data[a->used++] = value;
}

int IRArrayGet(IRArray *a, size_t index) {
    assert(index < a->used);
    return a->data[index];
}

void initIRBuilder(IRBuilder *builder) {
    initIRArray(&builder->ir);
    initArray(&builder->literals);
}

static void ir_literal_free_callback(void *literal, void *cl) {
    UNUSED(cl);
    FREE(literal);
}

void freeIRBuilder(IRBuilder *builder) {
    freeIRArray(&builder->ir);
    arrayMap(&builder->literals, ir_literal_free_callback, NULL);
    freeArray(&builder->literals);
}

void IRBuilderWriteByte(IRBuilder *builder, int byte) {
    IRArrayPush(&builder->ir, byte);
}

void IRBuilderWriteBytes(IRBuilder *builder, int byte1, int byte2) {
    IRBuilderWriteByte(builder, byte1);
    IRBuilderWriteByte(builder, byte2);
}

int IRBuilderAddInt32Literal(IRBuilder *builder, i32 value) {
    IRLiteral *literal = CALLOC(1, sizeof(*literal));
    literal->type = LIT_NUM32;
    literal->as.int32 = value;
    arrayPush(&builder->literals, literal);
    return builder->literals.used - 1;
}
