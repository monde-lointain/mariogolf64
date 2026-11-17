/*
 * The decoders of the game's two kinds of LZ-compressed data, which the
 * loaders feed a chunk at a time. Both read and write 16-bit words, which
 * the ROM holds big-endian.
 */

#ifndef LZ_DECODER_H
#define LZ_DECODER_H

#include <ultra64.h>

#define LZ_STREAM_FLAG_FIRST (1 << 0)
#define LZ_STREAM_FLAG_LAST (1 << 1)
typedef u16 ULzStreamFlags16;
#define LZ_HEADER_WORD_COUNT 2
#define LZ_LITERAL_RUN_LENGTH 16
#define LZ_WINDOW_MASK 0x7FF
#define LZ_HISTORY_MASK 0xFF
#define LZ_PATCH_KIND_LITERALS 0x8000
// A reference word holds a distance back into the output, then a length less
// than the copy's
#define LZ_REFERENCE_LENGTH_MASK 0x1F
#define LZ_REFERENCE_DISTANCE_SHIFT 5
#define LZ_REFERENCE_LENGTH_MIN 2

// A control word's 16 bits say whether each item is a literal or a reference.
// They are shifted up to the top of controlBits, with LZ_CONTROL_END_BIT set
// below them; once that bit is shifted out, the next word is read.
// LZ_SPENT_CONTROL_BITS is that bit alone at the top, which the next shift
// shifts out.
#define LZ_CONTROL_BIT_COUNT 16
#define LZ_CONTROL_END_BIT 0x8000
#define LZ_SPENT_CONTROL_BITS 0x80000000

// A patch command holds its kind or offset in its high byte and its count less
// one in its low byte
#define LZ_PATCH_COUNT_MASK 0xFF
#define LZ_PATCH_KIND_MASK 0xFF00
#define LZ_PATCH_OFFSET_SHIFT 8

typedef struct LzStream {
    u16 *input;
    u16 *outputStart;
    u16 *nextInput;
    u16 *output;
    u16 *inputEnd;
    ULzStreamFlags16 flags;
    u16 *window;
    u16 windowIndex;
    u16 patchCount;
    u16 historyIndex;
    u16 *history;
} LzStream;

s32 DecodeLzChunk(LzStream *stream);
s32 DecodePatchedLzChunk(LzStream *stream);

#endif
