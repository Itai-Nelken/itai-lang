#include <string.h>
#include <stdbool.h>
#include "memory.h"
#include "Arena.h"

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

static ArenaBlock *new_block(size_t size) {
	ArenaBlock *b = CALLOC(1, sizeof(*b));
	b->data = CALLOC(size, sizeof(*b->data));
	b->current = b->data;
	b->size = size;
	b->next = NULL;
	return b;
}

static void free_blocks(ArenaBlock *blocks) {
	while(blocks != NULL) {
		ArenaBlock *next = blocks->next;
		FREE(blocks->data);
		FREE(blocks);
		blocks = next;
	}
}

void arenaInit(Arena *a, size_t block_size) {
	a->blocks = new_block(block_size);
	a->block_size = block_size;
	a->block_count = 1;
	a->bytes_allocated = 0;
}

void arenaFree(Arena *a) {
	free_blocks(a->blocks);
	a->blocks = NULL;
	a->block_size = a->block_count = a->bytes_allocated = 0;
}

static void add_block(Arena *a, size_t size_needed) {
	if(size_needed >= a->block_size) {
		a->block_size += size_needed;
	}
	ArenaBlock *b = new_block(a->block_size);
	a->blocks->next = b;
	a->blocks = b;
	a->block_count++;
}

ArenaInfo arenaGetInfo(Arena *a) {
	ArenaInfo info = {
		.block_count = a->block_count,
		.current_block_size = a->block_size,
		.free_bytes_in_current_block = (size_t)(a->blocks->size - (a->blocks->current - a->blocks->data)),
		.bytes_allocated_in_current_block = (size_t)(a->blocks->current - a->blocks->data),
		.total_bytes_allocated = a->bytes_allocated
	};
	return info;
}

static void *allocate(Arena *a, size_t element_count, size_t element_size, size_t *total_size) {
	// round the size up to an alignment boundary.
	size_t size = (((element_count * element_size) + sizeof(union align) - 1) / (sizeof(union align)) * sizeof(union align));
	if((size_t)((a->blocks->current + size) - a->blocks->data) > a->blocks->size) {
		add_block(a, size);
	}
	a->bytes_allocated += size;
	a->blocks->current += size + 2 * element_size;
	if(total_size) {
		*total_size = size;
	}
	return (void *)(a->blocks->current - size + element_size);
}

void *arenaAllocate(Arena *a, size_t size) {
	return allocate(a, size, 1, NULL);
}

// actually necessary? blocks are calloc()'ed so they are zeroed already.
void *arenaClearAllocate(Arena *a, size_t count, size_t size) {
	size_t total_size = 0;
	void *ptr = allocate(a, count, size, &total_size);
	if(ptr) {
		memset(ptr - size, 0, total_size);
	}
	return ptr + 1;
}
