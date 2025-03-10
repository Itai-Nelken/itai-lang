#include <stdlib.h>
#include <sys/types.h> // ssize_t
#include "common.h"
#include "memory.h"
#include "Array.h"

void arrayInit(Array *a) {
    arrayInitSized(a, ARRAY_INITIAL_CAPACITY);
}

static void *alloc_callback(void *arg, size_t size) {
    UNUSED(arg);
    return calloc(1, size);
}

static void *realloc_callback(void *arg, void *ptr, size_t size) {
    UNUSED(arg);
    return realloc(ptr, size);
}

static void free_callback(void *arg, void *ptr) {
    UNUSED(arg);
    free(ptr);
}

void arrayInitSized(Array *a, size_t size) {
    arrayInitAllocatorSized(a, allocatorNew(alloc_callback, realloc_callback, free_callback, NULL), size);
}

void arrayInitAllocatorSized(Array *a, Allocator alloc, size_t size) {
    a->allocator = alloc;
    a->used = 0;
    a->capacity = size == 0 ? ARRAY_INITIAL_CAPACITY : size;
    a->data = allocatorAllocate(&a->allocator, sizeof(void *) * a->capacity);
}

void arrayFree(Array *a) {
    allocatorFree(&a->allocator, a->data);
    a->data = NULL;
    a->used = a->capacity = 0;
}

size_t arrayLength(Array *a) {
    return a->used;
}

size_t arrayPush(Array *a, void *value) {
    if(a->used + 1 > a->capacity) {
        a->capacity *= 2;
        a->data = allocatorReallocate(&a->allocator, a->data, sizeof(void *) * a->capacity);
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

void arrayPrepend(Array *a, void *value) {
    arrayPush(a, NULL); // Hack to grow the array if neccesary without duplicating the code in arrayPush().
    // ssize_t to remove the 'comparison of unsigned expression in ‘>= 0’ is always true' warning.
    for(ssize_t i = a->used - 2; i >= 0; --i) {
        a->data[i + 1] = a->data[i];
    }
    a->data[0] = value;
}

void arrayInsert(Array *a, size_t index, void *value) {
    VERIFY(index < a->used);
    a->data[index] = value;
}

void *arrayGet(Array *a, size_t index) {
    if(index >= a->used) {
        return NULL;
    }
    return a->data[index];
}

void *arrayDelete(Array *a, size_t index) {
    VERIFY(index < a->used);
    void *value = arrayGet(a, index);

    for(size_t i = index; i < a->used - 1; ++i) {
        a->data[index] = a->data[index + 1];
    }
    // Set last element (that is now empty) to NULL.
    a->data[--a->used] = NULL;
    return value;
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
    // The division below might result in a float
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
