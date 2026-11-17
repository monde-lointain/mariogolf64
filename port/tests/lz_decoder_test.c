/*
 * Tests of the decoder that unpacks the holes' LZ-compressed data, with
 * streams worked out by hand from its format. Each starts with two header
 * words, which the decoder skips. A control word's bits, from the top, mark
 * the items that follow as literal words (0) or references (1); a control
 * word of 0 marks a run of 16 literals instead. A reference holds a distance
 * back into the output above its low 5 bits, and in those a length less 2;
 * a reference of 0 ends the stream.
 */

#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "course/lz_decoder.h"
#include "system/c_language.h"
#include "test_support.h"

#define HEADER_WORD 0xABCD
#define END_REFERENCE 0x0000
// Where the output is left unwritten
#define UNWRITTEN_BYTE 0xEE
#define UNWRITTEN_WORD 0xEEEE
#define OUTPUT_WORD_CAPACITY 2200
// The words that LoadLzData leaves room for before each chunk, which
// DecodeLzChunk carries the end of one chunk into
#define CARRY_WORD_COUNT 16

// References as the format packs them
#define Reference(distance, length) (((distance) << 5) | ((length) - 2))

// The streams are packed as the ROM holds them, big-endian, a byte at a
// time, so that the buffers start with no type, as DMA leaves them
static u8 *PackWords(const u16 *words, s32 count) {
    u8 *bytes = malloc((size_t)count * sizeof(u16));
    s32 i;

    for (i = 0; i < count; i++) {
        bytes[i * 2] = (u8)(words[i] >> 8);
        bytes[i * 2 + 1] = (u8)words[i];
    }
    return bytes;
}

static u8 *AllocUnwritten(s32 wordCount) {
    u8 *bytes = malloc((size_t)wordCount * sizeof(u16));

    memset(bytes, UNWRITTEN_BYTE, (size_t)wordCount * sizeof(u16));
    return bytes;
}

static u8 *AllocOutput(void) {
    return AllocUnwritten(OUTPUT_WORD_CAPACITY);
}

static u16 ReadWord(const u8 *bytes, s32 index) {
    return (u16)((bytes[index * 2] << 8) | bytes[index * 2 + 1]);
}

// Packs a chunk into a buffer as LoadLzData lays them out, after room for
// the words carried into it, which the decoder's bounds point into
static u8 *PackChunk(const u16 *words, s32 count) {
    u8 *buffer = AllocUnwritten(CARRY_WORD_COUNT + count);
    u8 *packed = PackWords(words, count);

    memcpy(buffer + CARRY_WORD_COUNT * sizeof(u16), packed, (size_t)count * sizeof(u16));
    free(packed);
    return buffer;
}

// Decodes a whole stream from one buffer, as LoadLzData does its last chunk
static s32 DecodeWholeStream(const u16 *words, s32 count, u8 *output) {
    u8 *buffer = PackChunk(words, count);
    LzStream stream;
    s32 size;

    stream.input = (u16 *)buffer + CARRY_WORD_COUNT;
    stream.inputEnd = stream.input + count;
    stream.nextInput = NULL;
    stream.outputStart = (u16 *)output;
    stream.flags = LZ_STREAM_FLAG_FIRST | LZ_STREAM_FLAG_LAST;
    size = DecodeLzChunk(&stream);
    free(buffer);
    return size;
}

// Whether the output starts with the expected words and stops after them
static Boolean HasOutput(const u8 *output, const u16 *expected, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (ReadWord(output, i) != expected[i]) {
            return FALSE;
        }
    }
    return ReadWord(output, count) == UNWRITTEN_WORD;
}

// The control word's 0 bits copy the literals in order
static void TestCopiesLiterals(void) {
    static const u16 stream[] = {HEADER_WORD, HEADER_WORD, 0x2000, 0x1234, 0x5678, END_REFERENCE};
    static const u16 expected[] = {0x1234, 0x5678};
    u8 *output = AllocOutput();

    EXPECT(DecodeWholeStream(stream, ARRAY_COUNT(stream), output) == sizeof(expected));
    EXPECT(HasOutput(output, expected, ARRAY_COUNT(expected)));
    free(output);
}

// A reference longer than its distance copies the words it has just written
static void TestCopiesOverlappingReferences(void) {
    static const u16 stream[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x1800,
        0x0A0A,
        0x0B0B,
        0x0C0C,
        Reference(3, 4),
        END_REFERENCE,
    };
    static const u16 expected[] = {0x0A0A, 0x0B0B, 0x0C0C, 0x0A0A, 0x0B0B, 0x0C0C, 0x0A0A};
    u8 *output = AllocOutput();

    EXPECT(DecodeWholeStream(stream, ARRAY_COUNT(stream), output) == sizeof(expected));
    EXPECT(HasOutput(output, expected, ARRAY_COUNT(expected)));
    free(output);
}

