#pragma once
#ifndef UTILS_H
#define UTILS_H

#define ASSERT(expression) if(!(expression)) {*(int *)0 = 0;}

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

typedef u8 b8;
typedef u16 b16;
typedef u32 b32;
typedef u64 b64;

#define TRUE  1
#define FALSE 0

#define NULL 0

typedef size_t size;

typedef struct Color
{
    u8 r, g, b, a;
};

#define RGB(r, g, b) (((r) << 16) | ((g) << 8) | ((b) << 0))
#define ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | ((b) << 0))
#define RGBA(r, g, b, a) (((r) << 24) | ((g) << 16) | ((b) << 8) | ((a) << 0))
#define BGR(b, g, r) (((b) << 16) | ((g) << 8) | ((r) << 0))
#define ABGR(a, b, g, r) (((a) << 24) | ((b) << 16) | ((g) << 8) | ((r) << 0))
#define BGRA(b, g, r, a) (((b) << 24) | ((g) << 16) | ((r) << 8) | ((a) << 0))

#define GetBitFlag(v, f) ((v) & (1 << (f)) ? 1 : 0)
#define SetBitFlag(v, f) (*(v) = (*(v) | (1 << (f))))
#define ClearBitFlag(v, f) (*(v) = *(v) ^ (*(v) & (1 << (f))))

inline void* Allocate(size size)
{
    return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

inline void Free(void *address)
{
    VirtualFree(address, 0, MEM_RELEASE);
}

struct LoadedFile
{
    u64 size;
    u8 *contents;
};

inline u64 fsize(FILE *file)
{
    u64 pos = ftell(file);
    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    fseek(file, pos, SEEK_SET);
    return size;
}

inline LoadedFile LoadEntireFile(char *filePath)
{
    LoadedFile file = {};

    FILE *in = fopen(filePath, "rb");
    if (in)
    {
        file.size = fsize(in);
        file.contents = (u8*)Allocate(file.size);
        fread(file.contents, 1, file.size, in);
        fclose(in);
    }

    return file;
}

struct LoadedBitmap
{
    u32 width, height;
    s32 xoffset, yoffset;
    Color *pixels;
};

#endif
