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

// The game reads what it unpacks as ints, floats and pointers, so the
// decoder has to copy the words without giving them a type of their own.
// Under the type sanitizer, a word stored as a u16 gives its bytes the type
// short, and reading them here as a u32 halts the test; the other builds
// only check the words. The stream has a literal, a run and a reference.
static void TestLeavesTheOutputUntyped(void) {
    u16 stream[2 + 1 + 16 + 1 + 1 + 1 + 1];
    u8 *output = AllocOutput();
    const u32 *outputWords = (const u32 *)output;
    u8 *expectedBytes;
    u16 expected[16 + 1 + 3];
    u32 expectedWord;
    s32 i;

    stream[0] = HEADER_WORD;
    stream[1] = HEADER_WORD;
    stream[2] = 0x0000;
    for (i = 0; i < 16; i++) {
        expected[i] = (u16)(0x3000 + i);
        stream[3 + i] = expected[i];
    }
    stream[19] = 0x6000;
    stream[20] = 0x4444;
    expected[16] = 0x4444;
    stream[21] = Reference(1, 3);
    expected[17] = expected[18] = expected[19] = 0x4444;
    stream[22] = END_REFERENCE;
    EXPECT(DecodeWholeStream(stream, ARRAY_COUNT(stream), output) == sizeof(expected));
    expectedBytes = PackWords(expected, ARRAY_COUNT(expected));
    for (i = 0; i < (s32)sizeof(expected) / (s32)sizeof(u32); i++) {
        memcpy(&expectedWord, expectedBytes + i * sizeof(u32), sizeof(u32));
        EXPECT(outputWords[i] == expectedWord);
    }
    free(expectedBytes);
    free(output);
}

// --- The patching decoder ---

// The output that the patches apply to, as LoadLzData leaves the unpatched
// data, with each word its own index above PATCHED_DATA_BASE
#define PATCHED_DATA_BASE 0xE000
#define PATCHED_DATA_WORD_COUNT 256
#define WINDOW_WORD_COUNT (LZ_WINDOW_MASK + 1)
#define HISTORY_WORD_COUNT (LZ_HISTORY_MASK + 1)

// Literal patches, as a command's kind and count less one
#define PatchLiterals(count) (LZ_PATCH_KIND_LITERALS | ((count) - 1))
// Copies from offset words later in the output, or before it in the
// history of the words patched over
#define PatchCopy(offset, count) ((((offset) & 0xFF) << 8) | ((count) - 1))

typedef struct PatchBuffers {
    u8 *output;
    u8 *window;
    u8 *history;
} PatchBuffers;

static PatchBuffers AllocPatchBuffers(void) {
    PatchBuffers buffers;
    u16 data[PATCHED_DATA_WORD_COUNT];
    s32 i;

    for (i = 0; i < PATCHED_DATA_WORD_COUNT; i++) {
        data[i] = (u16)(PATCHED_DATA_BASE + i);
    }
    buffers.output = PackWords(data, PATCHED_DATA_WORD_COUNT);
    buffers.window = AllocUnwritten(WINDOW_WORD_COUNT);
    buffers.history = AllocUnwritten(HISTORY_WORD_COUNT);
    return buffers;
}

static void FreePatchBuffers(PatchBuffers *buffers) {
    free(buffers->history);
    free(buffers->window);
    free(buffers->output);
}

static void StartPatchedStream(
    LzStream *stream,
    const PatchBuffers *buffers,
    u8 *chunk,
    s32 count,
    u16 *nextInput
) {
    stream->input = (u16 *)chunk + CARRY_WORD_COUNT;
    stream->inputEnd = stream->input + count;
    stream->nextInput = nextInput;
    stream->outputStart = (u16 *)buffers->output;
    stream->window = (u16 *)buffers->window;
    stream->history = (u16 *)buffers->history;
    stream->flags = LZ_STREAM_FLAG_FIRST | (nextInput == NULL ? LZ_STREAM_FLAG_LAST : 0);
}

static s32 PatchWholeStream(const u16 *words, s32 count, const PatchBuffers *buffers) {
    u8 *chunk = PackChunk(words, count);
    LzStream stream;
    s32 size;

    StartPatchedStream(&stream, buffers, chunk, count, NULL);
    size = DecodePatchedLzChunk(&stream);
    free(chunk);
    return size;
}

// Whether the output starts with the expected words, and the data after
// them is unpatched
static Boolean HasPatchedOutput(const u8 *output, const u16 *expected, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (ReadWord(output, i) != expected[i]) {
            return FALSE;
        }
    }
    return ReadWord(output, count) == PATCHED_DATA_BASE + count;
}

