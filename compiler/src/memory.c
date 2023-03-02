#include "memory.h"

Allocator allocatorNew(void *(*allocFn)(void *user_data, size_t size), void *(*reallocFn)(void *user_data, void *ptr, size_t size), void (*freeFn)(void *user_data, void *ptr), void *user_data) {
    return (Allocator){
        .allocFn = allocFn,
        .reallocFn = reallocFn,
        .freeFn = freeFn,
        .user_data = user_data
    };
}

void *allocatorAllocate(Allocator *a, size_t size) {
    return a->allocFn(a->user_data, size);
}


void *allocatorReallocate(Allocator *a, void *ptr, size_t size) {
    return a->reallocFn(a->user_data, ptr, size);
}

void allocatorFree(Allocator *a, void *ptr) {
    a->freeFn(a->user_data, ptr);
}

