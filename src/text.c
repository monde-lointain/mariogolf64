#include "asset.h"
#include "common.h"
#include "functions.h"
#include "heap.h"
#include "libc.h"
#include "screen_layers.h"
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
#define TEXT_ASSET_SHADOWED_FONT 0x4B2
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
/* The width of an icon, in pixels; icons are square */
#define ICON_WIDTH 12
#define ICON_TEXEL_COUNT (ICON_WIDTH * ICON_WIDTH)
/* Given as the icon of a code that has none */
#define NO_ICON (-1)
/* The fonts' images have 16 glyphs to a row, and a glyph is loaded 16 texels wide */
#define FONT_GLYPHS_PER_ROW 16
#define GLYPH_TILE_WIDTH 16

#define FONT_SIZE_MEDIUM 10
#define FONT_SIZE_SMALL 8

/* How far the shadowed font advances for a glyph, and for a full stop */
#define SHADOWED_GLYPH_WIDTH 8
#define SHADOWED_POINT_WIDTH 2
/* How far a full stop of the shadowed font is drawn back over the glyph before it */
#define SHADOWED_POINT_INSET 3
#define CONDENSED_GLYPH_WIDTH 7
/*
 * The shadowed font's glyphs are 8 pixels square, 31 to a row of its
 * image, from the space on
 */
#define SHADOWED_GLYPH_SIZE 8
#define SHADOWED_GLYPHS_PER_ROW 31
#define SHADOWED_GLYPH_FIRST ' '
/* Its last row has narrower glyphs, from the apostrophe on */
#define NARROW_GLYPH_ROW 3
#define NARROW_GLYPH_FIRST '\''
#define NARROW_GLYPH_SIZE 7
#define NARROW_GLYPH_WIDTH 6

/* How far a text shadow is drawn from its text, and how much darker */
#define SHADOW_OFFSET 1
#define SHADOW_DIMMING_SHIFT 2

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

/* Where the glyph and message files are in the ROM */
extern u8 g_GlyphFileRomStart[];
extern u8 g_MessageFileRomStart[];
extern void *g_SmallFontImage;
extern void *g_MediumFontImage;
extern void *g_LargeFontImage;
extern void *g_TextIconImage;
extern void *g_ShadowedFontImage;
extern void *D_800E1C3C[2];
extern void *g_MessageGlyphs;
extern MessageTable *g_Messages;
/* The index of the first character code in the message table, in halfwords */
extern s32 g_MessageCodesStart;
extern TextSlot g_TextSlots[TEXT_SLOT_COUNT];
extern u8 g_TextPalette[TEXT_PALETTE_COLOR_COUNT][3];
extern char g_TextName[TEXT_NAME_SIZE];
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
#define NO_GLYPH_GROUP (-1)
/* Given this as its font size, PlaceGlyphGroup only frees the group */
#define NO_FONT_SIZE (-1)
/* Without this flag, PlaceGlyphGroup centres the text on its x */
#define GLYPH_FLAG_LEFT_ALIGNED 0x10
/* Only the top or the bottom part of a large glyph is drawn */
#define GLYPH_FLAG_TOP_PART 0x01
#define GLYPH_FLAG_BOTTOM_PART 0x02
/* The texel row where a large glyph's bottom part starts */
#define LARGE_GLYPH_PART_ROW 7
/*
 * How far a glyph sprite of a mark is moved back over the glyph before it,
 * and up, in each font. Sprites are placed with y rising up the screen.
 */
#define LARGE_MARK_BACKSPACE 6.0f
#define LARGE_MARK_RISE 8.0f
#define MEDIUM_MARK_BACKSPACE 8.0f
#define MEDIUM_MARK_RISE 7.0f
#define SMALL_MARK_RISE 8.0f
/* PlaceMessageGlyphGroup moves marks back as in the large font, but less far up */
#define MESSAGE_MARK_RISE 4.0f
/* How far back and up the text drawing routines draw a mark, where y points down */
#define LARGE_TEXT_MARK_BACKSPACE 6
#define LARGE_TEXT_MARK_RISE 4
#define MEDIUM_TEXT_MARK_BACKSPACE 8
#define MEDIUM_TEXT_MARK_RISE 7
#define SMALL_TEXT_MARK_BACKSPACE 6
#define SMALL_TEXT_MARK_RISE 6
/* How far a full stop is drawn back, and how far it advances */
#define LARGE_POINT_INSET 6
#define LARGE_POINT_WIDTH 3
#define MEDIUM_POINT_INSET 5
#define MEDIUM_POINT_WIDTH 2
/* The small font draws this character code with the glyph at another code */
#define SMALL_FONT_MOVED_CODE 0xB0
#define SMALL_FONT_MOVED_GLYPH 0x70

extern GlyphSprite g_GlyphSprites[GLYPH_SPRITE_COUNT];
/* The quads glyph sprites are drawn with, centred on the sprite */
extern Vtx sLargeGlyphVertices[4];
extern Vtx sLargeGlyphTopVertices[4];
extern Vtx sLargeGlyphBottomVertices[4];
extern Vtx sMediumGlyphVertices[4];
extern Vtx sSmallGlyphVertices[4];
/* How many glyphs each group has */
extern u8 g_GlyphGroupSizes[];
/* The width of each character of the large font */
extern u8 g_LargeFontWidths[];

