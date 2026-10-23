#include "asset.h"
#include "common.h"
#include "functions.h"
#include "heap.h"
#include "libc.h"
#include "text.h"
#include "variables.h"

/* The messages and their character codes */
typedef struct MessageEntry {
    /* 0x00 */ u16 start; /* In codes from the first code */
    /* 0x02 */ u16 length;
} MessageEntry; /* size = 0x04 */

typedef struct MessageTable {
    /* 0x00 */ u16 size;
    /* 0x02 */ u16 messageCount;
    /* 0x04 */ MessageEntry messages[1]; /* messageCount of them, then the codes */
} MessageTable;

#define TEXT_ASSET_SMALL_FONT 0x4B3
#define TEXT_ASSET_MEDIUM_FONT 0x4B5
#define TEXT_ASSET_LARGE_FONT 0x4B6
#define TEXT_ASSET_ICONS 0x4B0
#define TEXT_ASSET_UNK4B2 0x4B2
#define TEXT_ASSET_UNK4B8 0x4B8
#define TEXT_ASSET_UNK4B9 0x4B9
/*
 * A glyph file's header holds the glyph count at 2 and each glyph's size
 * at 4; a message file's holds the file's size at 0. Both are big-endian.
 */
#define FILE_HEADER_SIZE 8

/* Character codes */
#define MESSAGE_CODE_GLYPH 0x100 /* The first that draws a glyph of the glyph file */
#define MESSAGE_CODE_CONTROL 0x8000
#define MESSAGE_CODE_NAME 0x800F
#define MESSAGE_CODE_VALUE 0x8014 /* Then one for each of the slot's values */
#define MESSAGE_CODE_LINE_BREAK 0x8019
/*
 * Marks drawn over the character before them take no room. In JIS X 0201,
 * 0xDE and 0xDF are the kana voicing marks.
 */
#define CHARACTER_CODE_MARK_FIRST 0xDE
#define CHARACTER_CODE_MARK_COUNT 2
#define CHARACTER_CODE_OTHER_MARK_FIRST 0x9E
/* Added to a lowercase letter to give its character code */
#define LOWERCASE_CODE_OFFSET 0x80
/* The pixels between lines, over the font size */
#define LINE_SPACING 6
#define MESSAGE_LINE_COUNT 3
/* The width of an icon, in pixels */
#define ICON_WIDTH 12

/* Where the glyph and message files are in the ROM */
extern u8 g_GlyphFileRomStart[];
extern u8 g_MessageFileRomStart[];
extern void *g_SmallFontImage;
extern void *g_MediumFontImage;
extern void *g_LargeFontImage;
extern void *g_TextIconImage;
extern void *D_8012F4E0;
extern void *D_800E1C3C[2];
extern void *g_MessageGlyphs;
extern MessageTable *g_Messages;
/* The index of the first character code in the message table, in halfwords */
extern s32 g_MessageCodesStart;
extern TextSlot g_TextSlots[TEXT_SLOT_COUNT];
extern u8 g_TextPalette[TEXT_PALETTE_COLOR_COUNT][3];
extern char g_TextName[TEXT_NAME_SIZE];
/* The width of each character of the large font */
extern u8 g_LargeFontWidths[];

void ClearGlyphSprites(void);
s32 GetTextLength(char *text);

const char g_TextStringFormat[] = "%s";
const char g_TextNumberFormat[] = "%d";
const char g_MatchScoreEvenText[] = "EVEN";
const char g_MatchScoreUpFormat[] = "%dUP";
const char g_MatchScoreDownFormat[] = "%dDOWN";
const char g_SkinsScoreFormat[] = "%dP";
const char g_ParScoreEvenText[] = "#0";
const char g_ParScoreFormat[] = "%+d";

/*
 * Loads the fonts, the icons and the message table, and clears the text
 * slots.
 */
