#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#define ASSERT(expression) assert(expression)

#define KILOBYTES(x) ((x) * 1024)
#define MEGABYTES(x) ((x) * 1024 * 1024)
#define GIGABYTES(x) ((x) * 1024 * 1024 * 1024)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BIT0_MASK 0x01
#define BIT1_MASK 0x02
#define BIT2_MASK 0x04
#define BIT3_MASK 0x08
#define BIT4_MASK 0x10
#define BIT5_MASK 0x20
#define BIT6_MASK 0x40
#define BIT7_MASK 0x80

#define U8LOW_MASK 0x0F
#define U8HIGH_MASK 0xF0
#define U16LOW_MASK 0xFF
#define U16HIGH_MASK 0xFF00

#define ISNEG(x) ((x) & 0x80)
#define HAS_FLAG(x, mask) (((x) & (mask)) == (mask))
#define ISBETWEEN(x, a, b) ((x) >= (0) && (x) < (b))

#define internal static
#define global static
#define local static

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef size_t size;

typedef struct Color {
    u8 r, g, b, a;
} Color;

#define GetBitFlag(v, f) ((v) & (1 << (f)) ? 1 : 0)
#define SetBitFlag(v, f) (*(v) = (*(v) | (1 << (f))))
#define ClearBitFlag(v, f) (*(v) = *(v) ^ (*(v) & (1 << (f))))

static inline void* Allocate(size size)
{
    return malloc(size);
}

static inline void Free(void* address)
{
    free(address);
}

#endif // UTILS_H