void func_80071CE4(Gfx **displayListHead, s32 slot);
void func_80072A08(Gfx **displayListHead, s32 slot);
void ClearGlyphSprites(void);
void DrawLargeText(Gfx **displayListHead, s16 x, s16 y, char *text);
void DrawFixedWidthLargeText(Gfx **displayListHead, s16 x, s16 y, char *text);
void DrawCenteredLargeText(Gfx **displayListHead, s16 x, s16 y, char *text);
void DrawMediumText(Gfx **displayListHead, s16 x, s16 y, char *text);
void DrawSmallText(Gfx **displayListHead, s16 x, s16 y, char *text);
void DrawShadowedText(Gfx **displayListHead, s16 x, s16 y, char *text);
s32 GetTextLength(char *text);

const char g_TextStringFormat[] = "%s";
const char g_TextNumberFormat[] = "%d";
const char g_MatchScoreEvenText[] = "EVEN";
const char g_MatchScoreUpFormat[] = "%dUP";
const char g_MatchScoreDownFormat[] = "%dDOWN";
const char g_SkinsScoreFormat[] = "%dP";
const char g_ParScoreEvenText[] = "#0";
const char g_ParScoreFormat[] = "%+d";

/* Where a character's glyph is in the shadowed font's image, in pixels */
static inline s16 GetShadowedGlyphLeft(s32 code) {
    return (code - SHADOWED_GLYPH_FIRST) % SHADOWED_GLYPHS_PER_ROW * SHADOWED_GLYPH_SIZE;
}

static inline s16 GetShadowedGlyphTop(s32 code) {
    return (code - SHADOWED_GLYPH_FIRST) / SHADOWED_GLYPHS_PER_ROW * SHADOWED_GLYPH_SIZE;
}

/*
 * Loads the fonts, the icons and the message table, and clears the text
 * slots.
 */
void LoadTextAssets(void) {
    EncodedAsset asset;
    u8 *header;
    s32 size;

    g_ShadowedFontImage = AllocMemory(OpenEncodedAsset(TEXT_ASSET_SHADOWED_FONT, &asset));
    ReadEncodedAsset(g_ShadowedFontImage, &asset);
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
    FreeMemory(&g_ShadowedFontImage);
    FreeMemory(&g_SmallFontImage);
    FreeMemory(&g_MediumFontImage);
    FreeMemory(&g_LargeFontImage);
    FreeMemory(&g_TextIconImage);
    FreeMemory(&g_MessageGlyphs);
    FreeMemory(&g_Messages);
}

/*
 * Sets up the RDP to draw translucent text in the primitive colour, with
 * the glyphs' alpha scaled by the primitive alpha
 */
