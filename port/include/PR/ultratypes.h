/*
 * The fixed-width types of libultra's interfaces, for a 64-bit host.
 *
 * libultra defines s32 and u32 as long, which is 64 bits wide on LP64 hosts
 * such as Linux and macOS. The game's code and the data that it reads from
 * ROM need them 32 bits wide, so this header stands in for libultra's and
 * takes the widths from <stdint.h>.
 */

#ifndef _ULTRATYPES_H_
#define _ULTRATYPES_H_

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;

typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

typedef float f32;
typedef double f64;

#ifndef TRUE
#    define TRUE 1
#endif

#ifndef FALSE
#    define FALSE 0
#endif

#ifndef NULL
#    define NULL ((void *)0)
#endif

#endif