// A literal patch writes the words after it over the data, and the size is
// up to the last word patched
static void TestPatchesLiterals(void) {
    static const u16 stream[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x1000,
        PatchLiterals(2),
        0x1111,
        0x2222,
        END_REFERENCE,
    };
    static const u16 expected[] = {0x1111, 0x2222};
    PatchBuffers buffers = AllocPatchBuffers();

    EXPECT(PatchWholeStream(stream, ARRAY_COUNT(stream), &buffers) == sizeof(expected));
    EXPECT(HasPatchedOutput(buffers.output, expected, ARRAY_COUNT(expected)));
    FreePatchBuffers(&buffers);
}

// A copy from the word itself skips it, and one from later words moves them
// back
static void TestCopiesFromLaterWords(void) {
    static const u16 stream[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x2000,
        PatchCopy(0, 1),
        PatchCopy(2, 2),
        END_REFERENCE,
    };
    static const u16 expected[] = {0xE000, 0xE003, 0xE004};
    PatchBuffers buffers = AllocPatchBuffers();

    EXPECT(PatchWholeStream(stream, ARRAY_COUNT(stream), &buffers) == sizeof(expected));
    EXPECT(HasPatchedOutput(buffers.output, expected, ARRAY_COUNT(expected)));
    FreePatchBuffers(&buffers);
}

// A negative offset copies from the words that patches have overwritten,
// counting back from the last
static void TestCopiesOverwrittenWords(void) {
    static const u16 stream[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x0800,
        PatchLiterals(2),
        0x1111,
        0x2222,
        PatchCopy(-2, 2),
        END_REFERENCE,
    };
    static const u16 expected[] = {0x1111, 0x2222, 0xE000, 0xE001};
    PatchBuffers buffers = AllocPatchBuffers();

    EXPECT(PatchWholeStream(stream, ARRAY_COUNT(stream), &buffers) == sizeof(expected));
    EXPECT(HasPatchedOutput(buffers.output, expected, ARRAY_COUNT(expected)));
    FreePatchBuffers(&buffers);
}

// Only 0x80 in the high byte makes literals: 0x7F copies from 127 words on,
// and 0x81 from 127 words back in the history, where the history's
// unwritten words still are
static void TestTellsLiteralsFromFarCopies(void) {
    static const u16 stream[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x2000,
        PatchCopy(127, 1),
        PatchCopy(-127, 1),
        END_REFERENCE,
    };
    static const u16 expected[] = {0xE000 + 127, UNWRITTEN_WORD};
    PatchBuffers buffers = AllocPatchBuffers();

    EXPECT(PatchWholeStream(stream, ARRAY_COUNT(stream), &buffers) == sizeof(expected));
    EXPECT(HasPatchedOutput(buffers.output, expected, ARRAY_COUNT(expected)));
    FreePatchBuffers(&buffers);
}

// References repeat decoded words, here a skip and then a literal patch's
// command, as commands in turn
static void TestRepeatsDecodedCommands(void) {
    static const u16 stream[] = {
        HEADER_WORD,
        HEADER_WORD,
        0x4800,
        PatchCopy(0, 1),
        Reference(1, 3),
        PatchLiterals(1),
        0x5555,
        END_REFERENCE,
    };
    static const u16 expected[] = {0xE000, 0xE001, 0xE002, 0xE003, 0x5555};
    PatchBuffers buffers = AllocPatchBuffers();

    EXPECT(PatchWholeStream(stream, ARRAY_COUNT(stream), &buffers) == sizeof(expected));
    EXPECT(HasPatchedOutput(buffers.output, expected, ARRAY_COUNT(expected)));
    FreePatchBuffers(&buffers);
}

