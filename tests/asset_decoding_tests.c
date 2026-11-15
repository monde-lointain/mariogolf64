/*
 * Characterizes how asset.c decodes the asset archive's encoded assets.
 * ReadEncodedAsset decodes runs that each start with a signed count byte:
 * a positive count repeats the unit that follows that many times, a
 * negative one copies that many units, and zero ends the asset. The unit is
 * a byte for ENCODING_BYTE_RUNS and a halfword for any other method. The
 * encoded bytes are read from ROM in chunks of CHUNK_SIZE, and a run may
 * cross from one chunk to the next anywhere.
 *
 * The streams below are read from ROM where the test ROM keeps them, in the
 * tests' segment. Those that cross a chunk are mostly filler: runs of one
 * unit of FILLER_BYTE each, which decode to FILLER_BYTE.
 */

#include <string.h>
#include <ultra64.h>

#include "hole_fixture.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/memory_address.h"
#include "system/segment_symbols.h"
#include "unity_fixture.h"

DECLARE_SEGMENT(target_tests);

// Any method but ENCODING_BYTE_RUNS decodes halfword runs; the archive's
// halfword assets give this one
#define ENCODING_HALFWORD_RUNS 2

// The encoded bytes that the decoder reads at a time
#define CHUNK_SIZE 0x2000

// The bytes after the decoded ones, which the decoder must leave alone
#define GUARD_BYTE 0xEE
// Room for the largest asset decoded below, and the guard bytes after it
#define DECODED_BUFFER_SIZE 0xC000

// A run of one byte of FILLER_BYTE is two bytes of it, and a run of one
// halfword of it three
#define FILLER_BYTE 0x01
#define BYTE_FILLER_RUN_SIZE 2
#define HALFWORD_FILLER_RUN_SIZE 3

// The longest runs that a count byte gives
#define LONGEST_REPEAT_COUNT 127
#define LONGEST_COPY_COUNT 128

// Sixteen bytes counting up from start, for streams that copy many bytes
#define BYTES_COUNTING_UP_16(start) \
    (start), (start) + 1, (start) + 2, (start) + 3, (start) + 4, (start) + 5, (start) + 6, \
        (start) + 7, (start) + 8, (start) + 9, (start) + 10, (start) + 11, (start) + 12, \
        (start) + 13, (start) + 14, (start) + 15
#define BYTES_COUNTING_UP_128(start) \
    BYTES_COUNTING_UP_16(start), BYTES_COUNTING_UP_16((start) + 16), \
        BYTES_COUNTING_UP_16((start) + 32), BYTES_COUNTING_UP_16((start) + 48), \
        BYTES_COUNTING_UP_16((start) + 64), BYTES_COUNTING_UP_16((start) + 80), \
        BYTES_COUNTING_UP_16((start) + 96), BYTES_COUNTING_UP_16((start) + 112)

// The archive's assets that the characterization decodes, their sizes once
// decoded, and the FNV-1a hashes of what the matching build decodes
#define COURSE_SCENE_SIZE 0x4780
#define COURSE_SCENE_HASH 0xF35396F2
#define WIDE_BOARD_SIZE 0xA208
#define WIDE_BOARD_HASH 0xD624C21E
#define DRIVER_HEAD_IMAGE_SIZE 0x808
#define DRIVER_HEAD_IMAGE_HASH 0xDD5094F2

// Streams are read from ROM by DMA, which needs them on even addresses
#define ROM_STREAM static const u8 __attribute__((aligned(8)))

static u8 f_Decoded[DECODED_BUFFER_SIZE] __attribute__((aligned(16)));

TEST_GROUP(AssetDecoding);

TEST_SETUP(AssetDecoding) {
    memset(f_Decoded, GUARD_BYTE, sizeof(f_Decoded));
}

TEST_TEAR_DOWN(AssetDecoding) {
}

// Where the test ROM keeps data of the tests' segment
static u32 TestRomAddress(const void *data) {
    return (u32)target_tests_ROM_START + BytesBetween(target_tests_VRAM, data);
}

// Decodes the runs of the stream, which has no header word, into f_Decoded,
// as OpenEncodedAsset leaves an asset once it has read that word
static void DecodeStream(s32 method, const u8 *stream, u32 streamSize) {
    EncodedAsset asset;
    u32 romAddress = TestRomAddress(stream);

    asset.method = method;
    asset.encodedSize = sizeof(u32) + streamSize;
    asset.file.romStart = romAddress;
    asset.file.cursor = romAddress;
    asset.file.romEnd = romAddress + streamSize;
    asset.file.size = streamSize;
    ReadEncodedAsset(f_Decoded, &asset);
}

