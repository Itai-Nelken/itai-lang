#include <stdlib.h>
#include "common.h"
#include "memory.h"

static void *default_allocate(void *user_data, usize size) {
    UNUSED(user_data);
    return malloc(size);
}

static void *default_callocate(void *user_data, usize nmemb, usize size) {
    UNUSED(user_data);
    return calloc(nmemb, size);
}

static void *default_reallocate(void *user_data, void *ptr, usize size) {
    UNUSED(user_data);
    return realloc(ptr, size);
}

static void default_free(void *user_data, void *ptr) {
    UNUSED(user_data);
    free(ptr);
}

Allocator __default_allocator = {
    .allocate = default_allocate,
    .callocate = default_callocate,
    .reallocate = default_reallocate,
    .free = default_free,
    .user_data = NULL
};

void *allocatorAllocate(Allocator *a, usize size) {
    return a->allocate(a->user_data, size);
}

void *allocatorCAllocate(Allocator *a, usize nmemb, usize size) {
    return a->callocate(a->user_data, nmemb, size);
}

void *allocatorReallocate(Allocator *a, void *ptr, usize size) {
    return a->reallocate(a->user_data, ptr, size);
}

void allocatorFree(Allocator *a, void *ptr) {
    a->free(a->user_data, ptr);
}
