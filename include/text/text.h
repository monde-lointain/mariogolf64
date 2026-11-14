#ifndef TEXT_H
#define TEXT_H

#include <ultra64.h>

#include "screen/screen.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "text/text_format.h"

#define TextRenderMode(cycle) \
    (IM_RD | CLR_ON_CVG | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA | \
        GBL_c##cycle(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM))

#define TEXT_SLOT_COUNT 12
#define TEXT_SLOT_VALUE_COUNT 4
#define CLEARED_TEXT_X (SCREEN_WIDTH / 2)
#define CLEARED_TEXT_Y 114

typedef struct TextSpan {
    u16 start;
    u16 length;
} TextSpan;

#define SHADOWED_FONT_IMAGE_WIDTH 256
#define SHADOWED_FONT_IMAGE_HEIGHT 32

#define YARDS_GLYPH 0xF9
#define FEET_GLYPH 0xFB

#define TEXT_NAME_SIZE 20
#define LOWERCASE_CODE_OFFSET 0x80
#define TEXT_PALETTE_COLOR_COUNT 4

typedef struct GlyphSprite {
    Mtx matrix;
    u16 code;
    u8 fontSize;
    s8 group;
    UTextFlags8 flags;
} GlyphSprite;

#define GLYPH_SPRITE_COUNT 300
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

#define DECIMAL_BASE 10

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

#define PLACE_GLYPH_SIZE 24
#define PLACE_GLYPH_COUNT 14
#define PLACE_GLYPHS_HEIGHT (PLACE_GLYPH_SIZE * PLACE_GLYPH_COUNT)
#define PLACE_GLYPH_SPACING 20
#define PLACE_SUFFIX_GLYPH_FIRST 10

enum PlaceSuffix {
    PLACE_SUFFIX_ST,
    PLACE_SUFFIX_ND,
    PLACE_SUFFIX_RD,
    PLACE_SUFFIX_TH
};

typedef s32 PlaceSuffix;

#define PLACES_PER_HUNDRED 100
#define PLACE_TEENS_END 20

#define GLYPH_FLAG_LEFT_ALIGNED 0x10
#define GLYPH_FLAG_TOP_PART 0x01
#define GLYPH_FLAG_BOTTOM_PART 0x02
#define NO_FONT_SIZE (-1)

extern ImageAsset *g_ShadowedFontImage;

#define PRINT_BUFFER_SIZE 40
extern char g_PrintBuffer[PRINT_BUFFER_SIZE];

void LoadTextAssets(void);
void FreeTextAssets(void);
void ClearTextSlots(void);
void PlaceGlyphGroup(s32 group, s32 fontSize, TextFlags flags, s32 x, s32 y, READ_ONLY char *text);
void SetTextPaletteColor(s32 red, s32 green, s32 blue, s32 colorIndex);
void SetTextSlotValue(s32 value, s32 slot, s32 valueIndex);
void SetTextName(char *name);
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
#define PackHalfSize(halfWidth, halfHeight) (((halfWidth) << 16) | (halfHeight))
#define HalfSizeWidth(halfSize) ((u32)(halfSize) >> 16)
#define HalfSizeHeight(halfSize) ((halfSize) & 0xFFFF)

s32 PlaceMessageGlyphGroup(s32 group, s32 fontSize, s32 x, s32 y, MessageId messageId);

#endif