// Asserts that f_Decoded starts with the expected bytes, and that the
// decoder wrote nothing after them
static void AssertDecoded(const u8 *expected, u32 size) {
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, f_Decoded, size);
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[size]);
}

// Asserts that f_Decoded holds count bytes of FILLER_BYTE from offset
static void AssertFiller(u32 offset, u32 count) {
    TEST_ASSERT_EACH_EQUAL_HEX8(FILLER_BYTE, &f_Decoded[offset], count);
}

// Decodes an asset of the archive and asserts its decoded size, the hash of
// what was decoded and that nothing was written after it
static void AssertArchiveAsset(AssetId assetId, s32 method, u32 size, u32 hash) {
    EncodedAsset asset;

    TEST_ASSERT_EQUAL_HEX32(size, OpenEncodedAsset(assetId, &asset));
    TEST_ASSERT_EQUAL_INT32(method, asset.method);
    ReadEncodedAsset(f_Decoded, &asset);
    TEST_ASSERT_EQUAL_HEX32(hash, HashBytes(FNV_OFFSET_BASIS, f_Decoded, size));
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[size]);
}

TEST(AssetDecoding, ZeroCountEndsBeforeAnyByte) {
    ROM_STREAM stream[] = {0x00, 0x05, 0xAA};

    DecodeStream(ENCODING_BYTE_RUNS, stream, sizeof(stream));
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[0]);
}

TEST(AssetDecoding, PositiveCountRepeatsTheNextByte) {
    ROM_STREAM stream[] = {0x03, 0xAB, 0x00};
    static const u8 expected[] = {0xAB, 0xAB, 0xAB};

    DecodeStream(ENCODING_BYTE_RUNS, stream, sizeof(stream));
    AssertDecoded(expected, sizeof(expected));
}

TEST(AssetDecoding, NegativeCountCopiesTheNextBytes) {
    ROM_STREAM stream[] = {0xFD, 0x11, 0x22, 0x33, 0x00};
    static const u8 expected[] = {0x11, 0x22, 0x33};

    DecodeStream(ENCODING_BYTE_RUNS, stream, sizeof(stream));
    AssertDecoded(expected, sizeof(expected));
}

TEST(AssetDecoding, RunsOfEachKindFollowOneAnother) {
    ROM_STREAM stream[] = {0x02, 0x11, 0xFE, 0x22, 0x33, 0x01, 0x44, 0xFF, 0x55, 0x00};
    static const u8 expected[] = {0x11, 0x11, 0x22, 0x33, 0x44, 0x55};

    DecodeStream(ENCODING_BYTE_RUNS, stream, sizeof(stream));
    AssertDecoded(expected, sizeof(expected));
}

TEST(AssetDecoding, LongestByteRuns) {
    ROM_STREAM stream[] = {
        LONGEST_REPEAT_COUNT,
        0x5A,
        0x80,
        BYTES_COUNTING_UP_128(0x80),
        0x00,
    };
    static const u8 copied[] = {BYTES_COUNTING_UP_128(0x80)};

    DecodeStream(ENCODING_BYTE_RUNS, stream, sizeof(stream));
    TEST_ASSERT_EACH_EQUAL_HEX8(0x5A, f_Decoded, LONGEST_REPEAT_COUNT);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(copied, &f_Decoded[LONGEST_REPEAT_COUNT], LONGEST_COPY_COUNT);
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[LONGEST_REPEAT_COUNT + LONGEST_COPY_COUNT]);
}

// A copy of two bytes puts the filler's runs, and so the repeat's count, on
// odd offsets: the count is the first chunk's last byte
TEST(AssetDecoding, RepeatedByteIsTheNextChunksFirst) {
    ROM_STREAM stream[CHUNK_SIZE + 2] = {
        [0] = 0xFE,
        [1] = 0x11,
        [2] = 0x22,
        [3 ... CHUNK_SIZE - 2] = FILLER_BYTE,
        [CHUNK_SIZE - 1] = 0x05,
        [CHUNK_SIZE] = 0x33,
        [CHUNK_SIZE + 1] = 0x00,
    };
    u32 fillerCount = (CHUNK_SIZE - 4) / BYTE_FILLER_RUN_SIZE;

    DecodeStream(ENCODING_BYTE_RUNS, stream, sizeof(stream));
    TEST_ASSERT_EQUAL_HEX8(0x11, f_Decoded[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22, f_Decoded[1]);
    AssertFiller(2, fillerCount);
    TEST_ASSERT_EACH_EQUAL_HEX8(0x33, &f_Decoded[2 + fillerCount], 5);
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[2 + fillerCount + 5]);
}

