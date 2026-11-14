#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <ultra64.h>

typedef s32 Boolean;
// TRUE or FALSE kept in a byte, where the original's struct layout or
// code fixes the width and signedness.
typedef s8 Boolean8;
typedef u8 UBoolean8;

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

#define MemberSize(type, member) (s32)sizeof(((type *)0)->member)
#define MemberOffset(type, member) ((u32) & ((type *)0)->member)

#define BITS_PER_BYTE 8

#define S32_MAX 0x7FFFFFFF

#endif
