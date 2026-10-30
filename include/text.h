#ifndef TEXT_H
#define TEXT_H

#include <ultra64.h>

#include "text_format.h"

/*
 * The game draws its messages in text slots. A message is a run of
 * character codes from the message table; some codes are controls, such
 * as a line break or a number that the slot holds.
 */

/*
 * The render mode for text, blending it over the frame by its coverage.
 * It is a macro, as it pastes the cycle number into the blender macro.
 */
#define TextRenderMode(cycle) \
  (IM_RD | CLR_ON_CVG | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA | \
      GBL_c##cycle(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM))

#define TEXT_SLOT_COUNT 12
#define TEXT_SLOT_VALUE_COUNT 4
/* Given this as its message, SetTextSlot clears the slot */
#define NO_MESSAGE (-1)

/* The number of one of the game's messages, or NO_MESSAGE */
typedef s32 MessageId;

/* The shadowed font's glyphs share one image */
#define SHADOWED_FONT_IMAGE_WIDTH 256
#define SHADOWED_FONT_IMAGE_HEIGHT 32

/* The characters that the fonts draw as the symbols for yards and feet */
#define YARDS_GLYPH 0xF9
#define FEET_GLYPH 0xFB

/* MeasureText flag: use each character's own width rather than the font size */
#define MEASURE_TEXT_PROPORTIONAL 0x02
/*
 * MeasureText only tests for MEASURE_TEXT_PROPORTIONAL, so this
 * measures each character at the font size
 */
#define MEASURE_TEXT_FIXED_WIDTH 0x01

#define TEXT_NAME_SIZE 20
/* Added to a lowercase letter to give its character code */
#define LOWERCASE_CODE_OFFSET 0x80
#define TEXT_PALETTE_COLOR_COUNT 4

/*
 * A glyph drawn with its own matrix, so text can be moved a glyph at a
 * time. Glyphs are placed in groups.
 */
typedef struct GlyphSprite {
  /* 0x00 */ Mtx matrix;
  /* 0x40 */ u16 code;
  /* 0x42 */ u8 fontSize;
  /* 0x43 */ s8 group; /* NO_GLYPH_GROUP when the sprite is free */
  /* 0x44 */ u8 flags;
} GlyphSprite; /* size = 0x48 */

#define GLYPH_SPRITE_COUNT 300
/* The scorecard's leaderboard uses all of them */
#define GLYPH_GROUP_COUNT 20

typedef struct TextSlot {
  /* 0x00 */ s32 x;
  /* 0x04 */ s32 y;
  /* 0x08 */ s32 fontSize;
  /* 0x0C */ s32 shownCount;
  /* 0x10 */ TextFlags flags;
  /* 0x14 */ MessageId messageId;
  /* 0x18 */ u8 red;
  /* 0x19 */ u8 green;
  /* 0x1A */ u8 blue;
  /* 0x1B */ u8 alpha;
  /* 0x1C */ s32 values[TEXT_SLOT_VALUE_COUNT];
} TextSlot; /* size = 0x2C */

/*
 * The digit font's glyphs are 16 pixels square, one to a row of its image,
 * and drawn 14 pixels apart. The digits start at the second row, after the
 * star.
 */
#define DIGIT_GLYPH_SIZE 16
#define DIGIT_GLYPH_WIDTH 14
#define DIGIT_GLYPH_STAR 0
#define DIGIT_GLYPH_ZERO 1
#define DIGIT_GLYPH_COLON 11
#define DIGIT_GLYPH_PLUS 12
#define DIGIT_GLYPH_MINUS 13
#define DIGIT_GLYPH_HASH 14
#define DIGIT_GLYPH_EXCLAMATION 15
#define DIGIT_GLYPH_COUNT 16
#define DIGIT_FONT_HEIGHT (DIGIT_GLYPH_COUNT * DIGIT_GLYPH_SIZE)

/*
 * ASSET_PLACE_DIGITS holds 24x24 glyphs, the digits and then the suffixes,
 * which places draw closer than a glyph apart
 */
#define PLACE_GLYPH_SIZE 24
#define PLACE_GLYPH_COUNT 14
#define PLACE_GLYPHS_HEIGHT (PLACE_GLYPH_SIZE * PLACE_GLYPH_COUNT)
#define PLACE_GLYPH_SPACING 20
#define PLACE_SUFFIX_GLYPH_FIRST 10

/* The suffixes that end a place's number */
enum PlaceSuffix {
  PLACE_SUFFIX_ST,
  PLACE_SUFFIX_ND,
  PLACE_SUFFIX_RD,
  PLACE_SUFFIX_TH
};

/* Places past 100 lose their hundreds; "th" covers the 4th to the 20th */
#define PLACES_PER_HUNDRED 100
#define PLACE_TEENS_END 20

/* Without this flag, PlaceGlyphGroup centers the text on its x */
#define GLYPH_FLAG_LEFT_ALIGNED 0x10
/* Only the top or the bottom part of a large glyph is drawn */
#define GLYPH_FLAG_TOP_PART 0x01
#define GLYPH_FLAG_BOTTOM_PART 0x02
/* Given this as its font size, PlaceGlyphGroup only frees the group */
#define NO_FONT_SIZE (-1)

/* The font that DrawShadowedText and its kin draw */
extern void* g_ShadowedFontImage;

void LoadTextAssets(void);
void FreeTextAssets(void);
void ClearTextSlots(void);
void PlaceGlyphGroup(
    s32 group, s32 fontSize, TextFlags flags, s32 x, s32 y, char* text);
void SetTextPaletteColor(s32 red, s32 green, s32 blue, s32 colorIndex);
void SetTextSlotValue(s32 value, s32 slot, s32 valueIndex);
void SetTextName(char* name);
/* SetTextSlot reads only each color's low byte, but its callers pass the
 * colors as ints without narrowing them. text.c, which defines it with u8
 * colors, skips this declaration. */
#ifndef DEFINES_SET_TEXT_SLOT
void SetTextSlot(s32 x, s32 y, MessageId messageId, s32 fontSize, s32 slot,
    TextFlags flags, s32 red, s32 green, s32 blue, s32 alpha);
#endif
void SetUpTextDrawing(Gfx** displayListHead);
void SetTextColor(Gfx** displayListHead, s32 red, s32 green, s32 blue);
void SetUpGradientTextDrawing(Gfx** displayListHead, s32 gradient);
void DrawTextSlots(Gfx** displayListHead);
void DrawLargeTextSlot(Gfx** displayListHead, s32 slot);
void DrawLargeText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawTextStub(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawFixedWidthLargeText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawMediumText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawCenteredLargeText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawCenteredMediumText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawGlyphGroup(Gfx** displayListHead, s32 group);
void SetUpPerspectiveTextDrawing(Gfx** displayListHead);
void RevealTextSlot(s32 slot);
Boolean IsTextSlotRevealed(s32 slot);
s32 MeasureText(char* text, s32 fontSize, TextFlags flags);
void MeasureMessage(
    MessageId messageId, s32 fontSize, s32* width, s32* height, s32 slot);
/* The text routines take char* rather than const char*: GCC reads through a
 * const pointer as unchanging memory, which compiles them differently */
void DrawSmallText(Gfx** displayListHead, s16 x, s16 y, char* text);
void BeginShadowedText(Gfx** displayListHead);
void DrawCenteredSmallText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawShadowedText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawCondensedShadowedText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawCenteredNarrowText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawCenteredShadowedText(Gfx** displayListHead, s16 x, s16 y, char* text);
s32 GetDigitGlyphTop(char code);
void DrawDigitText(Gfx** displayListHead, s32 x, s32 y, char* text);
void DrawLargeTextWithShadow(Gfx** displayListHead, s32 x, s32 y, char* text,
    s32 envColor, s32 primColor, TextFlags flags);

#endif