TEST(AssetDecoding, CopiedBytesCrossIntoTheNextChunk) {
    ROM_STREAM stream[CHUNK_SIZE + 2] = {
        [0 ... CHUNK_SIZE - 5] = FILLER_BYTE,
        [CHUNK_SIZE - 4] = 0xFC,
        [CHUNK_SIZE - 3] = 0x44,
        [CHUNK_SIZE - 2] = 0x55,
        [CHUNK_SIZE - 1] = 0x66,
        [CHUNK_SIZE] = 0x77,
        [CHUNK_SIZE + 1] = 0x00,
    };
    static const u8 copied[] = {0x44, 0x55, 0x66, 0x77};
    u32 fillerCount = (CHUNK_SIZE - 4) / BYTE_FILLER_RUN_SIZE;

    DecodeStream(ENCODING_BYTE_RUNS, stream, sizeof(stream));
    AssertFiller(0, fillerCount);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(copied, &f_Decoded[fillerCount], sizeof(copied));
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[fillerCount + sizeof(copied)]);
}

TEST(AssetDecoding, PositiveCountRepeatsTheNextHalfword) {
    ROM_STREAM stream[] = {0x03, 0xAB, 0xCD, 0x00};
    static const u8 expected[] = {0xAB, 0xCD, 0xAB, 0xCD, 0xAB, 0xCD};

    DecodeStream(ENCODING_HALFWORD_RUNS, stream, sizeof(stream));
    AssertDecoded(expected, sizeof(expected));
}

TEST(AssetDecoding, NegativeCountCopiesTheNextHalfwords) {
    ROM_STREAM stream[] = {0xFE, 0x11, 0x22, 0x33, 0x44, 0x00};
    static const u8 expected[] = {0x11, 0x22, 0x33, 0x44};

    DecodeStream(ENCODING_HALFWORD_RUNS, stream, sizeof(stream));
    AssertDecoded(expected, sizeof(expected));
}

// The archive has only methods 1 and 2, but the decoder takes any but 1 for
// halfword runs
TEST(AssetDecoding, AnyOtherMethodDecodesHalfwords) {
    ROM_STREAM stream[] = {0x02, 0x12, 0x34, 0x00};
    static const u8 expected[] = {0x12, 0x34, 0x12, 0x34};

    DecodeStream(0, stream, sizeof(stream));
    AssertDecoded(expected, sizeof(expected));
}

TEST(AssetDecoding, HalfwordRunsOfEachKindFollowOneAnother) {
    ROM_STREAM stream[] = {0x02, 0x11, 0x22, 0xFF, 0x33, 0x44, 0x01, 0x55, 0x66, 0x00};
    static const u8 expected[] = {0x11, 0x22, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    DecodeStream(ENCODING_HALFWORD_RUNS, stream, sizeof(stream));
    AssertDecoded(expected, sizeof(expected));
}

TEST(AssetDecoding, LongestHalfwordRuns) {
    ROM_STREAM stream[] = {
        LONGEST_REPEAT_COUNT,
        0x5A,
        0xA5,
        0x80,
        BYTES_COUNTING_UP_128(0x00),
        BYTES_COUNTING_UP_128(0x80),
        0x00,
    };
    static const u8 copied[] = {BYTES_COUNTING_UP_128(0x00), BYTES_COUNTING_UP_128(0x80)};
    u32 repeatedSize = LONGEST_REPEAT_COUNT * sizeof(u16);
    s32 i;

    DecodeStream(ENCODING_HALFWORD_RUNS, stream, sizeof(stream));
    for (i = 0; i < LONGEST_REPEAT_COUNT; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x5A, f_Decoded[i * sizeof(u16)]);
        TEST_ASSERT_EQUAL_HEX8(0xA5, f_Decoded[(i * sizeof(u16)) + 1]);
    }
    TEST_ASSERT_EQUAL_HEX8_ARRAY(copied, &f_Decoded[repeatedSize], sizeof(copied));
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[repeatedSize + sizeof(copied)]);
}

// The filler's runs end just before the first chunk's last two bytes,
// which are the repeat's count and high byte
TEST(AssetDecoding, RepeatedHalfwordsLowByteIsTheNextChunksFirst) {
    ROM_STREAM stream[CHUNK_SIZE + 2] = {
        [0 ... CHUNK_SIZE - 3] = FILLER_BYTE,
        [CHUNK_SIZE - 2] = 0x02,
        [CHUNK_SIZE - 1] = 0xAB,
        [CHUNK_SIZE] = 0xCD,
        [CHUNK_SIZE + 1] = 0x00,
    };
    static const u8 repeated[] = {0xAB, 0xCD, 0xAB, 0xCD};
    u32 fillerSize = ((CHUNK_SIZE - 2) / HALFWORD_FILLER_RUN_SIZE) * sizeof(u16);

    DecodeStream(ENCODING_HALFWORD_RUNS, stream, sizeof(stream));
    AssertFiller(0, fillerSize);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(repeated, &f_Decoded[fillerSize], sizeof(repeated));
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[fillerSize + sizeof(repeated)]);
}