void SetUpTextDrawing(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gSPLoadGeometryMode(gfx++, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetBlendColor(gfx++, 0, 0, 0, 1);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPSetEnvColor(gfx++, 128, 128, 255, 255);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0,
                      0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

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

/*
 * Draws the message of every text slot that has one, in the slot's colour,
 * in the medium font or the font of the slot's size
 */
void DrawTextSlots(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 slot;

    SetUpTextDrawing(&gfx);
    for (slot = 0; slot != TEXT_SLOT_COUNT; slot++) {
        if (g_TextSlots[slot].messageId != NO_MESSAGE && !(g_TextSlots[slot].flags & TEXT_FLAG_UNK20)) {
            gDPSetPrimColor(gfx++, 0, 0, g_TextSlots[slot].red, g_TextSlots[slot].green, g_TextSlots[slot].blue,
                            g_TextSlots[slot].alpha);
            if (g_TextSlots[slot].fontSize == FONT_SIZE_MEDIUM) {
                func_80072A08(&gfx, slot);
            } else {
                func_80071CE4(&gfx, slot);
            }
        }
    }
    *displayListHead = gfx;
}

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

/*
 * Draws text in the large font, each glyph advancing by the font size but
 * a full stop, which is drawn back and advances less. A mark is drawn over
 * the glyph before it.
 */
void DrawFixedWidthLargeText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 code;
    s32 character;
    s32 left;
    s32 top;
    s32 glyphLeft;
    s32 glyphTop;

    gDPPipeSync(gfx++);
    while (*text != '\0') {
        character = *text;
        if (character == ' ') {
            text++;
            x += FONT_SIZE_LARGE;
        } else {
            left = x;
            top = y;
            code = character;
            if (code >= 'a' && code <= 'z') {
                code += LOWERCASE_CODE_OFFSET;
            }
            switch (code) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    left -= LARGE_TEXT_MARK_BACKSPACE;
                    top -= LARGE_TEXT_MARK_RISE;
                    x -= FONT_SIZE_LARGE;
                    break;
                case '.':
                    left -= LARGE_POINT_INSET;
                    x -= FONT_SIZE_LARGE - LARGE_POINT_WIDTH;
                    break;
            }
            glyphLeft = code % FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
            glyphTop = code / FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
            gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                               ((u32)g_LargeFontImage + IMAGE_HEADER_SIZE) & ~7);
            gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPLoadSync(gfx++);
            gDPLoadTile(gfx++, G_TX_LOADTILE, glyphLeft << 1, glyphTop << 2, (glyphLeft + GLYPH_TILE_WIDTH - 1) << 1,
                        (glyphTop + FONT_SIZE_LARGE - 1) << 2);
            gDPPipeSync(gfx++);
            gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                       G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPSetTileSize(gfx++, G_TX_RENDERTILE, glyphLeft << 2, glyphTop << 2,
                           (glyphLeft + GLYPH_TILE_WIDTH - 1) << 2, (glyphTop + FONT_SIZE_LARGE - 1) << 2);
            gDPSetTileSize(gfx++, 1, glyphLeft << 2, glyphTop << 2, (glyphLeft + GLYPH_TILE_WIDTH) << 2,
                           (glyphTop + FONT_SIZE_LARGE) << 2);
            gDPPipeSync(gfx++);
            gSPTextureRectangle(gfx++, left << 2, top << 2, (left + FONT_SIZE_LARGE) << 2, (top + FONT_SIZE_LARGE) << 2,
                                G_TX_RENDERTILE, glyphLeft << 5, glyphTop << 5, 1 << 10, 1 << 10);
            x += FONT_SIZE_LARGE;
            text++;
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws text in the large font, each glyph advancing by its own width. A
 * mark is drawn over the glyph before it.
 */
void DrawLargeText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 code;
    s32 character;
    s32 left;
    s32 top;
    s32 glyphLeft;
    s32 glyphTop;

    gDPPipeSync(gfx++);
    while (*text != '\0') {
        character = *text;
        if (character == ' ') {
            text++;
            x += g_LargeFontWidths[' '];
        } else {
            left = x;
            top = y;
            code = character;
            if (code >= 'a' && code <= 'z') {
                code += LOWERCASE_CODE_OFFSET;
            }
            switch (code) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    left -= LARGE_TEXT_MARK_BACKSPACE;
                    top -= LARGE_TEXT_MARK_RISE;
                    x -= FONT_SIZE_LARGE;
                    break;
            }
            glyphLeft = code % FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
            glyphTop = code / FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
            gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                               ((u32)g_LargeFontImage + IMAGE_HEADER_SIZE) & ~7);
            gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPLoadSync(gfx++);
            gDPLoadTile(gfx++, G_TX_LOADTILE, glyphLeft << 1, glyphTop << 2, (glyphLeft + GLYPH_TILE_WIDTH - 1) << 1,
                        (glyphTop + FONT_SIZE_LARGE - 1) << 2);
            gDPPipeSync(gfx++);
            gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                       G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPSetTileSize(gfx++, G_TX_RENDERTILE, glyphLeft << 2, glyphTop << 2,
                           (glyphLeft + GLYPH_TILE_WIDTH - 1) << 2, (glyphTop + FONT_SIZE_LARGE - 1) << 2);
            gDPSetTileSize(gfx++, 1, glyphLeft << 2, glyphTop << 2, (glyphLeft + GLYPH_TILE_WIDTH) << 2,
                           (glyphTop + FONT_SIZE_LARGE) << 2);
            gDPPipeSync(gfx++);
            gSPTextureRectangle(gfx++, left << 2, top << 2, (left + FONT_SIZE_LARGE) << 2, (top + FONT_SIZE_LARGE) << 2,
                                G_TX_RENDERTILE, glyphLeft << 5, glyphTop << 5, 1 << 10, 1 << 10);
            text++;
            x += g_LargeFontWidths[code];
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws text in the medium font, each glyph advancing by the font size
 * but a full stop, which is drawn back and advances less. A mark is drawn
 * over the glyph before it.
 */
void DrawMediumText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 character;
    s32 code;
    s32 left;
    s32 top;
    s32 glyphLeft;
    s32 glyphTop;

    gDPPipeSync(gfx++);
    while (*text != '\0') {
        character = *text;
        if (character == ' ') {
            text++;
            x += FONT_SIZE_MEDIUM;
        } else {
            left = x;
            top = y;
            code = character;
            if (code >= 'a' && code <= 'z') {
                code += LOWERCASE_CODE_OFFSET;
            }
            switch (code) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    left -= MEDIUM_TEXT_MARK_BACKSPACE;
                    top -= MEDIUM_TEXT_MARK_RISE;
                    x -= FONT_SIZE_MEDIUM;
                    break;
                case '.':
                    left -= MEDIUM_POINT_INSET;
                    x -= FONT_SIZE_MEDIUM - MEDIUM_POINT_WIDTH;
                    break;
            }
            glyphLeft = code % FONT_GLYPHS_PER_ROW * FONT_SIZE_MEDIUM;
            glyphTop = code / FONT_GLYPHS_PER_ROW * FONT_SIZE_MEDIUM;
            gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, FONT_SIZE_MEDIUM * FONT_GLYPHS_PER_ROW / 2,
                               ((u32)g_MediumFontImage + IMAGE_HEADER_SIZE) & ~7);
            gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPLoadSync(gfx++);
            gDPLoadTile(gfx++, G_TX_LOADTILE, glyphLeft << 1, glyphTop << 2, (glyphLeft + GLYPH_TILE_WIDTH - 1) << 1,
                        (glyphTop + FONT_SIZE_MEDIUM - 1) << 2);
            gDPPipeSync(gfx++);
            gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                       G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPSetTileSize(gfx++, G_TX_RENDERTILE, glyphLeft << 2, glyphTop << 2,
                           (glyphLeft + GLYPH_TILE_WIDTH - 1) << 2, (glyphTop + FONT_SIZE_MEDIUM - 1) << 2);
            gDPPipeSync(gfx++);
            gSPTextureRectangle(gfx++, left << 2, top << 2, (left + FONT_SIZE_MEDIUM) << 2,
                                (top + FONT_SIZE_MEDIUM) << 2, G_TX_RENDERTILE, glyphLeft << 5, glyphTop << 5, 1 << 10,
                                1 << 10);
            text++;
            x += FONT_SIZE_MEDIUM;
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws text in the small font, each glyph advancing by the font size. A
 * mark is drawn over the glyph before it.
 */
void DrawSmallText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 character;
    s32 code;
    s32 left;
    s32 top;
    s32 glyphLeft;
    s32 glyphTop;

    gDPPipeSync(gfx++);
    while (*text != '\0') {
        character = *text;
        if (character == ' ') {
            text++;
            x += FONT_SIZE_SMALL;
        } else {
            code = character;
            left = x;
            top = y;
            if (code >= 'a' && code <= 'z') {
                code += LOWERCASE_CODE_OFFSET;
            }
            switch (code) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    left -= SMALL_TEXT_MARK_BACKSPACE;
                    top -= SMALL_TEXT_MARK_RISE;
                    x -= FONT_SIZE_SMALL;
                    break;
                case SMALL_FONT_MOVED_CODE:
                    code = SMALL_FONT_MOVED_GLYPH;
                    break;
            }
            glyphLeft = code % FONT_GLYPHS_PER_ROW * FONT_SIZE_SMALL;
            glyphTop = code / FONT_GLYPHS_PER_ROW * FONT_SIZE_SMALL;
            gDPSetTextureImage(gfx++, G_IM_FMT_IA, G_IM_SIZ_8b, FONT_SIZE_SMALL * FONT_GLYPHS_PER_ROW / 2,
                               ((u32)g_SmallFontImage + IMAGE_HEADER_SIZE) & ~7);
            gDPSetTile(gfx++, G_IM_FMT_IA, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPLoadSync(gfx++);
            gDPLoadTile(gfx++, G_TX_LOADTILE, glyphLeft << 1, glyphTop << 2, (glyphLeft + FONT_SIZE_SMALL - 1) << 1,
                        (glyphTop + FONT_SIZE_SMALL - 1) << 2);
            gDPPipeSync(gfx++);
            gDPSetTile(gfx++, G_IM_FMT_IA, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                       G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
            gDPSetTileSize(gfx++, G_TX_RENDERTILE, glyphLeft << 2, glyphTop << 2,
                           (glyphLeft + FONT_SIZE_SMALL - 1) << 2, (glyphTop + FONT_SIZE_SMALL - 1) << 2);
            gDPPipeSync(gfx++);
            gSPTextureRectangle(gfx++, left << 2, top << 2, (left + FONT_SIZE_SMALL) << 2, (top + FONT_SIZE_SMALL) << 2,
                                G_TX_RENDERTILE, glyphLeft << 5, glyphTop << 5, 1 << 10, 1 << 10);
            text++;
            x += FONT_SIZE_SMALL;
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Draws text in the large font, centred on x */
void DrawCenteredLargeText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;

    DrawLargeText(&gfx, x - MeasureText(text, FONT_SIZE_LARGE, MEASURE_TEXT_PROPORTIONAL) / 2, y, text);
    *displayListHead = gfx;
}

/* Draws text in the medium font, centred on x */
void DrawCenteredMediumText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;

    DrawMediumText(&gfx, x - MeasureText(text, FONT_SIZE_MEDIUM, MEASURE_TEXT_PROPORTIONAL) / 2, y, text);
    *displayListHead = gfx;
}

/* Draws text in the small font, centred on x */
void DrawCenteredSmallText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;

    DrawSmallText(&gfx, x - MeasureText(text, FONT_SIZE_SMALL, MEASURE_TEXT_PROPORTIONAL) / 2, y, text);
    *displayListHead = gfx;
}

/*
 * Draws nothing. Its callers pass it a display list, a position and text,
 * as they would to the other text drawing routines.
 */
void DrawTextStub(Gfx **displayListHead, s16 x, s16 y, char *text) {
}

INCLUDE_ASM("asm/nonmatchings/text", BeginShadowedText);

/* Sets the colour, fully opaque, of the text drawn after it */
void SetTextColor(Gfx **displayListHead, s32 red, s32 green, s32 blue) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 255);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws text in the shadowed font, which BeginShadowedText loads. A full
 * stop is drawn narrower, over the end of the glyph before it.
 */
void DrawShadowedText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 code;
    s16 glyphLeft;
    s16 glyphTop;

    gDPPipeSync(gfx++);
    while (*text != '\0') {
        code = *text;
        if (code == '.') {
            x -= SHADOWED_POINT_INSET;
        }
        glyphLeft = GetShadowedGlyphLeft(code);
        glyphTop = GetShadowedGlyphTop(code);
        gSPTextureRectangle(gfx++, x << 2, y << 2, (x + SHADOWED_GLYPH_SIZE) << 2, (y + SHADOWED_GLYPH_SIZE) << 2,
                            G_TX_RENDERTILE, glyphLeft << 5, glyphTop << 5, 1 << 10, 1 << 10);
        if (*text == '.') {
            x += SHADOWED_POINT_INSET + SHADOWED_POINT_WIDTH;
        } else {
            x += SHADOWED_GLYPH_WIDTH;
        }
        text++;
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Draws text in the shadowed font, centred on x */
#ifdef NON_MATCHING
void DrawCenteredShadowedText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s16 right = x;
    char *character;

    for (character = text; *character != '\0'; character++) {
        if (*character == '.') {
            right += SHADOWED_POINT_WIDTH;
        } else {
            right += SHADOWED_GLYPH_WIDTH;
        }
    }
    DrawShadowedText(&gfx, x - (right - x) / 2, y, text);
    *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/text", DrawCenteredShadowedText);
#endif

/*
 * Draws text centred on x in the narrow glyphs of the shadowed font's last
 * row, which start at the apostrophe
 */
#ifdef NON_MATCHING
void DrawCenteredNarrowText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 length;
    char *character;
    s32 code;
    s16 glyphLeft;

    gDPPipeSync(gfx++);
    length = 0;
    for (character = text; *character != '\0'; character++) {
        length++;
    }
    x -= length * NARROW_GLYPH_WIDTH / 2;
    while ((code = *text) != '\0') {
        glyphLeft = (code - NARROW_GLYPH_FIRST) * SHADOWED_GLYPH_SIZE;
        gSPTextureRectangle(gfx++, x << 2, y << 2, (x + NARROW_GLYPH_SIZE) << 2, (y + SHADOWED_GLYPH_SIZE) << 2,
                            G_TX_RENDERTILE, glyphLeft << 5, (NARROW_GLYPH_ROW * SHADOWED_GLYPH_SIZE) << 5, 1 << 10,
                            1 << 10);
        text++;
        x += NARROW_GLYPH_WIDTH;
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/text", DrawCenteredNarrowText);
#endif

/* Draws text in the shadowed font with its glyphs closer together */
void DrawCondensedShadowedText(Gfx **displayListHead, s16 x, s16 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 code;
    s16 glyphLeft;
    s16 glyphTop;

    gDPPipeSync(gfx++);
    while ((code = *text) != '\0') {
        glyphLeft = GetShadowedGlyphLeft(code);
        glyphTop = GetShadowedGlyphTop(code);
        gSPTextureRectangle(gfx++, x << 2, y << 2, (x + SHADOWED_GLYPH_SIZE) << 2, (y + SHADOWED_GLYPH_SIZE) << 2,
                            G_TX_RENDERTILE, glyphLeft << 5, glyphTop << 5, 1 << 10, 1 << 10);
        text++;
        x += CONDENSED_GLYPH_WIDTH;
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Frees every glyph sprite */
void ClearGlyphSprites(void) {
    s32 i;

    for (i = 0; i != GLYPH_SPRITE_COUNT; i++) {
        g_GlyphSprites[i].group = NO_GLYPH_GROUP;
    }
}

/*
 * Places the glyphs of text as sprites of a group, replacing the group's
 * old sprites, or only frees them if the font size is NO_FONT_SIZE. Marks
 * are placed over the glyph before them. Without GLYPH_FLAG_LEFT_ALIGNED
 * the text is centred on x.
 */
#ifdef NON_MATCHING
void PlaceGlyphGroup(s32 group, s32 fontSize, s32 flags, s32 x, s32 y, char *text) {
    s32 i;
    s32 halfWidth;
    s32 advance;
    s32 placedCount;

    if (fontSize == NO_FONT_SIZE) {
        for (i = 0; i != GLYPH_SPRITE_COUNT; i++) {
            if (g_GlyphSprites[i].group == group) {
                g_GlyphSprites[i].group = NO_GLYPH_GROUP;
            }
        }
        return;
    }
    halfWidth = 0;
    if (!(flags & GLYPH_FLAG_LEFT_ALIGNED)) {
        if (flags & TEXT_FLAG_FIXED_WIDTH) {
            halfWidth = MeasureText(text, fontSize, MEASURE_TEXT_FIXED_WIDTH) / 2;
        } else {
            halfWidth = MeasureText(text, fontSize, MEASURE_TEXT_PROPORTIONAL) / 2;
        }
    }
    g_GlyphGroupSizes[group] = GetTextLength(text);
    for (i = 0; i != GLYPH_SPRITE_COUNT; i++) {
        if (g_GlyphSprites[i].group == group) {
            g_GlyphSprites[i].group = NO_GLYPH_GROUP;
        }
    }
    advance = 0;
    placedCount = 0;
    for (i = 0; placedCount != g_GlyphGroupSizes[group] && i != GLYPH_SPRITE_COUNT; i++) {
        if (g_GlyphSprites[i].group == NO_GLYPH_GROUP) {
            switch (*text) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    if (fontSize == FONT_SIZE_LARGE) {
                        guTranslate(&g_GlyphSprites[i].matrix, advance - halfWidth + x - LARGE_MARK_BACKSPACE,
                                    y + LARGE_MARK_RISE, 0);
                    } else if (fontSize == FONT_SIZE_MEDIUM) {
                        guTranslate(&g_GlyphSprites[i].matrix, advance - halfWidth + x - MEDIUM_MARK_BACKSPACE,
                                    y + MEDIUM_MARK_RISE, 0);
                    } else {
                        guTranslate(&g_GlyphSprites[i].matrix, advance - halfWidth + x, y + SMALL_MARK_RISE, 0);
                    }
                    break;
                default:
                    guTranslate(&g_GlyphSprites[i].matrix, advance - halfWidth + x, y, 0);
                    if (fontSize == FONT_SIZE_LARGE) {
                        if (flags & TEXT_FLAG_FIXED_WIDTH) {
                            advance += FONT_SIZE_LARGE;
                        } else {
                            advance += g_LargeFontWidths[*text];
                        }
                    } else {
                        advance += fontSize;
                    }
                    break;
            }
            g_GlyphSprites[i].code = *text;
            placedCount++;
            g_GlyphSprites[i].fontSize = fontSize;
            text++;
            g_GlyphSprites[i].flags = flags;
            g_GlyphSprites[i].group = group;
        }
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/text", PlaceGlyphGroup);
#endif

/*
 * Places the glyphs and icons of a message as sprites of a group, centred
 * on x and y, replacing the group's old sprites. Every glyph advances by
 * its width in the large font. Returns half the message's width in the
 * high halfword and half its height in the low one.
 */
s32 PlaceMessageGlyphGroup(s32 group, s32 fontSize, s32 x, s32 y, s32 messageId) {
    s32 width;
    s32 height;
    s32 i;
    s32 start;
    s32 length;
    s32 codeIndex;
    s32 advance;
    s32 code;

    if (fontSize == NO_FONT_SIZE) {
        for (i = 0; i != GLYPH_SPRITE_COUNT; i++) {
            if (g_GlyphSprites[i].group == group) {
                g_GlyphSprites[i].group = NO_GLYPH_GROUP;
            }
        }
        /* It returns no value here; the plain return is there to match */
        return;
    }
    /* The operand order is there to match */
    start = (messageId + g_Messages->messages)->start;
    length = (messageId + g_Messages->messages)->length;
    MeasureMessage(messageId, fontSize, &width, &height, 0);
    width /= 2;
    height /= 2;
    y += height;
    g_GlyphGroupSizes[group] = length;
    for (i = 0; i != GLYPH_SPRITE_COUNT; i++) {
        if (g_GlyphSprites[i].group == group) {
            g_GlyphSprites[i].group = NO_GLYPH_GROUP;
        }
    }
    codeIndex = 0;
    advance = 0;
    for (i = 0; codeIndex != length; i++) {
        if (g_GlyphSprites[i].group == NO_GLYPH_GROUP) {
            code = ((u16 *)g_Messages)[g_MessageCodesStart + start + codeIndex];
            if (code < MESSAGE_CODE_GLYPH) {
                if (code >= CHARACTER_CODE_MARK_FIRST && code < CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                    guTranslate(&g_GlyphSprites[i].matrix, advance - width + x - LARGE_MARK_BACKSPACE,
                                y + MESSAGE_MARK_RISE, 0);
                } else {
                    guTranslate(&g_GlyphSprites[i].matrix, advance - width + x, y, 0);
                    advance += g_LargeFontWidths[code];
                }
                g_GlyphSprites[i].code = code;
                g_GlyphSprites[i].fontSize = fontSize;
                g_GlyphSprites[i].group = group;
            } else if (code >= MESSAGE_CODE_CONTROL) {
                switch (code) {
                    case MESSAGE_CODE_LINE_BREAK:
                        advance = 0;
                        y -= LINE_SPACING + fontSize;
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
                        guTranslate(&g_GlyphSprites[i].matrix, advance - width + x, y, 0);
                        advance += fontSize;
                        g_GlyphSprites[i].code = code;
                        g_GlyphSprites[i].fontSize = fontSize;
                        g_GlyphSprites[i].group = group;
                        break;
                }
            }
            codeIndex++;
        }
    }
    return (width << 16) | height;
}

/*
 * Packs texel coordinates into a vertex's texture coordinates. It is a
 * macro, as an inline function compiles differently from the original.
 */
#define PackTextureCoords(s, t) (((s) << 21) | ((t) << 5))

/*
 * Draws the sprites of a group of glyphs. The fonts are four-bit images
 * with 16 glyphs to a row, loaded a glyph at a time as eight-bit texels;
 * icons are eight-bit images that share one palette.
 */
void DrawGlyphGroup(Gfx **displayListHead, s32 group) {
    Gfx *gfx = *displayListHead;
    s32 i;
    s32 isIconLoaded;
    s32 iconOffset;
    u32 code;
    s32 fontSize;
    s32 s;
    s32 t;
    s32 left;
    s32 top;
    s32 bottom;

    isIconLoaded = FALSE;
    gDPPipeSync(gfx++);
    for (i = 0; i != GLYPH_SPRITE_COUNT; i++) {
        if (g_GlyphSprites[i].group == group) {
            iconOffset = NO_ICON;
            if (isIconLoaded == TRUE) {
                gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE,
                                  0, TEXEL0, 0);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                isIconLoaded = FALSE;
            }
            code = g_GlyphSprites[i].code;
            if (code < MESSAGE_CODE_GLYPH) {
                s = (code % FONT_GLYPHS_PER_ROW) * g_GlyphSprites[i].fontSize;
                t = (code / FONT_GLYPHS_PER_ROW) * g_GlyphSprites[i].fontSize;
                gDPPipeSync(gfx++);
                if (g_GlyphSprites[i].fontSize == FONT_SIZE_LARGE) {
                    gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                                       ((u32)g_LargeFontImage + IMAGE_HEADER_SIZE) & ~7);
                } else if (g_GlyphSprites[i].fontSize == FONT_SIZE_MEDIUM) {
                    gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, FONT_SIZE_MEDIUM * FONT_GLYPHS_PER_ROW / 2,
                                       ((u32)g_MediumFontImage + IMAGE_HEADER_SIZE) & ~7);
                } else {
                    gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, FONT_SIZE_SMALL * FONT_GLYPHS_PER_ROW / 2,
                                       ((u32)g_SmallFontImage + IMAGE_HEADER_SIZE) & ~7);
                }
                gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPLoadSync(gfx++);
                gDPLoadTile(gfx++, G_TX_LOADTILE, s << 1, t << 2, (s + GLYPH_TILE_WIDTH - 1) << 1,
                            (t + FONT_SIZE_LARGE - 1) << 2);
                gDPPipeSync(gfx++);
                gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPSetTileSize(gfx++, G_TX_RENDERTILE, s << 2, t << 2, (s + GLYPH_TILE_WIDTH - 1) << 2,
                               (t + GLYPH_TILE_WIDTH - 1) << 2);
            } else if (code >= MESSAGE_CODE_CONTROL) {
                switch (code) {
                    case 0x8000:
                        iconOffset = 0 * ICON_TEXEL_COUNT;
                        break;
                    case 0x8001:
                        iconOffset = 1 * ICON_TEXEL_COUNT;
                        break;
                    case 0x8002:
                    case 0x8003:
                    case 0x8004:
                    case 0x8005:
                    case 0x8006:
                        iconOffset = 3 * ICON_TEXEL_COUNT;
                        break;
                    case 0x8007:
                        iconOffset = 6 * ICON_TEXEL_COUNT;
                        break;
                    case 0x8008:
                        iconOffset = 7 * ICON_TEXEL_COUNT;
                        break;
                    case 0x8009:
                        iconOffset = 8 * ICON_TEXEL_COUNT;
                        break;
                    case 0x800A:
                        iconOffset = 9 * ICON_TEXEL_COUNT;
                        break;
                    case 0x800C:
                        iconOffset = 2 * ICON_TEXEL_COUNT;
                        break;
                    case 0x800D:
                        iconOffset = 4 * ICON_TEXEL_COUNT;
                        break;
                    case 0x800E:
                        iconOffset = 5 * ICON_TEXEL_COUNT;
                        break;
                }
                if (isIconLoaded == FALSE) {
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    gDPLoadTLUT_pal256(gfx++, ((u32)g_TextIconImage + IMAGE_HEADER_SIZE) & ~7);
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    isIconLoaded = TRUE;
                }
                gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, ICON_WIDTH,
                                   ((u32)g_TextIconImage + LARGE_PALETTED_TEXELS_OFFSET + iconOffset) & ~7);
                gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPLoadSync(gfx++);
                gDPLoadTile(gfx++, G_TX_LOADTILE, 0, 0, (ICON_WIDTH - 1) << 2, (ICON_WIDTH - 1) << 2);
                gDPPipeSync(gfx++);
                gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, G_TX_RENDERTILE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
                gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, (ICON_WIDTH - 1) << 2, (ICON_WIDTH - 1) << 2);
                s = 0;
                t = 0;
            }
            gDPPipeSync(gfx++);
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_GlyphSprites[i].matrix), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            fontSize = g_GlyphSprites[i].fontSize;
            top = 0;
            left = 0;
            bottom = fontSize;
            if (fontSize == FONT_SIZE_LARGE) {
                if (g_GlyphSprites[i].flags & GLYPH_FLAG_TOP_PART) {
                    gSPVertex(gfx++, sLargeGlyphTopVertices, 4, 0);
                    bottom = LARGE_GLYPH_PART_ROW;
                } else if (g_GlyphSprites[i].flags & GLYPH_FLAG_BOTTOM_PART) {
                    gSPVertex(gfx++, sLargeGlyphBottomVertices, 4, 0);
                    bottom = g_GlyphSprites[i].fontSize;
                    top = LARGE_GLYPH_PART_ROW;
                } else {
                    gSPVertex(gfx++, sLargeGlyphVertices, 4, 0);
                }
            } else if (fontSize == FONT_SIZE_MEDIUM) {
                gSPVertex(gfx++, sMediumGlyphVertices, 4, 0);
            } else {
                gSPVertex(gfx++, sSmallGlyphVertices, 4, 0);
            }
            gSPModifyVertex(gfx++, 0, G_MWO_POINT_ST, PackTextureCoords(left + s, top + t));
            gSPModifyVertex(gfx++, 1, G_MWO_POINT_ST, PackTextureCoords(left + s, bottom + t));
            gSPModifyVertex(gfx++, 2, G_MWO_POINT_ST, PackTextureCoords(fontSize + s, bottom + t));
            gSPModifyVertex(gfx++, 3, G_MWO_POINT_ST, PackTextureCoords(fontSize + s, top + t));
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gDPPipeSync(gfx++);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Sets up the RDP to draw translucent text in the primitive colour, with
 * perspective-correct textures
 */
void SetUpPerspectiveTextDrawing(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetBlendColor(gfx++, 0, 0, 0, 1);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Where a character's glyph is in the digit font's image, in pixels from the top */
s32 GetDigitGlyphTop(char code) {
    switch (code) {
        case ':':
            return DIGIT_GLYPH_COLON * DIGIT_GLYPH_SIZE;
        case '*':
            return DIGIT_GLYPH_STAR * DIGIT_GLYPH_SIZE;
        case '+':
            return DIGIT_GLYPH_PLUS * DIGIT_GLYPH_SIZE;
        case '-':
            return DIGIT_GLYPH_MINUS * DIGIT_GLYPH_SIZE;
        case '#':
            return DIGIT_GLYPH_HASH * DIGIT_GLYPH_SIZE;
        case '!':
            return DIGIT_GLYPH_EXCLAMATION * DIGIT_GLYPH_SIZE;
        default:
            return (code - '0' + DIGIT_GLYPH_ZERO) * DIGIT_GLYPH_SIZE;
    }
}

/* Unused; it is there to match */
const s32 g_UnusedTextData[2] = { 0, 0 };

/* Draws text in the digit font, as long as g_PrintBuffer holds a string at least as long */
void DrawDigitText(Gfx **displayListHead, s32 x, s32 y, char *text) {
    Gfx *gfx = *displayListHead;
    s32 index;

    gDPPipeSync(gfx++);
    for (index = 0; g_PrintBuffer[index] != '\0'; index++) {
        DrawTextureRect(&gfx, x << 2, y << 2, (x + DIGIT_GLYPH_SIZE) << 2, (y + DIGIT_GLYPH_SIZE) << 2,
                        G_TX_RENDERTILE, 0, GetDigitGlyphTop(text[index]) << 5, 1 << 10, 1 << 10);
        x += DIGIT_GLYPH_WIDTH;
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws text in the large font over a shadow a quarter as bright, one pixel
 * down and to the right. The colours are packed as RGBA, but the alpha is
 * ignored.
 */
void DrawLargeTextWithShadow(Gfx **displayListHead, s32 x, s32 y, char *text, s32 envColor, s32 primColor, s32 flags) {
    Gfx *gfx = *displayListHead;
    u8 env[3];
    u8 prim[3];

    env[0] = envColor >> 24;
    env[1] = envColor >> 16;
    env[2] = envColor >> 8;
    prim[0] = primColor >> 24;
    prim[1] = primColor >> 16;
    prim[2] = primColor >> 8;
    gDPPipeSync(gfx++);
    gDPSetEnvColor(gfx++, env[0] >> SHADOW_DIMMING_SHIFT, env[1] >> SHADOW_DIMMING_SHIFT,
                   env[2] >> SHADOW_DIMMING_SHIFT, 255);
    gDPSetPrimColor(gfx++, 0, 0, prim[0] >> SHADOW_DIMMING_SHIFT, prim[1] >> SHADOW_DIMMING_SHIFT,
                    prim[2] >> SHADOW_DIMMING_SHIFT, 255);
    if (flags & TEXT_FLAG_CENTERED) {
        DrawCenteredLargeText(&gfx, x + SHADOW_OFFSET, y + SHADOW_OFFSET, text);
    } else if (flags & TEXT_FLAG_FIXED_WIDTH) {
        DrawFixedWidthLargeText(&gfx, x + SHADOW_OFFSET, y + SHADOW_OFFSET, text);
    } else {
        DrawLargeText(&gfx, x + SHADOW_OFFSET, y + SHADOW_OFFSET, text);
    }
    gDPSetEnvColor(gfx++, env[0], env[1], env[2], 255);
    gDPSetPrimColor(gfx++, 0, 0, prim[0], prim[1], prim[2], 255);
    if (flags & TEXT_FLAG_CENTERED) {
        DrawCenteredLargeText(&gfx, x, y, text);
    } else if (flags & TEXT_FLAG_FIXED_WIDTH) {
        DrawFixedWidthLargeText(&gfx, x, y, text);
    } else {
        DrawLargeText(&gfx, x, y, text);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