void LoadTextAssets(void) {
    EncodedAsset asset;
    u8 *header;
    s32 size;

    D_8012F4E0 = AllocMemory(OpenEncodedAsset(TEXT_ASSET_UNK4B2, &asset));
    ReadEncodedAsset(D_8012F4E0, &asset);
    g_SmallFontImage = AllocMemory(OpenEncodedAsset(TEXT_ASSET_SMALL_FONT, &asset));
    ReadEncodedAsset(g_SmallFontImage, &asset);
    g_MediumFontImage = AllocMemory(OpenEncodedAsset(TEXT_ASSET_MEDIUM_FONT, &asset));
    ReadEncodedAsset(g_MediumFontImage, &asset);
    g_LargeFontImage = AllocMemory(OpenEncodedAsset(TEXT_ASSET_LARGE_FONT, &asset));
    ReadEncodedAsset(g_LargeFontImage, &asset);
    g_TextIconImage = AllocMemory(OpenEncodedAsset(TEXT_ASSET_ICONS, &asset));
    ReadEncodedAsset(g_TextIconImage, &asset);
    D_800E1C3C[0] = AllocMemory(OpenEncodedAsset(TEXT_ASSET_UNK4B9, &asset));
    ReadEncodedAsset(D_800E1C3C[0], &asset);
    D_800E1C3C[1] = AllocMemory(OpenEncodedAsset(TEXT_ASSET_UNK4B8, &asset));
    ReadEncodedAsset(D_800E1C3C[1], &asset);
    header = AllocMemory(FILE_HEADER_SIZE);
    nuPiReadRom((u32)g_GlyphFileRomStart, header, FILE_HEADER_SIZE);
    size = ((header[2] << 8) | header[3]) * ((header[4] << 8) | header[5]) + FILE_HEADER_SIZE;
    g_MessageGlyphs = AllocMemory(size);
    nuPiReadRom((u32)g_GlyphFileRomStart, g_MessageGlyphs, size);
    nuPiReadRom((u32)g_MessageFileRomStart, header, FILE_HEADER_SIZE);
    size = (header[0] << 8) | header[1];
    FreeMemory(&header);
    g_Messages = AllocMemory(size);
    nuPiReadRom((u32)g_MessageFileRomStart, g_Messages, size);
    /* The halfwords of the table's header and its entries come first */
    g_MessageCodesStart = g_Messages->messageCount * 2 + 2;
    ClearTextSlots();
    ClearGlyphSprites();
}

/* Frees what LoadTextAssets loaded, except the two images at D_800E1C3C */
void FreeTextAssets(void) {
    FreeMemory(&D_8012F4E0);
    FreeMemory(&g_SmallFontImage);
    FreeMemory(&g_MediumFontImage);
    FreeMemory(&g_LargeFontImage);
    FreeMemory(&g_TextIconImage);
    FreeMemory(&g_MessageGlyphs);
    FreeMemory(&g_Messages);
}

INCLUDE_ASM("asm/nonmatchings/text", func_80071608);

void ClearTextSlots(void) {
    s32 slot;

    for (slot = 0; slot != TEXT_SLOT_COUNT; slot++) {
        g_TextSlots[slot].messageId = NO_MESSAGE;
    }
}

void SetTextPaletteColor(s32 red, s32 green, s32 blue, s32 colorIndex) {
    g_TextPalette[colorIndex][0] = red;
    g_TextPalette[colorIndex][1] = green;
    g_TextPalette[colorIndex][2] = blue;
}

void SetTextSlotValue(s32 value, s32 slot, s32 valueIndex) {
    g_TextSlots[slot].values[valueIndex] = value;
}

/* Sets the name that MESSAGE_CODE_NAME shows, as character codes */
void SetTextName(char *name) {
    s32 i;
    char character;

    for (i = 0; i != TEXT_NAME_SIZE - 1; i++) {
        character = name[i];
        g_TextName[i] = character;
        if (character >= 'a' && character <= 'z') {
            g_TextName[i] = character + LOWERCASE_CODE_OFFSET;
        }
        if (name[i] == '\0') {
            break;
        }
    }
    g_TextName[TEXT_NAME_SIZE - 1] = '\0';
}

