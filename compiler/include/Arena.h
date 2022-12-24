#ifndef ARENA_H
#define ARENA_H

#include <stddef.h> // size_t
#include "memory.h" // Allocator

// The default block size is 10K
#define ARENA_DEFAULT_BLOCK_SIZE 10 * 1024

typedef struct block Block;

typedef struct arena {
	Block *blocks;
} Arena;

void arenaInit(Arena *a);

void arenaFree(Arena *a);

void *arenaAlloc(Arena *a, size_t size);

void *arenaCalloc(Arena *a, size_t nmemb, size_t size);

Allocator arenaMakeAllocator(Arena *a);

#endif // ARENA_H
