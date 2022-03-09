#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define UNREACHABLE() do { \
		fprintf(stderr, "\n============\nInternal error at %s(): %s:%d: unreachable state!\n============\n", __func__, __FILE__, __LINE__); \
		abort(); \
	} while(0);

#endif // COMMON_H
