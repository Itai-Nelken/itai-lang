#include <stdlib.h>
#include <assert.h>
#include "common.h"
#include "memory.h"
#include "Array.h"

void arrayInit(Array *a) {
    arrayInitSized(a, ARRAY_INITIAL_CAPACITY);
}

static void *alloc_fn(void *arg, size_t size) {
    UNUSED(arg);
    return calloc(1, size);
}

static void *realloc_fn(void *arg, void *ptr, size_t size) {
    UNUSED(arg);
    return realloc(ptr, size);
}

static void free_fn(void *arg, void *ptr) {
    UNUSED(arg);
    free(ptr);
}

void arrayInitSized(Array *a, size_t size) {
    a->allocator = (Allocator){
        .allocFn = alloc_fn,
        .reallocFn = realloc_fn,
        .freeFn = free_fn,
        .arg = NULL
    };
    a->used = 0;
    a->capacity = size == 0 ? ARRAY_INITIAL_CAPACITY : size;
    a->data = a->allocator.allocFn(a->allocator.arg, a->capacity * sizeof(void *));
}

void arrayInitAllocatorSized(Array *a, Allocator alloc, size_t size) {
    a->allocator = alloc;
    // TODO: un-duplicate with arrayInitSized().
    a->used = 0;
    a->capacity = size == 0 ? ARRAY_INITIAL_CAPACITY : size;
    a->data = a->allocator.allocFn(a->allocator.arg, a->capacity * sizeof(void *));
}

void arrayFree(Array *a) {
    a->allocator.freeFn(a->allocator.arg, a->data);
    a->data = NULL;
    a->used = a->capacity = 0;
}

size_t arrayLength(Array *a) {
    return a->used;
}

size_t arrayPush(Array *a, void *value) {
    if(a->used + 1 > a->capacity) {
        a->capacity *= 2;
        a->data = a->allocator.reallocFn(a->allocator.arg, a->data, sizeof(void *) * a->capacity);
    }
    a->data[a->used++] = value;
    return a->used - 1;
}

void *arrayPop(Array *a) {
    if(a->used == 0) {
        return NULL;
    }
    return a->data[--a->used];
}

void arrayInsert(Array *a, size_t index, void *value) {
    assert(index < a->used);
    a->data[index] = value;
}

void *arrayGet(Array *a, size_t index) {
    if(index > a->used) {
        return NULL;
    }
    return a->data[index];
}

void arrayClear(Array *a) {
    a->used = 0;
}

void arrayCopy(Array *dest, Array *src) {
    arrayClear(dest);
    for(size_t i = 0; i < src->used; ++i) {
        arrayPush(dest, arrayGet(src, i));
    }
}

void arrayReverse(Array *a) {
    // The division below mught result in a float,
    // in which case it is rounded down which is what we want.
    for(size_t i = 0; i < a->used / 2; ++i) {
        void *tmp = a->data[i];
        a->data[i] = a->data[a->used - i - 1];
        a->data[a->used - i - 1] = tmp;
    }
}

void arrayMap(Array *a, void(*callback)(void *item, void *cl), void *cl) {
    for(size_t i = 0; i < a->used; ++i) {
        callback(a->data[i], cl);
    }
}

void arrayMapIndex(Array *a, void (*callback)(void *item, size_t index, void *cl), void *cl) {
    for(size_t i = 0; i < a->used; ++i) {
        callback(a->data[i], i, cl);
    }
}
