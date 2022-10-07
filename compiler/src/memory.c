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

static Allocator default_allocator = {
    .allocate = default_allocate,
    .callocate = default_callocate,
    .reallocate = default_reallocate,
    .free = default_free,
    .user_data = NULL
};

static Allocator *allocators[MAX_ALLOCATORS + 1] = {
    [0] = &default_allocator
};

static u32 next_allocator = 1;

AllocatorID allocatorAddAllocator(Allocator *a) {
    if(next_allocator > MAX_ALLOCATORS) {
        LOG_ERR("Maximum number of allocators reached!\n");
        UNREACHABLE();
    }

    allocators[next_allocator] = a;
    return next_allocator++;
}

static Allocator *get_allocator(AllocatorID id) {
    VERIFY(id < next_allocator);
    return allocators[id];
}

void *allocatorAllocate(AllocatorID id, usize size) {
    Allocator *a = get_allocator(id);
    VERIFY(a->allocate);
    return a->allocate(a->user_data, size);
}

void *allocatorCAllocate(AllocatorID id, usize nmemb, usize size) {
    Allocator *a = get_allocator(id);
    VERIFY(a->callocate);
    return a->callocate(a->user_data, nmemb, size);
}

void *allocatorReallocate(AllocatorID id, void *ptr, usize size) {
    Allocator *a = get_allocator(id);
    VERIFY(a->reallocate);
    return a->reallocate(a->user_data, ptr, size);
}

void allocatorFree(AllocatorID id, void *ptr) {
    Allocator *a = get_allocator(id);
    VERIFY(a->free);
    a->free(a->user_data, ptr);
}
