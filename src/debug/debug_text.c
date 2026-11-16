/*
 * Debug text is kept in a character grid that is drawn over each frame.
 * Characters with the high bit set are drawn highlighted. A ring buffer of
 * lines, the log, can be copied into the grid. Messages are also drawn
 * straight into the framebuffers; the fault handler prints with them.
 */

#include "debug/debug_text.h"

#include <ultra64.h>

#include "debug/game_flags.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "text/text.h"

#define TEXT_COLUMN_COUNT 40
#define TEXT_ROW_COUNT 30
#define LOG_LINE_COUNT 120
// The log is shown above this many rows at the bottom of the grid
#define LOG_BOTTOM_MARGIN 3
// How many of the log's last lines are shown
#define LOG_SHOWN_LINE_COUNT 10

#define GLYPHS_PER_FONT_ROW 32
#define FIRST_GLYPH ' '
#define LAST_GLYPH 'z'
// The font is 4 bits per pixel, so each u16 holds 4 pixels
#define PIXELS_PER_FONT_WORD 4
#define FONT_ROW_WORD_COUNT (GLYPHS_PER_FONT_ROW * DEBUG_GLYPH_SIZE / PIXELS_PER_FONT_WORD)
#define FONT_WIDTH 256
#define FONT_HEIGHT 24
// The pixel value that is drawn as ink in messages
#define FONT_INK 0xF

#define HIGHLIGHT_BIT 0x80
#define CHARACTER_MASK 0x7F

#define MESSAGE_INK_COLOR 0x0FFF
#define MESSAGE_PAPER_COLOR 0x0001

#define HEX_DIGIT_COUNT 8

// Its 0x1800 bytes are twice the font's 4-bit texels, and the second half
// is zero
static u16 f_DebugFont[FONT_WIDTH * FONT_HEIGHT / 2] RCP_ALIGNED = {
#include "assets/debug/debug_text/debug_font.ia4.inc.c"
};
u16 *g_MessageFramebuffer = NULL;
static s8 f_DebugTextHighlight = 0;
static s32 f_DebugLogShownLineCount = LOG_SHOWN_LINE_COUNT;

static u8 f_DebugText[TEXT_ROW_COUNT * TEXT_COLUMN_COUNT];
static u8 f_DebugLog[LOG_LINE_COUNT * TEXT_COLUMN_COUNT];
static s32 f_DebugLogCursor;

