#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h> // size_t
#include <sys/types.h> // ssize_t
#include <stdbool.h>
#include <stdint.h>
#include "utilities.h"

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef ssize_t isize;
typedef size_t usize;

typedef float f32;
typedef double f64;

// TODO: remove in release builds (after checking that not used anywhere with side effects.)
#define VERIFY(x) ((x) ? ((void)0) : assertFail(#x, __FILE__, __LINE__, __func__))

#define UNREACHABLE() do { \
        fprintf(stderr, "\n============\nInternal error at %s(): %s:%d: unreachable state!\n============\n", __func__, __FILE__, __LINE__); \
        abort(); \
    } while(0);

#define UNUSED(a) ((void)a)

// the ## before __VA_ARGS__ removes the comma if there are no vaargs.
#define LOG_ERR(msg, ...) fprintf(stderr, "\x1b[1;31m[ERROR]:\x1b[0m " msg, ##__VA_ARGS__)
#define LOG_MSG(msg, ...) fprintf(stderr, "\x1b[1;36m[MSG]:\x1b[0m " msg, ##__VA_ARGS__)

#endif // COMMON_H
