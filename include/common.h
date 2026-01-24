#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef float f32;
typedef double f64;

typedef struct {
    f32 x, y, z;
} Vec3f;

typedef struct {
    s32 x, y, z;
} Vec3i;

typedef struct LzDecompressState {
    /* 0x00 */ u8* src_cur;         // current read position
    /* 0x04 */ u8* dst_start;       // output buffer start
    /* 0x08 */ u8* dst_cur;         // current write position
    /* 0x0C */ u8* dst_alt;         // alternate output ptr (double-buffer)
    /* 0x10 */ u8* src_end;         // end of compressed data
    /* 0x14 */ u16 flags;           // bit 0: buffer select, bit 1: final chunk
    /* 0x18 */ u8* ring_buffer;     // for extended variant
    /* 0x1C */ u16 ring_index;      // ring buffer write index
    /* 0x1E */ u16 ring_read_idx;   // ring buffer read index
    /* 0x20 */ u16 pending_count;   // pending copy operations
    /* 0x24 */ u8* pending_dst;     // for extended: secondary output
} LzDecompressState; // size >= 0x28

#endif /* COMMON_H */
