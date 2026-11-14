/*
 * The game draws its messages in text slots. A message is a run of
 * character codes from the message table; some codes are controls, such
 * as a line break or a number that the slot holds.
 */

#ifndef TEXT_H
#define TEXT_H

#include <ultra64.h>

#include "screen/screen.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "text/text_format.h"

// The render mode for text, blending it over the frame by its coverage.
// It is a macro, as it pastes the cycle number into the blender macro.
#define TextRenderMode(cycle) \
    (IM_RD | CLR_ON_CVG | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA | \
        GBL_c##cycle(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM))

#define TEXT_SLOT_COUNT 12
#define TEXT_SLOT_VALUE_COUNT 4
// Where the scenes leave a text slot as they clear it; the position goes
// unused with NO_MESSAGE
#define CLEARED_TEXT_X (SCREEN_WIDTH / 2)
#define CLEARED_TEXT_Y 114

// A run of a text's characters
typedef struct TextSpan {
    u16 start;
    u16 length;
} TextSpan;

// The shadowed font's glyphs share one image
#define SHADOWED_FONT_IMAGE_WIDTH 256
#define SHADOWED_FONT_IMAGE_HEIGHT 32

// The characters that the fonts draw as the symbols for yards and feet
#define YARDS_GLYPH 0xF9
#define FEET_GLYPH 0xFB

#define TEXT_NAME_SIZE 20
// Added to a lowercase letter to give its character code
#define LOWERCASE_CODE_OFFSET 0x80
#define TEXT_PALETTE_COLOR_COUNT 4

// The character codes of messages. Codes below MESSAGE_CODE_GLYPH draw a
// character of the font, and codes below MESSAGE_CODE_CONTROL a glyph of the
// glyph file.
enum MessageCode {
    // The first that draws a glyph of the glyph file
    MESSAGE_CODE_GLYPH = 0x100,
    // The first that controls the text instead of drawing a glyph
    MESSAGE_CODE_CONTROL = 0x8000,
    // The codes up to MESSAGE_CODE_R_BUTTON draw an icon of TEXT_ASSET_ICONS
    MESSAGE_CODE_A_BUTTON = MESSAGE_CODE_CONTROL,
    MESSAGE_CODE_B_BUTTON,
    // The five codes from here all draw the Z button. Only the last appears in
    // the message file, in message 58's list of buttons.
    MESSAGE_CODE_UNUSED_Z_BUTTON_A,
    MESSAGE_CODE_UNUSED_Z_BUTTON_B,
    MESSAGE_CODE_UNUSED_Z_BUTTON_C,
    MESSAGE_CODE_UNUSED_Z_BUTTON_D,
    MESSAGE_CODE_Z_BUTTON,
    MESSAGE_CODE_C_UP,
    MESSAGE_CODE_C_DOWN,
    MESSAGE_CODE_C_LEFT,
    MESSAGE_CODE_C_RIGHT,
    // Drawn as nothing, and in no message
    MESSAGE_CODE_UNUSED_BLANK,
    MESSAGE_CODE_START_BUTTON,
    MESSAGE_CODE_L_BUTTON,
    MESSAGE_CODE_R_BUTTON,
    MESSAGE_CODE_NAME,
    // Then one for each of the slot's values
    MESSAGE_CODE_VALUE = 0x8014,
    // The current player's score, as the game mode counts it
    MESSAGE_CODE_SCORE = 0x8018,
    MESSAGE_CODE_LINE_BREAK,
    // Then one for each of the text palette's colors
    MESSAGE_CODE_PALETTE_COLOR = 0x8022,
    MESSAGE_CODE_SLOT_COLOR = 0x8026
};

typedef s32 MessageCode;
typedef u32 UMessageCode;
typedef u16 UMessageCode16;

// A glyph drawn with its own matrix, so text can be moved a glyph at a
// time. Glyphs are placed in groups.
typedef struct GlyphSprite {
    Mtx matrix;
    UMessageCode16 code;
    u8 fontSize;
    s8 group; // NO_GLYPH_GROUP when the sprite is free
    UTextFlags8 flags;
} GlyphSprite;

#define GLYPH_SPRITE_COUNT 300
// The scorecard's leaderboard uses all of them
#define GLYPH_GROUP_COUNT 20

typedef struct TextSlot {
    s32 x;
    s32 y;
    s32 fontSize;
    s32 shownCount;
    TextFlags flags;
    MessageId messageId;
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
    s32 values[TEXT_SLOT_VALUE_COUNT];
} TextSlot;

// The base of the decimal numbers the game shows
#define DECIMAL_BASE 10

// The digit font's glyphs are 16 pixels square, one to a row of its image,
// and drawn 14 pixels apart. The digits start at the second row, after the
// star.
#define DIGIT_GLYPH_SIZE 16
#define DIGIT_GLYPH_WIDTH 14
#define DIGIT_GLYPH_IMAGE_SIZE (DIGIT_GLYPH_SIZE * DIGIT_GLYPH_SIZE / 2)
#define DIGIT_GLYPH_TIMES 0
#define DIGIT_GLYPH_ZERO 1
#define DIGIT_GLYPH_COLON 11
#define DIGIT_GLYPH_PLUS 12
#define DIGIT_GLYPH_MINUS 13
#define DIGIT_GLYPH_PLUS_MINUS 14
#define DIGIT_GLYPH_CIRCLE 15
#define DIGIT_GLYPH_OFF 16
#define DIGIT_GLYPH_POINT 18
#define DIGIT_GLYPH_COUNT 16
#define DIGIT_FONT_HEIGHT (DIGIT_GLYPH_COUNT * DIGIT_GLYPH_SIZE)

// ASSET_PLACE_DIGITS holds 24x24 glyphs, the digits and then the suffixes,
// which places draw closer than a glyph apart
#define PLACE_GLYPH_SIZE 24
#define PLACE_GLYPH_COUNT 14
#define PLACE_GLYPHS_HEIGHT (PLACE_GLYPH_SIZE * PLACE_GLYPH_COUNT)
#define PLACE_GLYPH_SPACING 20
#define PLACE_SUFFIX_GLYPH_FIRST 10

// The suffixes that end a place's number
enum PlaceSuffix {
    PLACE_SUFFIX_ST,
    PLACE_SUFFIX_ND,
    PLACE_SUFFIX_RD,
    PLACE_SUFFIX_TH
};

typedef s32 PlaceSuffix;

// Places past 100 lose their hundreds; "th" covers the 4th to the 20th
#define PLACES_PER_HUNDRED 100
#define PLACE_TEENS_END 20

// Without this flag, PlaceGlyphGroup centers the text on its x
#define GLYPH_FLAG_LEFT_ALIGNED 0x10
// Only the top or the bottom part of a large glyph is drawn
#define GLYPH_FLAG_TOP_PART 0x01
#define GLYPH_FLAG_BOTTOM_PART 0x02
// Given this as its font size, PlaceGlyphGroup only frees the group
#define NO_FONT_SIZE (-1)

// The font that DrawShadowedText and its kin draw
extern ImageAsset *g_ShadowedFontImage;

// Formatted text on its way to the screen. No routine checks the length
// against this room, which is all there is before the next object.
#define PRINT_BUFFER_SIZE 40
extern char g_PrintBuffer[PRINT_BUFFER_SIZE];

void LoadTextAssets(void);
void FreeTextAssets(void);
void ClearTextSlots(void);
void PlaceGlyphGroup(s32 group, s32 fontSize, TextFlags flags, s32 x, s32 y, READ_ONLY char *text);
void SetTextPaletteColor(s32 red, s32 green, s32 blue, s32 colorIndex);
void SetTextSlotValue(s32 value, s32 slot, s32 valueIndex);
void SetTextName(char *name);
// SetTextSlot reads only each color's low byte, but its callers pass the
// colors as ints without narrowing them. text.c, which defines it with u8
// colors, takes the definition's form. With the u8 form everywhere, only
// help_board.c's and pause_menu.c's objects differ of the 20 files that call
// it, and DrawRingShotRecords' draft gives 35 words differing (3 in shape)
// rather than 26 (0).
#if defined(AVOID_UB) || defined(DEFINES_SET_TEXT_SLOT)
void SetTextSlot(
    s32 x,
    s32 y,
    MessageId messageId,
    s32 fontSize,
    s32 slot,
    TextFlags flags,
    u8 red,
    u8 green,
    u8 blue,
    u8 alpha
);
#else
void SetTextSlot(
    s32 x,
    s32 y,
    MessageId messageId,
    s32 fontSize,
    s32 slot,
    TextFlags flags,
    s32 red,
    s32 green,
    s32 blue,
    s32 alpha
);
#endif
void SetUpTextDrawing(Gfx **displayListHead);
void SetTextColor(Gfx **displayListHead, u8 red, u8 green, u8 blue);
void SetUpGradientTextDrawing(Gfx **displayListHead, s32 gradient);
void DrawTextSlots(Gfx **displayListHead);
void DrawLargeTextSlot(Gfx **displayListHead, s32 slot);
void DrawLargeText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawTextStub(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawFixedWidthLargeText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawMediumText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawCenteredLargeText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawCenteredMediumText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawGlyphGroup(Gfx **displayListHead, s32 group);
void SetUpPerspectiveTextDrawing(Gfx **displayListHead);
void RevealTextSlot(s32 slot);
Boolean IsTextSlotRevealed(s32 slot);
// save_menu.c defines OMITS_MEASURE_TEXT_FLAGS, as its one call leaves out
// MeasureText's flags. It is there to match: passing the flags gives 5 words
// differing in DrawGameSaveMenu, 1 in shape.
#if defined(AVOID_UB) || !defined(OMITS_MEASURE_TEXT_FLAGS)
s32 MeasureText(READ_ONLY char *text, s32 fontSize, TextFlags flags);
#else
s32 MeasureText(READ_ONLY char *text, s32 fontSize);
#endif
void MeasureMessage(MessageId messageId, s32 fontSize, s32 *width, s32 *height, s32 slot);
void DrawSmallText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void SetUpShadowedTextDrawing(Gfx **displayListHead);
void DrawCenteredSmallText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawShadowedText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawCondensedShadowedText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
void DrawCenteredNarrowText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
// The original's shot_labels.c calls DrawCenteredShadowedText without a
// prototype, passing x and y as they are rather than as s16s. It defines
// CALLS_CENTERED_SHADOWED_TEXT_UNPROTOTYPED to do so, unless AVOID_UB is set.
#if !defined(AVOID_UB) && defined(CALLS_CENTERED_SHADOWED_TEXT_UNPROTOTYPED)
void DrawCenteredShadowedText();
#else
void DrawCenteredShadowedText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text);
#endif
s32 GetDigitGlyphTop(char code);
void DrawDigitText(Gfx **displayListHead, s32 x, s32 y, READ_ONLY char *text);
void DrawLargeTextWithShadow(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    READ_ONLY char *text,
    s32 envColor,
    s32 primColor,
    TextFlags flags
);
// PlaceMessageGlyphGroup's result: half the message's width in the high
// halfword and half its height in the low one
#define PackHalfSize(halfWidth, halfHeight) (((halfWidth) << 16) | (halfHeight))
#define HalfSizeWidth(halfSize) ((u32)(halfSize) >> 16)
#define HalfSizeHeight(halfSize) ((halfSize) & 0xFFFF)

s32 PlaceMessageGlyphGroup(s32 group, s32 fontSize, s32 x, s32 y, MessageId messageId);

#endif
