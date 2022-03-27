#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define UNREACHABLE() do { \
		fprintf(stderr, "\n============\nInternal error at %s(): %s:%d: unreachable state!\n============\n", __func__, __FILE__, __LINE__); \
		abort(); \
	} while(0);

#define LOG_ERR(msg) fprintf(stderr, "\x1b[1;31m[ERROR]:\x1b[0m" msg)
#define LOG_ERR_F(msg, ...) fprintf(stderr, "\x1b[1;31m[ERROR]:\x1b[0m" msg, __VA_ARGS__)

#endif // COMMON_H