// A copy of three halfwords first puts the filler's runs where the copy
// that crosses chunks has its second halfword's high byte as the first
// chunk's last
TEST(AssetDecoding, CopiedHalfwordCrossesIntoTheNextChunk) {
    ROM_STREAM stream[CHUNK_SIZE + 2] = {
        [0] = 0xFD,
        [1] = 0x0A,
        [2] = 0x0B,
        [3] = 0x0C,
        [4] = 0x0D,
        [5] = 0x0E,
        [6] = 0x0F,
        [7 ... CHUNK_SIZE - 5] = FILLER_BYTE,
        [CHUNK_SIZE - 4] = 0xFE,
        [CHUNK_SIZE - 3] = 0x11,
        [CHUNK_SIZE - 2] = 0x22,
        [CHUNK_SIZE - 1] = 0x33,
        [CHUNK_SIZE] = 0x44,
        [CHUNK_SIZE + 1] = 0x00,
    };
    static const u8 first[] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    static const u8 crossing[] = {0x11, 0x22, 0x33, 0x44};
    u32 fillerSize = ((CHUNK_SIZE - 11) / HALFWORD_FILLER_RUN_SIZE) * sizeof(u16);

    DecodeStream(ENCODING_HALFWORD_RUNS, stream, sizeof(stream));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(first, f_Decoded, sizeof(first));
    AssertFiller(sizeof(first), fillerSize);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(
        crossing,
        &f_Decoded[sizeof(first) + fillerSize],
        sizeof(crossing)
    );
    TEST_ASSERT_EQUAL_HEX8(GUARD_BYTE, f_Decoded[sizeof(first) + fillerSize + sizeof(crossing)]);
}

// Two chunks and a part of a third
TEST(AssetDecoding, CourseSceneDecodesAsTheMatchingBuildDoes) {
    AssertArchiveAsset(
        ASSET_COURSE_SCENE,
        ENCODING_BYTE_RUNS,
        COURSE_SCENE_SIZE,
        COURSE_SCENE_HASH
    );
}

// Three chunks and most of a fourth
TEST(AssetDecoding, WideBoardDecodesAsTheMatchingBuildDoes) {
    AssertArchiveAsset(ASSET_WIDE_BOARD, ENCODING_BYTE_RUNS, WIDE_BOARD_SIZE, WIDE_BOARD_HASH);
}

TEST(AssetDecoding, DriverHeadImageDecodesAsTheMatchingBuildDoes) {
    AssertArchiveAsset(
        ASSET_DRIVER_HEAD_IMAGE,
        ENCODING_HALFWORD_RUNS,
        DRIVER_HEAD_IMAGE_SIZE,
        DRIVER_HEAD_IMAGE_HASH
    );
}

TEST_GROUP_RUNNER(AssetDecoding) {
    RUN_TEST_CASE(AssetDecoding, ZeroCountEndsBeforeAnyByte);
    RUN_TEST_CASE(AssetDecoding, PositiveCountRepeatsTheNextByte);
    RUN_TEST_CASE(AssetDecoding, NegativeCountCopiesTheNextBytes);
    RUN_TEST_CASE(AssetDecoding, RunsOfEachKindFollowOneAnother);
    RUN_TEST_CASE(AssetDecoding, LongestByteRuns);
    RUN_TEST_CASE(AssetDecoding, RepeatedByteIsTheNextChunksFirst);
    RUN_TEST_CASE(AssetDecoding, CopiedBytesCrossIntoTheNextChunk);
    RUN_TEST_CASE(AssetDecoding, PositiveCountRepeatsTheNextHalfword);
    RUN_TEST_CASE(AssetDecoding, NegativeCountCopiesTheNextHalfwords);
    RUN_TEST_CASE(AssetDecoding, AnyOtherMethodDecodesHalfwords);
    RUN_TEST_CASE(AssetDecoding, HalfwordRunsOfEachKindFollowOneAnother);
    RUN_TEST_CASE(AssetDecoding, LongestHalfwordRuns);
    RUN_TEST_CASE(AssetDecoding, RepeatedHalfwordsLowByteIsTheNextChunksFirst);
    RUN_TEST_CASE(AssetDecoding, CopiedHalfwordCrossesIntoTheNextChunk);
    RUN_TEST_CASE(AssetDecoding, CourseSceneDecodesAsTheMatchingBuildDoes);
    RUN_TEST_CASE(AssetDecoding, WideBoardDecodesAsTheMatchingBuildDoes);
    RUN_TEST_CASE(AssetDecoding, DriverHeadImageDecodesAsTheMatchingBuildDoes);
}
