#ifndef TEXT_H
#define TEXT_H

#include <ultra64.h>

/*
 * The game draws its messages in text slots. A message is a run of
 * character codes from the message table; some codes are controls, such
 * as a line break or a number that the slot holds.
 */

#define TEXT_SLOT_COUNT 12
#define TEXT_SLOT_VALUE_COUNT 4
/* Given this as its message, SetTextSlot clears the slot */
#define NO_MESSAGE (-1)

/* Flags of a text slot */
#define TEXT_FLAG_CENTERED 0x10
/* DrawTextSlots skips a slot with this flag */
#define TEXT_FLAG_UNK20 0x20
/* Each glyph of the large font takes the same width */
#define TEXT_FLAG_FIXED_WIDTH 0x80

/* The font size, in pixels, of the font whose characters differ in width */
#define FONT_SIZE_LARGE 12

/* MeasureText flag: use each character's own width rather than the font size */
#define MEASURE_TEXT_PROPORTIONAL 0x02
/*
 * MeasureText only tests for MEASURE_TEXT_PROPORTIONAL, so this
 * measures each character at the font size
 */
#define MEASURE_TEXT_FIXED_WIDTH 0x01

#define TEXT_NAME_SIZE 20
#define TEXT_PALETTE_COLOR_COUNT 4

typedef struct TextSlot {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 fontSize;
    /* 0x0C */ s32 shownCount;
    /* 0x10 */ s32 flags;
    /* 0x14 */ s32 messageId;
    /* 0x18 */ u8 red;
    /* 0x19 */ u8 green;
    /* 0x1A */ u8 blue;
    /* 0x1B */ u8 alpha;
    /* 0x1C */ s32 values[TEXT_SLOT_VALUE_COUNT];
} TextSlot; /* size = 0x2C */

void LoadTextAssets(void);
void FreeTextAssets(void);
void ClearTextSlots(void);
void SetTextPaletteColor(s32 red, s32 green, s32 blue, s32 colorIndex);
void SetTextSlotValue(s32 value, s32 slot, s32 valueIndex);
void SetTextName(char *name);
void SetTextSlot(s32 x, s32 y, s32 messageId, s32 fontSize, s32 slot, s32 flags, u8 red, u8 green, u8 blue,
                 u8 alpha);
void SetUpTextDrawing(Gfx **displayListHead);
void DrawTextSlots(Gfx **displayListHead);
void RevealTextSlot(s32 slot);
s32 IsTextSlotRevealed(s32 slot);
s32 MeasureText(char *text, s32 fontSize, s32 flags);
void MeasureMessage(s32 messageId, s32 fontSize, s32 *width, s32 *height, s32 slot);
void DrawLargeTextWithShadow(Gfx **displayListHead, s32 x, s32 y, char *text, s32 envColor, s32 primColor,
                             s32 flags);

#endif
