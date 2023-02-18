#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "memory.h" // Allocator
#include "Arena.h"

typedef struct block {
    struct block *prev;
    size_t size;
    size_t used;
    char data[];
} Block;

union align {
    int i;
    long l;
    long *lp;
    void *p;
    void (*fp)(void);
    float f;
    double d;
    long double ld;
};

union header {
    Block block;
    union align a;
};

static inline Block *new_block(size_t size, Block *prev) {
    Block *b = malloc(sizeof(union header) + size);
    b->size = size;
    b->used = 0;
    b->prev = prev;
    return b;
}

static inline void free_blocks(Block *b) {
    while(b) {
        Block *prev = b->prev;
        free(b);
        b = prev;
    }
}


void arenaInit(Arena *a) {
    a->blocks = new_block(ARENA_DEFAULT_BLOCK_SIZE, NULL);
}

void arenaFree(Arena *a) {
    free_blocks(a->blocks);
    a->blocks = NULL;
}

static inline size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

void *arenaAlloc(Arena *a, size_t size) {
    size = (size + sizeof(union align) - 1) / sizeof(union align) * sizeof(union align);
    if(a->blocks->used + size > a->blocks->size) {
        // All blocks have at least 10K of memory.
        a->blocks = new_block(max(size, ARENA_DEFAULT_BLOCK_SIZE), a->blocks);
    }
    a->blocks->used += size;
    return (void *)(a->blocks->data + a->blocks->used - size);
}

void *arenaCalloc(Arena *a, size_t nmemb, size_t size) {
    void *p = arenaAlloc(a, nmemb * size);
    memset(p, 0, nmemb * size);
    return p;
}

static void *alloc_callback(void *arena, size_t size) {
    return arenaCalloc((Arena *)arena, 1, size);
}

static void *realloc_callback(void *arena, void *ptr, size_t size) {
    UNUSED(arena);
    UNUSED(ptr);
    UNUSED(size);
    LOG_ERR("Arena doesn't support reallocation!\n");
    UNREACHABLE();
}

static void free_callback(void *arena, void *ptr) {
    UNUSED(arena);
    UNUSED(ptr);
    // Unlike realloc(), freeing memory in an arena will happen.
    // so just do nothing.
}

Allocator arenaMakeAllocator(Arena *a) {
    return allocatorNew(alloc_callback,
                        realloc_callback,
                        free_callback,
                        (void *)a);
}
