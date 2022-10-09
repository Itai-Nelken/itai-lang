#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>

#define ALLOC(size) malloc((size));
#define CALLOC(nmemb, size) calloc((nmemb), (size))
#define REALLOC(ptr, size) realloc((ptr), (size))
#define FREE(p) free((p))

// uses 'o' only once
#define NEW(o) ((o) = ALLOC(sizeof(*o)))
#define NEW0(o) ((o) = CALLOC(1, sizeof(*o)))

#endif // MEMORY_H
