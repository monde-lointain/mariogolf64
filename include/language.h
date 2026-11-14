#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <ultra64.h>

typedef s32 Boolean;
// TRUE or FALSE kept at another width or signedness, where the original's
// struct layout or code fixes them.
typedef u32 UBoolean;
typedef s16 Boolean16;
typedef u16 UBoolean16;
typedef s8 Boolean8;
typedef u8 UBoolean8;

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

#define MemberSize(type, member) (s32)sizeof(((type *)0)->member)
#define MemberOffset(type, member) ((u32) & ((type *)0)->member)

#define BITS_PER_BYTE 8

#define S32_MAX 0x7FFFFFFF

#endif
