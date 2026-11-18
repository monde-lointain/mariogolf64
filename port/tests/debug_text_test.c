/*
 * Tests of the debug font's two readers on a host: the CPU, which draws
 * messages straight into the framebuffers, and the RDP, which draws text
 * from the font and from textures of it. Whatever the font's glyphs look
 * like, both must see the same ones. The port fills the font from the ROM,
 * which here holds numbered bytes.
 */

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <string.h>
#include <ultra64.h>

#include "debug/debug_text.h"
#include "port/os_host.h"
#include "screen/screen.h"
#include "system/main.h"
#include "system/rom_arrays.h"
#include "test_support.h"

#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
// The pixel that messages draw as ink
#define MESSAGE_INK 0x0FFF
// The RDP's 4-bit texel that the font draws as ink
#define FONT_INK 0xF
// The font is 32 glyphs wide, so a row of its texels is 128 bytes
#define FONT_ROW_BYTES (32 * DEBUG_GLYPH_SIZE / 2)
#define FIRST_GLYPH ' '
#define G_SETTIMG_OPCODE 0xFD
// Bytes of the ROM past the font, which no fill reads
#define ROM_SLACK 16

Boolean8 g_IsDebugEnabled;
static u16 f_CurrentFramebuffer[FRAMEBUFFER_SIZE];
static u16 f_NextFramebuffer[FRAMEBUFFER_SIZE];
// A ROM that reaches past the font, whose bytes differ from their
// neighbors', so that a glyph read in the wrong order differs
static u8 *f_Rom;

Boolean IsGameFlagSet(u32 flag) {
    (void)flag;
    return FALSE;
}

void osViBlack(u8 isBlack) {
    (void)isBlack;
}

void *osViGetCurrentFramebuffer(void) {
    return f_CurrentFramebuffer;
}

void *osViGetNextFramebuffer(void) {
    return f_NextFramebuffer;
}

// Fills the font from a ROM of numbered bytes, as the port does at start-up
static void FillFontFromRom(void) {
    size_t size = debug_font_ROM_OFFSET + debug_font_ROM_ARRAY.size + ROM_SLACK;
    size_t i;

    f_Rom = SDL_malloc(size);
    SDL_assert_always(f_Rom != NULL);
    for (i = 0; i < size; i++) {
        f_Rom[i] = (u8)((i * 37) + 11);
    }

    PcInsertCartridge(f_Rom, size);
    osPcFillRomArray(&debug_font_ROM_ARRAY);
}

// The 4-bit texel at (x, y) of an image of rowBytes bytes a row, as the RDP
// reads it: bytes in memory's order, the high half first
static int ReadRdpTexel(const u8 *image, int rowBytes, int x, int y) {
    u8 byte = image[(y * rowBytes) + (x / 2)];

    return (x & 1) ? (byte & 0xF) : (byte >> 4);
}

static int CountInk(const u16 *framebuffer) {
    int count = 0;
    int i;

    for (i = 0; i < FRAMEBUFFER_SIZE; i++) {
        count += framebuffer[i] == MESSAGE_INK;
    }

    return count;
}

// Whether a texture's glyph inks the pixels that a message's glyph inked
// in the framebuffer's upper left corner
static bool DoesGlyphMatchMessage(const u8 *image, int rowBytes, int left, int top) {
    int x;
    int y;

    for (y = 0; y < DEBUG_GLYPH_SIZE; y++) {
        for (x = 0; x < DEBUG_GLYPH_SIZE; x++) {
            bool isTextureInk = ReadRdpTexel(image, rowBytes, left + x, top + y) == FONT_INK;
            bool isMessageInk = f_CurrentFramebuffer[(y * SCREEN_WIDTH) + x] == MESSAGE_INK;

            if (isTextureInk != isMessageInk) {
                return false;
            }
        }
    }

    return true;
}

static void DrawMessageGlyph(char character) {
    memset(f_CurrentFramebuffer, 0, sizeof(f_CurrentFramebuffer));
    DrawMessageCharacter((s8)character, 0, 0);
}

// The texture of a text shows each glyph as a message draws it, and the RDP
// reads it in the cartridge's order. The first and last glyphs bound the
// font, and a letter lies between.
static void TestTextTextureMatchesMessages(void) {
    static const char glyphs[] = {' ', 'A', 'z'};
    static u16 texture[DEBUG_GLYPH_SIZE * 2];
    size_t i;

    for (i = 0; i < sizeof(glyphs); i++) {
        DrawMessageGlyph(glyphs[i]);
        DrawTextIntoTexture(&glyphs[i], 1, texture);
        EXPECT(DoesGlyphMatchMessage((const u8 *)texture, DEBUG_GLYPH_SIZE / 2, 0, 0));
        EXPECT(osPcIsRomOrder(&texture[0]));
        EXPECT(osPcIsRomOrder((const u8 *)&texture[DEBUG_GLYPH_SIZE * 2] - 1));
    }

    // A letter has ink, so the comparison is not of two blank glyphs
    DrawMessageGlyph('A');
    EXPECT(CountInk(f_CurrentFramebuffer) > 0);
}

// The font that the RDP loads shows each glyph as a message draws it
static void TestRdpFontMatchesMessages(void) {
    static const char glyphs[] = {'A', 'z'};
    Gfx displayList[64];
    Gfx *gfx = displayList;
    const u8 *font = NULL;
    Gfx *command;
    size_t i;

    SetUpDebugTextDrawing(&gfx);
    for (command = displayList; command < gfx; command++) {
        if (((command->words.w0 >> 24) & 0xFF) == G_SETTIMG_OPCODE) {
            font = (const u8 *)command->words.w1;
            break;
        }
    }

    EXPECT(font != NULL);
    if (font == NULL) {
        return;
    }

    EXPECT(osPcIsRomOrder(font));
    for (i = 0; i < sizeof(glyphs); i++) {
        int glyph = glyphs[i] - FIRST_GLYPH;

        DrawMessageGlyph(glyphs[i]);
        EXPECT(DoesGlyphMatchMessage(
            font,
            FONT_ROW_BYTES,
            (glyph % 32) * DEBUG_GLYPH_SIZE,
            (glyph / 32) * DEBUG_GLYPH_SIZE
        ));
    }
}

int main(void) {
    FillFontFromRom();
    TestTextTextureMatchesMessages();
    TestRdpFontMatchesMessages();
    SDL_free(f_Rom);
    return ReportTestResults("debug_text_test");
}
