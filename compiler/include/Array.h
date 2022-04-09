#ifndef ARRAYS_H
#define ARRAYS_H

#include <stddef.h> // size_t

typedef struct array {
    void **data;
    size_t used, capacity;
} Array;

void initArray(Array *a);
void freeArray(Array *a);
void arrayPush(Array *a, void *value);
void *arrayPop(Array *a);
void *arrayGet(Array *a, int index);
void arrayMap(Array *a, void(*fn)(void *item, void *cl), void *cl);

#define ARRAY_POP_AS(type, array) ((type)arrayPop(array))
#define ARRAY_GET_AS(type, array, index) ((type)arrayGet(array, index))

#endif // ARRAYS_H
