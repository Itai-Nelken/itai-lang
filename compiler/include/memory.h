#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>

#define ALLOC(size) malloc(size)
#define CALLOC(nmemb, size) calloc(nmemb, size)
#define REALLOC(ptr, size) realloc(ptr, size)
#define FREE(p) free(p)

#endif // MEMORY_H
