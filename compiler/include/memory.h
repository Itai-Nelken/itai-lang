#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

// Always add one more than needed for the default allocator.
#define MAX_ALLOCATORS 3

typedef struct allocator {
    void *(*allocate)(void *user_data, usize size);
    void *(*callocate)(void *user_data, usize nmemb, usize size);
    void *(*reallocate)(void *user_data, void *ptr, usize size);
    void (*free)(void *user_data, void *ptr);
    void *user_data;
} Allocator;

typedef u32 AllocatorID;

AllocatorID allocatorAddAllocator(Allocator *a);

void *allocatorAllocate(AllocatorID id, usize size);
void *allocatorCAllocate(AllocatorID id, usize nmemb, usize size);
void *allocatorReallocate(AllocatorID id, void *ptr, usize size);
void allocatorFree(AllocatorID id, void *ptr);

// 0 is the default allocators AllocatorID.
#define ALLOC(size) allocatorAllocate(0, (size));
#define CALLOC(nmemb, size) allocatorCAllocate(0, (nmemb), (size))
#define REALLOC(ptr, size) allocatorReallocate(0, (ptr), (size))
#define FREE(p) allocatorFree(0, (p))

// uses 'o' only once
#define NEW(o) ((o) = ALLOC(sizeof(*o)))
#define NEW0(o) ((o) = CALLOC(1, sizeof(*o)))

#endif // MEMORY_H
