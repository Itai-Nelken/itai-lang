#ifndef ARENA_H
#define ARENA_H

#include <stddef.h> // size_t
#include <stdbool.h>

typedef struct block {
	char *data;
	char *current;
	size_t size;
	struct block *next;
} ArenaBlock;

typedef struct arena {
	ArenaBlock *blocks;
	size_t block_size;
	size_t block_count;
	size_t bytes_allocated;
} Arena;

typedef struct arena_info {
	size_t block_count;
	size_t current_block_size;
	size_t free_bytes_in_current_block;
	size_t bytes_allocated_in_current_block;
	size_t total_bytes_allocated;
} ArenaInfo;

/***
 * Initialize a new Arena.
 * 
 * @param a A Arena to initialize.
 * @param block_size the initial size for each block.
 ***/
void arenaInit(Arena *a, size_t block_size);

/***
 * Free an initialized Arena.
 * 
 * @param a An initialized Arena to free.
 ***/
void arenaFree(Arena *a);

/***
 * Get information about an Arena.
 * 
 * @param a An initialized Arena.
 * @return An ArenaInfo struct.
 ***/
ArenaInfo arenaGetInfo(Arena *a);

/***
 * Allocate 'size' bytes from an Arena.
 * An extra byte is added before and after the allocated space.
 * If the current block doesn't have enough space, a new one will be added.
 * 
 * @param a An initialized Arena.
 * @param size The amount of bytes to allocate.
 * @return A pointer to the start of the allocated space.
 ***/
void *arenaAllocate(Arena *a, size_t size);

/***
 * Allocate 'count' elements of 'size' size and set them all to 0.
 * An extra byte is added before and after the allocated space.
 * If the current block doesn't have enough space, a new one will be added.
 * 
 * @param a An initialized Arena.
 * @param count The amount of elements to allocate.
 * @param size The size of each element.
 * @return A pointer to the start of the allocated space.
 ***/
void *arenaClearAllocate(Arena *a, size_t count, size_t size);

#define ARENA_NEW(arena, o) ((o) = arenaAllocate((arena), sizeof(*o)))
#define ARENA_NEW0(arena, o) ((o) = arenaClearAllocate((arena), 1, sizeof(*o)))

#endif // ARENA_H
