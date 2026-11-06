#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <ultra64.h>

/*
 * Names the game's C uses as though the language had them: its boolean
 * type, the element count of an array, the size of a member, the bits
 * of a byte and the largest s32
 */

/* Zero is false, and any other value true, as C tests it */
typedef s32 Boolean;

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

/* The size of a struct member, without an instance to apply sizeof to */
#define MemberSize(type, member) (s32)sizeof(((type*)0)->member)

#define BITS_PER_BYTE 8

/* The largest s32 value */
#define S32_MAX 0x7FFFFFFF

#endif
