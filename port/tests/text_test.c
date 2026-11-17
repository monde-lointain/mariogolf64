/*
 * Tests of the message file as the CPU reads it on a host. The file is
 * big-endian halfwords on the cartridge: its size, the message count, each
 * message's start and length, and then the character codes. A fake
 * cartridge holds a small file of that shape, and MeasureMessage shows what
 * the game made of it.
 */

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "round/game_state.h"
#include "round/round_hud.h"
#include "screen/screen_layers.h"
#include "shot/current_shot.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "system/rom_files.h"
#include "test_support.h"
#include "text/text.h"
#include "text/text_format.h"

// The space a line break adds below a line, past the font's height
#define LINE_SPACING 6
#define GLYPH_FILE_SIZE 10
#define MESSAGE_COUNT 3
#define MESSAGE_FILE_HALFWORDS 31

GameState g_GameState;
RoundHud g_RoundHud;
CURRENT_SHOT_VOLATILE CurrentShot g_CurrentShot;

// A glyph file of one glyph of two bytes
static const u8 f_GlyphFile[GLYPH_FILE_SIZE] = {0, 0, 0, 1, 0, 2, 0, 0, 0xAB, 0xCD};
static u8 f_MessageFile[MESSAGE_FILE_HALFWORDS * 2];

// --- Doubles for text.c's collaborators ---

// The cartridge: reads the glyph file or the message file, marking what it
// reads as the ROM's order, as the real read does
void nuPiReadRom(u32 romAddress, void *buffer, u32 size) {
    const u8 *file = f_MessageFile;
    u32 fileSize = sizeof(f_MessageFile);

    if (romAddress == (u32)AddressNumber(g_GlyphFileRomStart)) {
        file = f_GlyphFile;
        fileSize = sizeof(f_GlyphFile);
    }
    else {
        EXPECT(romAddress == (u32)AddressNumber(g_MessageFileRomStart));
    }

    EXPECT(size <= fileSize);
    memcpy(buffer, file, size < fileSize ? size : fileSize);
    osPcMarkRomOrder(buffer, size);
}

void *AllocMemory(u32 size) {
    return calloc(1, size);
}

void FreeMemory(void *pointerToBlock) {
    void **block = pointerToBlock;

    free(*block);
    *block = NULL;
}

s32 OpenEncodedAsset(AssetId assetId, EncodedAsset *asset) {
    (void)assetId;
    (void)asset;
    return 16;
}

void ReadEncodedAsset(void *buffer, EncodedAsset *asset) {
    (void)buffer;
    (void)asset;
}

void PlaySound(SoundId soundId, SoundChannel channel, SoundVolume volume) {
    (void)soundId;
    (void)channel;
    (void)volume;
}

void DrawTextureRectangle(
    Gfx **displayListHead,
    s32 xl,
    s32 yl,
    s32 xh,
    s32 yh,
    s32 tile,
    s32 s,
    s32 t,
    s32 dsdx,
    s32 dtdy
) {
}

void guTranslate(Mtx *m, float x, float y, float z) {
    (void)m;
    (void)x;
    (void)y;
    (void)z;
}

// --- Scaffolding ---

static void StoreHalfword(int index, u16 value) {
    SDL_assert(index >= 0 && index < MESSAGE_FILE_HALFWORDS);
    f_MessageFile[index * 2] = (u8)(value >> 8);
    f_MessageFile[(index * 2) + 1] = (u8)value;
}

// Stores the codes of message index as the text's characters, with each new
// line a line break, after the codes of the messages before it
static void StoreMessage(int index, const char *text) {
    static int codeCount;
    int length = (int)strlen(text);
    int i;

    if (index == 0) {
        codeCount = 0;
    }

    StoreHalfword(2 + (index * 2), (u16)codeCount);
    StoreHalfword(3 + (index * 2), (u16)length);
    for (i = 0; i < length; i++) {
        StoreHalfword(
            2 + (MESSAGE_COUNT * 2) + codeCount + i,
            (text[i] == '\n') ? MESSAGE_CODE_LINE_BREAK : (u16)text[i]
        );
    }

    codeCount += length;
}

// Three messages: "AB", a line break and "C"; "DEF" on one line; and five
// lines, the last two the widest. Each code below MESSAGE_CODE_GLYPH that is
// no mark is as wide as a small or medium font.
static void WriteMessageFile(void) {
    StoreHalfword(0, sizeof(f_MessageFile));
    StoreHalfword(1, MESSAGE_COUNT);
    StoreMessage(0, "AB\nC");
    StoreMessage(1, "DEF");
    StoreMessage(2, "A\nB\nC\nDEFG\nHIJKL");
}

// The messages' spans and codes read as the cartridge's halfwords, so each
// message measures as its codes do. The first message bounds the table.
static void TestMessagesReadAsBigEndian(void) {
    s32 width = -1;
    s32 height = -1;

    MeasureMessage(0, FONT_SIZE_MEDIUM, &width, &height, 0);
    EXPECT(width == 2 * FONT_SIZE_MEDIUM);
    EXPECT(height == LINE_SPACING + FONT_SIZE_MEDIUM);

    MeasureMessage(1, FONT_SIZE_SMALL, &width, &height, 0);
    EXPECT(width == 3 * FONT_SIZE_SMALL);
    EXPECT(height == 0);
}

// A message of more than three lines, such as the erase-data question,
// measures as wide as its first three, as on the N64, whose lines past the
// third went to unused stack words. Each line still adds to the height.
static void TestMeasuresFirstThreeLines(void) {
    s32 width = -1;
    s32 height = -1;

    MeasureMessage(2, FONT_SIZE_SMALL, &width, &height, 0);
    EXPECT(width == FONT_SIZE_SMALL);
    EXPECT(height == 4 * (LINE_SPACING + FONT_SIZE_SMALL));
}

int main(void) {
    WriteMessageFile();
    LoadTextAssets();
    TestMessagesReadAsBigEndian();
    TestMeasuresFirstThreeLines();
    return ReportTestResults("text_test");
}
