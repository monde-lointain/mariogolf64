#ifndef C_LANGUAGE_H
#define C_LANGUAGE_H

#include <ultra64.h>

typedef s32 Boolean;
typedef u32 UBoolean;
typedef s16 Boolean16;
typedef u16 UBoolean16;
typedef s8 Boolean8;
typedef u8 UBoolean8;

enum Outcome {
    OUTCOME_FAILED = -1,
    OUTCOME_OK
};

typedef s32 Outcome;

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

#define MemberSize(type, member) (s32)sizeof(((type *)0)->member)
#define MemberOffset(type, member) ((u32) & ((type *)0)->member)

#define BITS_PER_BYTE 8

#define S32_MAX 0x7FFFFFFF

#endif
