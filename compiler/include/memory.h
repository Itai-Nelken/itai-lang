#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

typedef struct allocator {
    void *(*allocate)(void *user_data, usize size);
    void *(*callocate)(void *user_data, usize nmemb, usize size);
    void *(*reallocate)(void *user_data, void *ptr, usize size);
    void (*free)(void *user_data, void *ptr);
    void *user_data;
} Allocator;

void *allocatorAllocate(Allocator *a, usize size);
void *allocatorCAllocate(Allocator *a, usize nmemb, usize size);
void *allocatorReallocate(Allocator *a, void *ptr, usize size);
void allocatorFree(Allocator *a, void *ptr);

extern Allocator __default_allocator;

#define ALLOC(size) allocatorAllocate(&__default_allocator, (size));
#define CALLOC(nmemb, size) allocatorCAllocate(&__default_allocator, (nmemb), (size))
#define REALLOC(ptr, size) allocatorReallocate(&__default_allocator, (ptr), (size))
#define FREE(p) allocatorFree(&__default_allocator, (p))

// uses 'o' only once
#define NEW(o) ((o) = ALLOC(sizeof(*o)))
#define NEW0(o) ((o) = CALLOC(1, sizeof(*o)))

#endif // MEMORY_H