// A chunk ends inside a literal patch, which the next goes on with; there a
// copy from the history and a reference into the window find what the
// first chunk left in them. The first chunk's last 16 words, its second
// control word and 15 of that word's items, are carried and decoded with
// the next. The first two literals are a skip and a history copy, as data,
// which the reference then repeats as commands from 31 words back.
static void TestCarriesPatchesIntoTheNextChunk(void) {
    u16 firstChunk[2 + 1 + 16 + 1 + 15];
    static const u16 secondChunk[] = {PatchCopy(-11, 1), 0xC000, Reference(31, 2), END_REFERENCE};
    u16 expected[33];
    u8 *firstBuffer;
    u8 *nextBuffer = AllocUnwritten(CARRY_WORD_COUNT + ARRAY_COUNT(secondChunk));
    u16 *nextInput = (u16 *)nextBuffer + CARRY_WORD_COUNT;
    u8 *packed = PackWords(secondChunk, ARRAY_COUNT(secondChunk));
    PatchBuffers buffers = AllocPatchBuffers();
    LzStream stream;
    s32 i;

    memcpy(nextInput, packed, sizeof(secondChunk));
    free(packed);
    firstChunk[0] = HEADER_WORD;
    firstChunk[1] = HEADER_WORD;
    firstChunk[2] = 0x0000;
    firstChunk[3] = PatchLiterals(20);
    for (i = 0; i < 20; i++) {
        expected[i] = (u16)(0x1001 + i);
    }
    expected[0] = PatchCopy(0, 1);
    expected[1] = PatchCopy(-11, 1);
    memcpy(&firstChunk[4], &expected[0], 15 * sizeof(u16));
    firstChunk[19] = 0x0000;
    memcpy(&firstChunk[20], &expected[15], 5 * sizeof(u16));
    for (i = 25; i < 35; i++) {
        firstChunk[i] = PatchCopy(0, 1);
    }

    firstBuffer = PackChunk(firstChunk, ARRAY_COUNT(firstChunk));
    StartPatchedStream(&stream, &buffers, firstBuffer, ARRAY_COUNT(firstChunk), nextInput);
    EXPECT(DecodePatchedLzChunk(&stream) == -1);
    EXPECT(stream.input == nextInput - 16);
    EXPECT(stream.output == (u16 *)buffers.output + 15);
    EXPECT(stream.patchCount == 5);
    EXPECT(stream.windowIndex == 16);
    EXPECT(stream.historyIndex == 15);
    EXPECT(ReadWord((const u8 *)(nextInput - 16), 0) == 0x0000);

    // The skips keep words 20 to 29, which go into the history after the
    // 20 that the literals overwrote; 11 back from there is word 19's
    // old value. The reference's skip keeps word 31, and its history copy
    // then finds word 21's.
    for (i = 20; i < 30; i++) {
        expected[i] = (u16)(PATCHED_DATA_BASE + i);
    }
    expected[30] = 0xE013;
    expected[31] = 0xE01F;
    expected[32] = 0xE015;
    stream.inputEnd = nextInput + ARRAY_COUNT(secondChunk);
    stream.nextInput = (u16 *)firstBuffer + CARRY_WORD_COUNT;
    stream.flags = LZ_STREAM_FLAG_LAST;
    EXPECT(DecodePatchedLzChunk(&stream) == sizeof(expected));
    EXPECT(HasPatchedOutput(buffers.output, expected, ARRAY_COUNT(expected)));
    FreePatchBuffers(&buffers);
    free(firstBuffer);
    free(nextBuffer);
}

// As DecodeLzChunk, a chunk carries its words over once fewer than 17 are
// left after a control word's items: with 17 left it decodes a literal
// patch's command and 15 words, and with 16 it carries them all
static void TestCarriesPatchesOnceARunMightNotFit(void) {
    u16 stream[2 + 1 + 16];
    u8 *chunk;
    u8 *nextBuffer = AllocUnwritten(CARRY_WORD_COUNT + 1);
    u16 *nextInput = (u16 *)nextBuffer + CARRY_WORD_COUNT;
    PatchBuffers buffers = AllocPatchBuffers();
    LzStream decoded;
    s32 count;
    s32 i;

    stream[0] = HEADER_WORD;
    stream[1] = HEADER_WORD;
    stream[2] = 0x0000;
    stream[3] = PatchLiterals(15);
    for (i = 0; i < 15; i++) {
        stream[4 + i] = (u16)(0x5000 + i);
    }

    for (count = ARRAY_COUNT(stream); count >= ARRAY_COUNT(stream) - 1; count--) {
        chunk = PackChunk(stream, count);
        StartPatchedStream(&decoded, &buffers, chunk, count, nextInput);
        EXPECT(DecodePatchedLzChunk(&decoded) == -1);
        if (count == ARRAY_COUNT(stream)) {
            EXPECT(decoded.output == (u16 *)buffers.output + 15);
            EXPECT(decoded.input == nextInput);
        }
        else {
            EXPECT(decoded.output == (u16 *)buffers.output);
            EXPECT(decoded.input == nextInput - 16);
        }
        free(chunk);
    }

    FreePatchBuffers(&buffers);
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
    TestLeavesTheOutputUntyped();
    TestPatchesLiterals();
    TestCopiesFromLaterWords();
    TestCopiesOverwrittenWords();
    TestTellsLiteralsFromFarCopies();
    TestRepeatsDecodedCommands();
    TestCarriesPatchesIntoTheNextChunk();
    TestCarriesPatchesOnceARunMightNotFit();
    return ReportTestResults("lz_decoder_test");
}
