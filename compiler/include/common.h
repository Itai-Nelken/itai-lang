#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef __int128_t i128;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef __uint128_t u128;

typedef ssize_t isize;
typedef size_t usize;

typedef float f32;
typedef double f64;

typedef const char *str;

#define UNREACHABLE() do { \
		fprintf(stderr, "\n============\nInternal error at %s(): %s:%d: unreachable state!\n============\n", __func__, __FILE__, __LINE__); \
		abort(); \
	} while(0);

#define UNUSED(a) ((void)a)

// the ## before __VA_ARGS__ removes the comma if there are no vaargs.
#define LOG_ERR(msg, ...) fprintf(stderr, "\x1b[1;31m[ERROR]:\x1b[0m " msg, ##__VA_ARGS__)

#endif // COMMON_H
