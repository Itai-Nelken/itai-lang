#ifndef ARRAYS_H
#define ARRAYS_H

#include <stddef.h> // size_t

#define ARRAY_INITIAL_CAPACITY 8

typedef struct array {
    void **data;
    size_t used, capacity;
} Array;

void arrayInit(Array *a);
void arrayFree(Array *a);
int arrayPush(Array *a, void *value);
void *arrayPop(Array *a);
void *arrayGet(Array *a, int index);
void arrayCopy(Array *dest, Array *src);
void arrayMap(Array *a, void(*fn)(void *item, void *cl), void *cl);

#define ARRAY_POP_AS(type, array) ((type)arrayPop(array))
#define ARRAY_GET_AS(type, array, index) ((type)arrayGet(array, index))

#endif // ARRAYS_H
