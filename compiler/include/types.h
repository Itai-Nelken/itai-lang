#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h> // for size_t and ssize_t

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

typedef _Float32 f32;
typedef _Float64 f64;

typedef const char *str;

#endif // TYPES_H