/*
 * Shows a message in a slot at x and y, or around them when it is
 * centered. A new message starts unrevealed.
 */
#ifdef NON_MATCHING
void SetTextSlot(s32 x, s32 y, s32 messageId, s32 fontSize, s32 slot, s32 flags, u8 red, u8 green, u8 blue,
                 u8 alpha) {
    s32 width;
    s32 height;

    if (messageId != NO_MESSAGE) {
        width = 0;
        height = 0;
        if (flags & TEXT_FLAG_CENTERED) {
            MeasureMessage(messageId, FONT_SIZE_LARGE, &width, &height, slot);
        }
        g_TextSlots[slot].red = red;
        g_TextSlots[slot].green = green;
        g_TextSlots[slot].blue = blue;
        g_TextSlots[slot].alpha = alpha;
        g_TextSlots[slot].fontSize = fontSize;
        g_TextSlots[slot].flags = flags;
        g_TextSlots[slot].x = x - width / 2;
        g_TextSlots[slot].y = y - height / 2;
        if (g_TextSlots[slot].messageId != messageId) {
            g_TextSlots[slot].shownCount = 0;
        }
    }
    g_TextSlots[slot].messageId = messageId;
}

#else
INCLUDE_ASM("asm/nonmatchings/text", SetTextSlot);
#endif

INCLUDE_ASM("asm/nonmatchings/text", func_80071B34);

/* Shows the whole of a slot's message at once */
void RevealTextSlot(s32 slot) {
    g_TextSlots[slot].shownCount = 0xFF;
}

s32 IsTextSlotRevealed(s32 slot) {
    TextSlot *textSlot = &g_TextSlots[slot];

    /* The operand order is there to match */
    return !(textSlot->shownCount < (textSlot->messageId + g_Messages->messages)->length);
}

INCLUDE_ASM("asm/nonmatchings/text", func_80071CE4);

INCLUDE_ASM("asm/nonmatchings/text", func_80072A08);

INCLUDE_ASM("asm/nonmatchings/text", func_800734F0);

/*
 * Measures a string of character codes, at the font size. Only the large
 * font can be proportional. The two loops differ in form to match.
 */
s32 MeasureText(char *text, s32 fontSize, s32 flags) {
    s32 width = 0;
    s32 code;

    if (fontSize == FONT_SIZE_LARGE) {
        if (*text != '\0') {
            do {
            code = *text;
            if (code >= 'a' && code <= 'z') {
                code += LOWERCASE_CODE_OFFSET;
            }
            switch (code) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    break;
                default:
                    if (flags & MEASURE_TEXT_PROPORTIONAL) {
                        width += g_LargeFontWidths[code];
                    } else {
                        width += FONT_SIZE_LARGE;
                    }
                    break;
            }
                text++;
            } while (*text != '\0');
        }
    } else {
        for (;;) {
            if (*text == '\0') {
                break;
            }
            code = *text;
            if (code >= 'a' && code <= 'z') {
                code += LOWERCASE_CODE_OFFSET;
            }
            switch (code) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    break;
                default:
                    width += fontSize;
                    break;
            }
            text++;
        }
    }
    return width;
}

/*
 * Measures a message as a slot would show it: its widest line and the
 * height of its line breaks.
 */
