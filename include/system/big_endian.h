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
 * over such memory, or converted them in place, and MarkRomOrder that code
 * has copied or unpacked the ROM's data into memory.
 *
 * StoreBigEndian16 and StoreRomCommand write a value or a display list
 * command into such data in its own order and form: the N64's eight-byte
 * commands, whose words a host's Gfx holds at its own width.
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
// The N64's values are already in its order, so these do nothing
#    define ConvertFromBigEndian16(lvalue) ((void)0)
#    define ConvertFromBigEndian32(lvalue) ((void)0)
#    define ConvertHalvesFromBigEndian(start, size) ((void)0)
#    define ConvertWordsFromBigEndian(start, size) ((void)0)
#endif

#ifdef TARGET_PC
#    define MarkHostOrder(start, size) osPcUnmarkRomOrder((start), (size))
#    define MarkRomOrder(start, size) osPcMarkRomOrder((start), (size))

static inline void StoreBigEndian16(void *destination, u16 value) {
    u8 *bytes = destination;

    bytes[0] = (u8)(value >> 8);
    bytes[1] = (u8)value;
}

static inline void StoreBigEndian32(void *destination, u32 value) {
    u8 *bytes = destination;

    bytes[0] = (u8)(value >> 24);
    bytes[1] = (u8)(value >> 16);
    bytes[2] = (u8)(value >> 8);
    bytes[3] = (u8)value;
}

// The N64's commands take the low 32 bits of each word: a host's address
// can't go in one, only an N64 address that the renderer resolves
static inline void StoreRomCommand(void *destination, u32 firstWord, u32 secondWord) {
    StoreBigEndian32(destination, firstWord);
    StoreBigEndian32((u8 *)destination + sizeof(u32), secondWord);
}

// Puts an array of u16 words that only the RDP reads, such as 4-bit texels
// packed into words, into the cartridge's byte order in place, which is the
// order the RDP reads texels of 8 bits and fewer in. Call it once.
static inline void PutHalvesInRomOrder(void *start, u32 size) {
    // Converting from big-endian swaps the bytes, which is its own inverse
    ConvertHalvesFromBigEndian(start, size);
    MarkRomOrder(start, size);
}

#    define ROM_COMMAND_SIZE 8

static inline void StoreRomCommands(void *destination, const Gfx *commands, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        StoreRomCommand(
            (u8 *)destination + (i * ROM_COMMAND_SIZE),
            (u32)commands[i].words.w0,
            (u32)commands[i].words.w1
        );
    }
}
#else
#    define MarkHostOrder(start, size) ((void)0)
#    define MarkRomOrder(start, size) ((void)0)
#    define PutHalvesInRomOrder(start, size) ((void)0)
#endif

#endif
