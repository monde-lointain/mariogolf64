/*
 * Tests the game's encoded assets, src/system/asset.c, against a fake
 * cartridge holding a small archive: what each decoder writes, and that the
 * renderer will read exactly those bytes in the ROM's order.
 */

#include <SDL3/SDL.h>
#include <nusys.h>
#include <ultra64.h>

#include "debug/game_flags.h"
#include "system/asset.h"
#include "system/memory_address.h"
#include "system/rom_files.h"
#include "test_support.h"

#define ARCHIVE_SIZE 0x200
#define TABLE_ENTRY_SIZE 8
#define FIRST_FILE_OFFSET 0x40
#define HALFWORD_RUNS 2
// Room around the decoded bytes, to see that the marks stop at their ends
#define GUARD_SIZE 16

static u8 f_Archive[ARCHIVE_SIZE];
static u32 f_NextFileOffset = FIRST_FILE_OFFSET;
static u8 f_Decoded[GUARD_SIZE + 64 + GUARD_SIZE] __attribute__((aligned(16)));

// --- Doubles for asset.c's collaborators ---

// The cartridge: reads the archive, marking what it reads as the ROM's
// order, as the real read does
void nuPiReadRom(u32 romAddress, void *buffer, u32 size) {
    u32 offset = romAddress - (u32)AddressNumber(g_AssetArchiveRomStart);

    SDL_assert(offset + size <= ARCHIVE_SIZE);
    SDL_memcpy(buffer, &f_Archive[offset], size);
    osPcMarkRomOrder(buffer, size);
}

void nuGfxTaskAllEndWait(void) {
}

Boolean IsGameFlagSet(u32 flag) {
    (void)flag;
    return FALSE;
}

// --- The archive ---

static void PutWord(u32 offset, u32 value) {
    f_Archive[offset] = (u8)(value >> 24);
    f_Archive[offset + 1] = (u8)(value >> 16);
    f_Archive[offset + 2] = (u8)(value >> 8);
    f_Archive[offset + 3] = (u8)value;
}

// Adds an encoded file to the archive as the asset: its header word, then
// its runs
static void AddEncodedAsset(AssetId id, u8 method, u32 decodedSize, const u8 *runs, u32 size) {
    u32 offset = f_NextFileOffset;

    SDL_assert(offset + 4 + size <= ARCHIVE_SIZE);
    PutWord((u32)id * TABLE_ENTRY_SIZE, 4 + size);
    PutWord(((u32)id * TABLE_ENTRY_SIZE) + 4, offset);
    PutWord(offset, ((u32)method << 24) | decodedSize);
    SDL_memcpy(&f_Archive[offset + 4], runs, size);
    f_NextFileOffset = (offset + 4 + size + 1) & ~1u;
}

// Decodes the asset after the guard bytes, which start in the host's order
static u8 *Decode(AssetId id, s32 *decodedSize) {
    EncodedAsset asset;

    SDL_memset(f_Decoded, 0xEE, sizeof(f_Decoded));
    osPcUnmarkRomOrder(f_Decoded, sizeof(f_Decoded));
    *decodedSize = OpenEncodedAsset(id, &asset);
    ReadEncodedAsset(&f_Decoded[GUARD_SIZE], &asset);
    return &f_Decoded[GUARD_SIZE];
}

// Whether the decoded bytes, and no others, are marked as the ROM's order
static bool IsOnlyDecodedRomOrder(s32 decodedSize) {
    s32 i;

    for (i = 0; i < (s32)sizeof(f_Decoded); i++) {
        bool isDecoded = i >= GUARD_SIZE && i < GUARD_SIZE + decodedSize;

        if ((osPcIsRomOrder(&f_Decoded[i]) != 0) != isDecoded) {
            return false;
        }
    }

    return true;
}

// Byte runs: a repeat, a copy, and an odd decoded size
static void TestDecodesByteRuns(void) {
    static const u8 runs[] = {3, 0xAB, (u8)-2, 0x12, 0x34, 0};
    static const u8 expected[] = {0xAB, 0xAB, 0xAB, 0x12, 0x34};
    s32 size;
    u8 *decoded;

    AddEncodedAsset(1, ENCODING_BYTE_RUNS, sizeof(expected), runs, sizeof(runs));
    decoded = Decode(1, &size);
    EXPECT(size == (s32)sizeof(expected));
    EXPECT(SDL_memcmp(decoded, expected, sizeof(expected)) == 0);
    EXPECT(decoded[size] == 0xEE);
    EXPECT(IsOnlyDecodedRomOrder(size));
}

// Halfword runs: each unit keeps its bytes in the ROM's order
static void TestDecodesHalfwordRuns(void) {
    static const u8 runs[] = {2, 0xF8, 0x01, (u8)-1, 0x07, 0xC1, 0};
    static const u8 expected[] = {0xF8, 0x01, 0xF8, 0x01, 0x07, 0xC1};
    s32 size;
    u8 *decoded;

    AddEncodedAsset(2, HALFWORD_RUNS, sizeof(expected), runs, sizeof(runs));
    decoded = Decode(2, &size);
    EXPECT(size == (s32)sizeof(expected));
    EXPECT(SDL_memcmp(decoded, expected, sizeof(expected)) == 0);
    EXPECT(IsOnlyDecodedRomOrder(size));
}

// The least asset decodes to nothing, and marks nothing
static void TestDecodesEmptyAsset(void) {
    static const u8 runs[] = {0};
    s32 size;

    AddEncodedAsset(3, ENCODING_BYTE_RUNS, 0, runs, sizeof(runs));
    Decode(3, &size);
    EXPECT(size == 0);
    EXPECT(f_Decoded[GUARD_SIZE] == 0xEE);
    EXPECT(IsOnlyDecodedRomOrder(0));
}

int main(void) {
    InitAssetFiles();
    TestDecodesByteRuns();
    TestDecodesHalfwordRuns();
    TestDecodesEmptyAsset();
    return ReportTestResults("asset_test");
}
