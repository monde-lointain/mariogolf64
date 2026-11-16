/*
 * Names that the game's C uses as though the language had them: boolean
 * types, the result of an operation that can fail, and the sizes of arrays,
 * members and bytes.
 *
 * An enum whose values variables hold is paired with a typedef of the
 * integer type that the original stores them in, which the variables take.
 * The enum's own type would be an int, and unsigned if no constant is
 * negative, either of which compiles to different code from the original's.
 */

#ifndef C_LANGUAGE_H
#define C_LANGUAGE_H

#include <ultra64.h>
#ifdef TARGET_PC
#    include <stddef.h>
#endif

// Zero is false and any other value true, as C tests it. The sized forms
// match the original's storage for a flag.
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
// The size of a struct member, without an instance to apply sizeof to
#define MemberSize(type, member) (s32)sizeof(((type *)0)->member)
// How many bytes into a struct a member starts. The original takes the
// member's address in a struct at zero, which a host's wider pointers would
// truncate.
#ifdef TARGET_PC
#    define MemberOffset(type, member) ((u32)offsetof(type, member))
#else
#    define MemberOffset(type, member) ((u32) & ((type *)0)->member)
#endif

#define BITS_PER_BYTE 8
// The largest s32 value
#define S32_MAX 0x7FFFFFFF

#endif