// The shortest distance with the shortest length repeats the last word twice
static void TestRepeatsTheLastWord(void) {
    static const u16 stream[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x6000,
        0x0102,
        Reference(1, 2),
        END_REFERENCE,
    };
    static const u16 expected[] = {0x0102, 0x0102, 0x0102};
    u8 *output = AllocOutput();

    EXPECT(DecodeWholeStream(stream, ARRAY_COUNT(stream), output) == sizeof(expected));
    EXPECT(HasOutput(output, expected, ARRAY_COUNT(expected)));
    free(output);
}

// A control word of 0 copies the next 16 words in order, even ones and odd
static void TestCopiesLiteralRuns(void) {
    u16 stream[2 + 1 + 16 + 2];
    u16 expected[16];
    u8 *output = AllocOutput();
    s32 i;

    stream[0] = HEADER_WORD;
    stream[1] = HEADER_WORD;
    stream[2] = 0x0000;
    for (i = 0; i < 16; i++) {
        expected[i] = (u16)(0x1100 * (i + 1) + i);
        stream[3 + i] = expected[i];
    }
    stream[19] = 0x8000;
    stream[20] = END_REFERENCE;
    EXPECT(DecodeWholeStream(stream, ARRAY_COUNT(stream), output) == sizeof(expected));
    EXPECT(HasOutput(output, expected, ARRAY_COUNT(expected)));
    free(output);
}

// Once a control word's 16th item is decoded, the next word is read as
// control bits; this one's last two items are a reference and a literal
static void TestReadsTheNextControlWord(void) {
    u16 stream[2 + 1 + 14 + 2 + 2];
    u16 expected[17];
    u8 *output = AllocOutput();
    s32 i;

    stream[0] = HEADER_WORD;
    stream[1] = HEADER_WORD;
    stream[2] = 0x0002;
    for (i = 0; i < 14; i++) {
        expected[i] = (u16)(0x0100 + i);
        stream[3 + i] = expected[i];
    }
    stream[17] = Reference(1, 2);
    expected[14] = expected[13];
    expected[15] = expected[13];
    stream[18] = 0x7777;
    expected[16] = 0x7777;
    stream[19] = 0x8000;
    stream[20] = END_REFERENCE;
    EXPECT(DecodeWholeStream(stream, ARRAY_COUNT(stream), output) == sizeof(expected));
    EXPECT(HasOutput(output, expected, ARRAY_COUNT(expected)));
    free(output);
}

// The farthest and longest reference, 0xFFFF, copies 33 words from 2047
// back; its distance fills the top bit, which a signed reference would
// sign-extend
static void TestCopiesTheFarthestLongestReference(void) {
    enum {
        RUN_COUNT = 128,
        LITERAL_COUNT = RUN_COUNT * 16
    };

    u16 *stream = malloc((2 + RUN_COUNT * 17 + 3) * sizeof(u16));
    u16 *expected = malloc((LITERAL_COUNT + 33) * sizeof(u16));
    u8 *output = AllocOutput();
    s32 count = 0;
    s32 run;
    s32 i;

    stream[count++] = HEADER_WORD;
    stream[count++] = HEADER_WORD;
    for (run = 0; run < RUN_COUNT; run++) {
        stream[count++] = 0x0000;
        for (i = 0; i < 16; i++) {
            expected[run * 16 + i] = (u16)(run * 16 + i);
            stream[count++] = expected[run * 16 + i];
        }
    }
    stream[count++] = 0xC000;
    stream[count++] = 0xFFFF;
    stream[count++] = END_REFERENCE;
    for (i = 0; i < 33; i++) {
        expected[LITERAL_COUNT + i] = (u16)(LITERAL_COUNT - 2047 + i);
    }
    EXPECT(DecodeWholeStream(stream, count, output) == (LITERAL_COUNT + 33) * (s32)sizeof(u16));
    EXPECT(HasOutput(output, expected, LITERAL_COUNT + 33));
    free(output);
    free(expected);
    free(stream);
}

