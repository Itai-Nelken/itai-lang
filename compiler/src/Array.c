#include <stdlib.h>
#include "memory.h"
#include "Array.h"

void initArray(Array *a) {
    a->used = 0;
    a->capacity = 8;
    a->data = CALLOC(a->capacity, sizeof(void *));
}

void freeArray(Array *a) {
    FREE(a->data);
    a->data = NULL;
    a->used = a->capacity = 0;
}

int arrayPush(Array *a, void *value) {
    if(a->used + 1 > a->capacity) {
        a->capacity *= 2;
        a->data = REALLOC(a->data, sizeof(void *) * a->capacity);
    }
    a->data[a->used++] = value;
    return a->used - 1;
}

void *arrayPop(Array *a) {
    if(a->used <= 0) {
        return NULL;
    }
    return a->data[a->used--];
}

void *arrayGet(Array *a, int index) {
    if((size_t)index > a->used) {
        return NULL;
    }
    return a->data[index];
}

void arrayCopy(Array *dest, Array *src) {
    // ssize_t to make gcc happy (unsigned value >= 0 is always true)
    for(ssize_t i = src->used-1; i >= 0; --i) {
        arrayPush(dest, arrayGet(src, i));
    }
}

void arrayMap(Array *a, void(*fn)(void *item, void *cl), void *cl) {
    for(size_t i = 0; i < a->used; ++i) {
        fn(a->data[i], cl);
    }
}
