/*
 * Reading the big-endian values of data from ROM.
 *
 * FromBigEndian gives the host's value of one, and ConvertFromBigEndian
 * converts a variable or member that holds one in place.
 * ConvertHalvesFromBigEndian and ConvertWordsFromBigEndian convert arrays of
 * two- and four-byte values in place.
 *
 * The N64's CPU is big-endian, as the ROM's data is, so the game reads that
 * data in place. The native port's hosts are little-endian: where the game's
 * code reads a value of more than a byte from data read from ROM, it passes
 * the value through these first. On the N64 they leave it as it is.
 *
 * The port's renderer reads memory that the cartridge filled in the ROM's
 * order. MarkHostOrder tells it that code has written values of its own
 * over such memory, or converted them in place.
 */

#ifndef BIG_ENDIAN_H
#define BIG_ENDIAN_H

#include <ultra64.h>

#if defined(TARGET_PC) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define FromBigEndian16(value) ((u16)__builtin_bswap16((u16)(value)))
#    define FromBigEndian32(value) ((u32)__builtin_bswap32((u32)(value)))
#    define ConvertFromBigEndian16(lvalue) ((lvalue) = FromBigEndian16(lvalue))
#    define ConvertFromBigEndian32(lvalue) ((lvalue) = FromBigEndian32(lvalue))

static inline void ConvertHalvesFromBigEndian(void *start, u32 size) {
    u16 *half = start;
    u32 i;

    for (i = 0; i < size / sizeof(u16); i++) {
        half[i] = FromBigEndian16(half[i]);
    }
}

static inline void ConvertWordsFromBigEndian(void *start, u32 size) {
    u32 *word = start;
    u32 i;

    for (i = 0; i < size / sizeof(u32); i++) {
        word[i] = FromBigEndian32(word[i]);
    }
}
#else
#    define FromBigEndian16(value) (value)
#    define FromBigEndian32(value) (value)
// Statements that would store a value over itself, which KMC GCC compiles
#    define ConvertFromBigEndian16(lvalue) ((void)0)
#    define ConvertFromBigEndian32(lvalue) ((void)0)
#    define ConvertHalvesFromBigEndian(start, size) ((void)0)
#    define ConvertWordsFromBigEndian(start, size) ((void)0)
#endif

#ifdef TARGET_PC
#    define MarkHostOrder(start, size) osPcUnmarkRomOrder((start), (size))
#else
#    define MarkHostOrder(start, size) ((void)0)
#endif

#endif