void MeasureMessage(s32 messageId, s32 fontSize, s32 *width, s32 *height, s32 slot) {
    s32 lineWidths[MESSAGE_LINE_COUNT] = { 0, 0, 0 };
    s32 i;
    s32 lineCount;
    s32 line;
    s32 widest;
    s32 code;
    s32 length;
    s32 start;

    lineCount = 0;
    /* The operand order is there to match */
    length = (messageId + g_Messages->messages)->length;
    start = (messageId + g_Messages->messages)->start;
    line = 0;
    for (i = 0; i != length; i++) {
        code = ((u16 *)g_Messages)[g_MessageCodesStart + start + i];
        if (code < MESSAGE_CODE_GLYPH) {
            if (code < CHARACTER_CODE_MARK_FIRST || code >= CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                if (fontSize == FONT_SIZE_LARGE) {
                    lineWidths[line] += g_LargeFontWidths[code];
                } else {
                    lineWidths[line] += fontSize;
                }
            }
        } else if (code >= MESSAGE_CODE_CONTROL) {
            switch (code) {
                case MESSAGE_CODE_LINE_BREAK:
                    line++;
                    lineCount += LINE_SPACING + fontSize;
                    break;
                case MESSAGE_CODE_NAME:
                    sprintf(g_PrintBuffer, g_TextStringFormat, g_TextName);
                    lineWidths[line] += MeasureText(g_PrintBuffer, fontSize, MEASURE_TEXT_PROPORTIONAL);
                    break;
                case MESSAGE_CODE_VALUE:
                case MESSAGE_CODE_VALUE + 1:
                case MESSAGE_CODE_VALUE + 2:
                case MESSAGE_CODE_VALUE + 3:
                    sprintf(g_PrintBuffer, g_TextNumberFormat, g_TextSlots[slot].values[code - MESSAGE_CODE_VALUE]);
                    lineWidths[line] += GetTextLength(g_PrintBuffer) * fontSize;
                    break;
                case 0x8000:
                case 0x8001:
                case 0x8002:
                case 0x8003:
                case 0x8004:
                case 0x8005:
                case 0x8006:
                case 0x8007:
                case 0x8008:
                case 0x8009:
                case 0x800A:
                case 0x800C:
                case 0x800D:
                case 0x800E:
                    lineWidths[line] += ICON_WIDTH;
                    break;
            }
        }
    }
    widest = lineWidths[0];
    if (widest < lineWidths[1]) {
        widest = lineWidths[1];
    }
    *width = widest;
    if (widest < lineWidths[2]) {
        widest = lineWidths[2];
    }
    *width = widest;
    *height = lineCount;
}

s32 GetTextLength(char *text) {
    s32 length = 0;

    while (*text != '\0') {
        text++;
        length++;
    }
    return length;
}

INCLUDE_ASM("asm/nonmatchings/text", func_80073C14);

INCLUDE_ASM("asm/nonmatchings/text", DrawLargeText);

INCLUDE_ASM("asm/nonmatchings/text", DrawMediumText);

INCLUDE_ASM("asm/nonmatchings/text", DrawSmallText);

INCLUDE_ASM("asm/nonmatchings/text", DrawCenteredLargeText);

INCLUDE_ASM("asm/nonmatchings/text", DrawCenteredMediumText);

INCLUDE_ASM("asm/nonmatchings/text", DrawCenteredSmallText);

INCLUDE_ASM("asm/nonmatchings/text", DrawTextStub);

INCLUDE_ASM("asm/nonmatchings/text", BeginShadowedText);

INCLUDE_ASM("asm/nonmatchings/text", SetTextColor);

INCLUDE_ASM("asm/nonmatchings/text", DrawShadowedText);

INCLUDE_ASM("asm/nonmatchings/text", DrawCenteredShadowedText);

INCLUDE_ASM("asm/nonmatchings/text", DrawCenteredNarrowText);

INCLUDE_ASM("asm/nonmatchings/text", DrawCondensedShadowedText);

INCLUDE_ASM("asm/nonmatchings/text", ClearGlyphSprites);

INCLUDE_ASM("asm/nonmatchings/text", func_8007515C);

INCLUDE_ASM("asm/nonmatchings/text", func_800754BC);

INCLUDE_ASM("asm/nonmatchings/text", func_8007580C);

INCLUDE_ASM("asm/nonmatchings/text", func_80075E48);

INCLUDE_ASM("asm/nonmatchings/text", func_800760CC);

INCLUDE_ASM("asm/nonmatchings/text", func_80076138);

INCLUDE_ASM("asm/nonmatchings/text", func_8007624C);