void SetUpDebugTextDrawing(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, TextRenderMode(1), TextRenderMode(2));
    gDPSetCombineMode(gfx++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(f_DebugFont),
        G_IM_FMT_IA,
        FONT_WIDTH,
        FONT_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Draws the glyphs of text into a 4-bit texture one glyph tall and length
// glyphs wide.
void DrawTextIntoTexture(READ_ONLY char *text, s32 length, u16 *texture) {
    s32 index;
    s32 rowFontIndex;
    s32 pixelRow;
    s32 wordColumn;
    s32 row;
    s32 fontIndex;
    s32 textureIndex;
    s32 character;
    // The glyph's first word in the font, copied to fontIndex to match:
    // computing fontIndex directly gives 14 words differing, 2 in shape
    s32 glyphFontIndex;

    for (index = 0; index < length; index++) {
        // Signed to match; either way a byte past 0x7F is drawn as FIRST_GLYPH.
        // Reading it unsigned, by cast or as the plain char, gives 2 words
        // differing, both in shape.
        character = (s8)*text;
        if (character == '\0') {
            break;
        }

        if (character < FIRST_GLYPH) {
            character = FIRST_GLYPH;
        }

        if (character > LAST_GLYPH) {
            character = FIRST_GLYPH;
        }

        character -= FIRST_GLYPH;
        glyphFontIndex =
            ((character & (GLYPHS_PER_FONT_ROW - 1)) * (DEBUG_GLYPH_SIZE / PIXELS_PER_FONT_WORD)) |
            ((character / GLYPHS_PER_FONT_ROW) * (DEBUG_GLYPH_SIZE * FONT_ROW_WORD_COUNT));
        pixelRow = 0;
        fontIndex = glyphFontIndex;
        textureIndex = index;
        for (; pixelRow < DEBUG_GLYPH_SIZE; pixelRow++) {
            wordColumn = 0;
            row = textureIndex * 2;
            rowFontIndex = fontIndex;
            for (; wordColumn < DEBUG_GLYPH_SIZE / PIXELS_PER_FONT_WORD; wordColumn++) {
                texture[row + wordColumn] = f_DebugFont[rowFontIndex + wordColumn];
            }

            fontIndex += FONT_ROW_WORD_COUNT;
            textureIndex += length;
        }
        text++;
    }
}

// Puts text in the grid and draws it into the framebuffers at once
void PrintMessage(s8 *text, u32 column, u32 row) {
    PutDebugText(text, column, row);
    while (*text != '\0') {
        DrawMessageCharacter(*text, column, row);
        text++;
        column++;
    }
}

// Draws a character of the debug font straight into both framebuffers, at a
// column and row of the message grid, and turns the screen back on.
void DrawMessageCharacter(s8 character, u32 column, u32 row) {
    u16 *currentFramebuffer = UncachedAddress(osViGetCurrentFramebuffer());
    u16 *nextFramebuffer = UncachedAddress(osViGetNextFramebuffer());
    s32 glyphStart;
    s32 pixelRow;
    s32 pixelColumn;
    s32 pixelIndex;
    u16 color;

    // The main loop shows this framebuffer from now on
    g_MessageFramebuffer = currentFramebuffer;
    if (character < FIRST_GLYPH) {
        character = FIRST_GLYPH;
    }

    if (character > LAST_GLYPH) {
        character = FIRST_GLYPH;
    }

    character -= FIRST_GLYPH;
    glyphStart =
        ((character & (GLYPHS_PER_FONT_ROW - 1)) * (DEBUG_GLYPH_SIZE / PIXELS_PER_FONT_WORD)) +
        ((character / GLYPHS_PER_FONT_ROW) * (DEBUG_GLYPH_SIZE * FONT_ROW_WORD_COUNT));
    pixelRow = 0;
    if (column >= TEXT_COLUMN_COUNT || row >= TEXT_ROW_COUNT) {
        return;
    }

    for (; pixelRow < DEBUG_GLYPH_SIZE; pixelRow++) {
        for (pixelColumn = 0; pixelColumn < DEBUG_GLYPH_SIZE; pixelColumn++) {
            color = ((f_DebugFont
                             [glyphStart + (pixelRow * FONT_ROW_WORD_COUNT) +
                                 (pixelColumn / PIXELS_PER_FONT_WORD)] >>
                         ((3 - (pixelColumn & 3)) * 4)) &
                        0xF) == FONT_INK ?
                MESSAGE_INK_COLOR :
                MESSAGE_PAPER_COLOR;
            pixelIndex = (((row * DEBUG_GLYPH_SIZE) + pixelRow) * SCREEN_WIDTH) +
                ((column * DEBUG_GLYPH_SIZE) + pixelColumn);
            currentFramebuffer[pixelIndex] = color;
            nextFramebuffer[pixelIndex] = color;
        }
    }

    osViBlack(FALSE);
}

void ClearDebugText(void) {
    s32 index;
    u8 *cell;

    for (index = ARRAY_COUNT(f_DebugText) - 1, cell = &f_DebugText[index]; index >= 0; index--) {
        *cell-- = 0;
    }
}

void DrawDebugText(Gfx **displayListHead) {
    s32 index = 0;
    u32 highlight = 0;
    s32 row;
    s32 column;
    u8 character;
    s32 glyph;
    Gfx *gfx;

    SetUpDebugTextDrawing(displayListHead);
    if (IsGameFlagSet(GAME_FLAG_HIDE_DEBUG_TEXT)) {
        return;
    }

    gfx = *displayListHead;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    for (row = 0; row < TEXT_ROW_COUNT; row++) {
        for (column = 0; column < TEXT_COLUMN_COUNT; index++, column++) {
            character = f_DebugText[index];
            if (character <= ' ') {
                continue;
            }

            glyph = (character & CHARACTER_MASK) - FIRST_GLYPH;
            if ((character & HIGHLIGHT_BIT) != highlight) {
                highlight = character & HIGHLIGHT_BIT;
                gDPPipeSync(gfx++);
                if (highlight) {
                    gDPSetPrimColor(gfx++, 0, 0, 255, 0, 0, OPAQUE_ALPHA);
                }
                else {
                    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
                }

                gDPPipeSync(gfx++);
            }

            gSPTextureRectangle(
                gfx++,
                QuarterPixels(column * DEBUG_GLYPH_SIZE),
                QuarterPixels(row * DEBUG_GLYPH_SIZE),
                QuarterPixels((column * DEBUG_GLYPH_SIZE) + DEBUG_GLYPH_SIZE),
                QuarterPixels((row * DEBUG_GLYPH_SIZE) + DEBUG_GLYPH_SIZE),
                G_TX_RENDERTILE,
                TexelCoordinate((glyph & 0x1F) * DEBUG_GLYPH_SIZE),
                TexelCoordinate((glyph >> 5) * DEBUG_GLYPH_SIZE),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Highlights the next text put in the grid
void SetDebugTextHighlight(Boolean isHighlighted) {
    if (isHighlighted) {
        f_DebugTextHighlight = HIGHLIGHT_BIT;
    }
    else {
        f_DebugTextHighlight = 0;
    }
}

void PutDebugText(s8 *text, s32 column, s32 row) {
    s8 highlight = f_DebugTextHighlight;
    u8 *cell = &f_DebugText[(row * TEXT_COLUMN_COUNT) + column];
    s32 character;

    while (TRUE) {
        character = *text++;
        if (character == '\0') {
            break;
        }

        if (cell >= f_DebugText && cell < f_DebugText + ARRAY_COUNT(f_DebugText)) {
            *cell = highlight | character;
        }

        cell++;
    }

    f_DebugTextHighlight = 0;
}

void PrintDebugText(const char *text, s32 column, s32 row) {
    if (g_IsDebugEnabled) {
        PutDebugText((s8 *)text, column, row);
    }
}

// Prints value in hex, right-aligned so its last digit is 7 columns right of
// column
void PrintDebugHex(u32 number, s32 column, s32 row) {
    char digits[HEX_DIGIT_COUNT + 1];
    char *digit = &digits[HEX_DIGIT_COUNT];
    s32 nibble;
    s32 character;

    column += HEX_DIGIT_COUNT;
    *digit = '\0';
    do {
        nibble = number & 0xF;
        character = nibble + '0';
        if (character > '9') {
            character = nibble + 'A' - 10;
        }

        *--digit = character;
        number >>= 4;
        column--;
    }
    while (number != 0);
    PrintDebugText(digit, column, row);
}

// Prints all 8 hex digits of value
void PrintDebugPaddedHex(u32 number, s32 column, s32 row) {
    char digits[HEX_DIGIT_COUNT + 1];
    char *digit = &digits[HEX_DIGIT_COUNT];
    s32 nibble;
    s32 character;

    column += HEX_DIGIT_COUNT;
    *digit = '\0';
    do {
        nibble = number & 0xF;
        character = nibble + '0';
        if (character > '9') {
            character = nibble + 'A' - 10;
        }

        *--digit = character;
        column--;
        number >>= 4;
    }
    while (digit != digits);
    PrintDebugText(digit, column, row);
}

void AddToDebugLog(char *text) {
    s32 blanksLeft;
    s8 character;

    while (TRUE) {
        character = *text++;
        if (character == '\0') {
            break;
        }

        if (character == '\r' || character == '\n') {
            f_DebugLogCursor =
                (((f_DebugLogCursor / TEXT_COLUMN_COUNT) * TEXT_COLUMN_COUNT) + TEXT_COLUMN_COUNT) %
                ARRAY_COUNT(f_DebugLog);
            for (blanksLeft = TEXT_COLUMN_COUNT; blanksLeft != 0; blanksLeft--) {
                f_DebugLog[f_DebugLogCursor++] = ' ';
            }

            f_DebugLogCursor -= TEXT_COLUMN_COUNT;
        }
        else {
            f_DebugLog[f_DebugLogCursor++] = character;
        }

        f_DebugLogCursor %= ARRAY_COUNT(f_DebugLog);
    }
}

// Copies the last lines of the log into the grid
void ShowDebugLog(void) {
    s32 textIndex = ARRAY_COUNT(f_DebugText) -
        ((f_DebugLogShownLineCount + LOG_BOTTOM_MARGIN) * TEXT_COLUMN_COUNT);
    // The line is reused for the cursor's line, to match: a cursorLine local,
    // or the division written into logIndex, gives 30 words differing, 2 in
    // shape
    s32 line = f_DebugLogCursor / TEXT_COLUMN_COUNT;
    s32 logIndex =
        (((line - f_DebugLogShownLineCount) * TEXT_COLUMN_COUNT) + ARRAY_COUNT(f_DebugLog)) %
        ARRAY_COUNT(f_DebugLog);
    s32 column;

    for (line = 0; line < f_DebugLogShownLineCount; line++) {
        for (column = 0; column < TEXT_COLUMN_COUNT; column++) {
            f_DebugText[textIndex++] = f_DebugLog[logIndex++];
        }

        logIndex %= ARRAY_COUNT(f_DebugLog);
    }
}