// A chunk that runs low moves its last words to just before the next
// chunk's buffer, in order, and the next call carries on from them
static void TestCarriesInputIntoTheNextChunk(void) {
    static const u16 firstChunk[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x0000,
        0x1001,
        0x1002,
        0x1003,
        0x1004,
        0x1005,
        0x1006,
        0x1007,
        0x1008,
        0x1009,
        0x100A,
        0x100B,
        0x100C,
        0x100D,
        0x100E,
        0x100F,
        0x1010,
        0x3000,
        0x2001,
    };
    static const u16 secondChunk[] = {0x2002, Reference(2, 2), END_REFERENCE};
    static const u16 expected[] = {
        0x1001,
        0x1002,
        0x1003,
        0x1004,
        0x1005,
        0x1006,
        0x1007,
        0x1008,
        0x1009,
        0x100A,
        0x100B,
        0x100C,
        0x100D,
        0x100E,
        0x100F,
        0x1010,
        0x2001,
        0x2002,
        0x2001,
        0x2002,
    };
    u8 *buffers[2] = {
        AllocUnwritten(CARRY_WORD_COUNT + ARRAY_COUNT(firstChunk)),
        AllocUnwritten(CARRY_WORD_COUNT + ARRAY_COUNT(firstChunk))
    };
    u8 *packed;
    u16 *nextInput = (u16 *)buffers[1] + CARRY_WORD_COUNT;
    u8 *output = AllocOutput();
    LzStream stream;

    packed = PackWords(firstChunk, ARRAY_COUNT(firstChunk));
    memcpy(buffers[0] + CARRY_WORD_COUNT * sizeof(u16), packed, sizeof(firstChunk));
    free(packed);
    packed = PackWords(secondChunk, ARRAY_COUNT(secondChunk));
    memcpy(nextInput, packed, sizeof(secondChunk));
    free(packed);

    stream.input = (u16 *)buffers[0] + CARRY_WORD_COUNT;
    stream.inputEnd = stream.input + ARRAY_COUNT(firstChunk);
    stream.nextInput = nextInput;
    stream.outputStart = (u16 *)output;
    stream.flags = LZ_STREAM_FLAG_FIRST;
    EXPECT(DecodeLzChunk(&stream) == -1);
    EXPECT(stream.input == nextInput - 2);
    EXPECT(stream.output == (u16 *)output + 16);
    EXPECT(ReadWord((const u8 *)(nextInput - 2), 0) == 0x3000);
    EXPECT(ReadWord((const u8 *)(nextInput - 2), 1) == 0x2001);

    stream.inputEnd = nextInput + ARRAY_COUNT(secondChunk);
    stream.nextInput = (u16 *)buffers[0] + CARRY_WORD_COUNT;
    stream.flags = LZ_STREAM_FLAG_LAST;
    EXPECT(DecodeLzChunk(&stream) == sizeof(expected));
    EXPECT(HasOutput(output, expected, ARRAY_COUNT(expected)));
    free(output);
    free(buffers[1]);
    free(buffers[0]);
}

// Decodes a stream's first chunk, which more follow, with the next
// chunk's buffer at nextInput
static s32 DecodeFirstChunk(
    LzStream *stream,
    const u16 *words,
    s32 count,
    u16 *nextInput,
    u8 *output
) {
    u8 *buffer = PackChunk(words, count);
    s32 size;

    stream->input = (u16 *)buffer + CARRY_WORD_COUNT;
    stream->inputEnd = stream->input + count;
    stream->nextInput = nextInput;
    stream->outputStart = (u16 *)output;
    stream->flags = LZ_STREAM_FLAG_FIRST;
    size = DecodeLzChunk(stream);
    free(buffer);
    return size;
}

// A chunk carries its words over once fewer than 17 are left, the most that
// a control word and its run can take: with 17 left it decodes the run, and
// with 16 it carries them all
static void TestCarriesOnceARunMightNotFit(void) {
    u16 stream[2 + 1 + 16];
    u8 *nextBuffer = AllocUnwritten(CARRY_WORD_COUNT + 1);
    u16 *nextInput = (u16 *)nextBuffer + CARRY_WORD_COUNT;
    u8 *output = AllocOutput();
    LzStream decoded;
    s32 i;

    stream[0] = HEADER_WORD;
    stream[1] = HEADER_WORD;
    stream[2] = 0x0000;
    for (i = 0; i < 16; i++) {
        stream[3 + i] = (u16)(0x5000 + i);
    }
    EXPECT(DecodeFirstChunk(&decoded, stream, ARRAY_COUNT(stream), nextInput, output) == -1);
    EXPECT(decoded.output == (u16 *)output + 16);
    EXPECT(decoded.input == nextInput);

    EXPECT(DecodeFirstChunk(&decoded, stream, ARRAY_COUNT(stream) - 1, nextInput, output) == -1);
    EXPECT(decoded.output == (u16 *)output);
    EXPECT(decoded.input == nextInput - 16);
    free(output);
    free(nextBuffer);
}

int main(void) {
    TestCopiesLiterals();
    TestCopiesOverlappingReferences();
    TestRepeatsTheLastWord();
    TestCopiesLiteralRuns();
    TestReadsTheNextControlWord();
    TestCopiesTheFarthestLongestReference();
    TestCarriesInputIntoTheNextChunk();
    TestCarriesOnceARunMightNotFit();
    return ReportTestResults("lz_decoder_test");
}
