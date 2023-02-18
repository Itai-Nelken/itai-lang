#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>

typedef struct allocator {
    void *(*allocFn)(void *arg, size_t size);
    void *(*reallocFn)(void *arg, void *ptr, size_t size);
    void (*freeFn)(void *arg, void *ptr);
    void *user_data;
} Allocator;

/***
 * Create a new Allocator.
 *
 * @param allocFn A callback to allocate memory.
 * @param reallocFn A callback to reallocate memory.
 * @param freeFn A callback to free memory.
 * @param user_data User data to be passed to each of the callbacks.
 ***/
Allocator allocatorNew(void *(*allocFn)(void *user_data, size_t size), void *(*reallocFn)(void *user_data, void *ptr, size_t size), void (*freeFn)(void *user_data, void *ptr), void *user_data);

/***
 * Allocate [size] bytes using Allocator [a].
 *
 * @param a The Allocator to use.
 * @param size The amount of bytes to allocate.
 ***/
void *allocatorAllocate(Allocator *a, size_t size);

/***
 * Resize [ptr] to [size] bytes using Allocator [a].
 *
 * @param a The Allocator to use.
 * @param ptr The pointer to the memory to reallocate.
 * @param size The amount of bytes to allocate.
 ***/
void *allocatorReallocate(Allocator *a, void *ptr, size_t size);

/***
 * Free [ptr] using Allocator [a].
 *
 * @param a The Allocator to use.
 * @param ptr A pointer to the memory to free.
 ***/
void allocatorFree(Allocator *a, void *ptr);

#define ALLOC(size) malloc((size))
#define CALLOC(nmemb, size) calloc((nmemb), (size))
#define REALLOC(ptr, size) realloc((ptr), (size))
#define FREE(p) free((p))

// uses 'o' only once
#define NEW(o) ((o) = ALLOC(sizeof(*o)))
#define NEW0(o) ((o) = CALLOC(1, sizeof(*o)))

#endif // MEMORY_H
