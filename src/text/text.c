#include <ultra64.h>

#define DEFINES_SET_TEXT_SLOT

#include "audio/sound.h"
#include "game/scene.h"
#include "round/game_state.h"
#include "round/round_hud.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "shot/current_shot.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "text/text.h"

typedef struct MessageTable {
    u16 size;
    u16 messageCount;
    TextSpan messages[1];
} MessageTable;

#define MessageSpanAt(id) (f_Messages->messages + (id))

#define ASSET_TEXT_SMALL_FONT 0x4B3
#define ASSET_TEXT_MEDIUM_FONT 0x4B5
#define ASSET_TEXT_LARGE_FONT 0x4B6
#define ASSET_TEXT_ICONS 0x4B0
#define ASSET_TEXT_SHADOWED_FONT 0x4B2
#define ASSET_TEXT_BANDED_GRADIENT 0x4B8
#define ASSET_TEXT_FADING_GRADIENT 0x4B9
#define GRADIENT_WIDTH 16
#define GRADIENT_HEIGHT 12
#define GRADIENT_HEIGHT_MASK 4
#define GRADIENT_TMEM_ADDRESS 0x100
#define FILE_HEADER_SIZE 8
#define GLYPH_COUNT_OFFSET 2
#define GLYPH_SIZE_OFFSET 4
#define MESSAGE_FILE_SIZE_OFFSET 0
#define ReadHeaderHalfword(header, offset) (((header)[offset] << 8) | (header)[(offset) + 1])

enum MessageCode {
    MESSAGE_CODE_GLYPH = 0x100,
    MESSAGE_CODE_CONTROL = 0x8000,
    MESSAGE_CODE_A_BUTTON = MESSAGE_CODE_CONTROL,
    MESSAGE_CODE_B_BUTTON,
    MESSAGE_CODE_UNUSED_Z_BUTTON_A,
    MESSAGE_CODE_UNUSED_Z_BUTTON_B,
    MESSAGE_CODE_UNUSED_Z_BUTTON_C,
    MESSAGE_CODE_UNUSED_Z_BUTTON_D,
    MESSAGE_CODE_Z_BUTTON,
    MESSAGE_CODE_C_UP,
    MESSAGE_CODE_C_DOWN,
    MESSAGE_CODE_C_LEFT,
    MESSAGE_CODE_C_RIGHT,
    MESSAGE_CODE_UNUSED_BLANK,
    MESSAGE_CODE_START_BUTTON,
    MESSAGE_CODE_L_BUTTON,
    MESSAGE_CODE_R_BUTTON,
    MESSAGE_CODE_NAME,
    MESSAGE_CODE_VALUE = 0x8014,
    MESSAGE_CODE_SCORE = 0x8018,
    MESSAGE_CODE_LINE_BREAK,
    MESSAGE_CODE_PALETTE_COLOR = 0x8022,
    MESSAGE_CODE_SLOT_COLOR = 0x8026
};

enum TextIcon {
    TEXT_ICON_A_BUTTON,
    TEXT_ICON_B_BUTTON,
    TEXT_ICON_START_BUTTON,
    TEXT_ICON_Z_BUTTON,
    TEXT_ICON_L_BUTTON,
    TEXT_ICON_R_BUTTON,
    TEXT_ICON_C_UP,
    TEXT_ICON_C_DOWN,
    TEXT_ICON_C_LEFT,
    TEXT_ICON_C_RIGHT
};

#define CHARACTER_CODE_MARK_FIRST 0xDE
#define CHARACTER_CODE_MARK_COUNT 2
#define CHARACTER_CODE_OTHER_MARK_FIRST 0x9E
#define LINE_SPACING 6
#define MEDIUM_SLOT_LINE_SPACING 4
#define SLOT_MARK_BACKSPACE 6
#define SLOT_MARK_RISE 4
#define MESSAGE_LINE_COUNT 3
#define ICON_WIDTH 12
#define MESSAGE_GLYPH_WIDTH 16
#define MESSAGE_GLYPH_HEIGHT 12
#define WIDE_LARGE_FONT_ADVANCE 14
#define TYPING_SOUND_CHANNEL 2
#define TYPING_SOUND_VOLUME 64
#define TYPING_SOUND_INTERVAL 3
#define ICON_TEXEL_COUNT (ICON_WIDTH * ICON_WIDTH)
#define NO_ICON (-1)
#define FONT_GLYPHS_PER_ROW 16
#define GLYPH_TILE_WIDTH 16

#define SHADOWED_GLYPH_WIDTH 8
#define SHADOWED_POINT_WIDTH 2
#define SHADOWED_POINT_INSET 3
#define CONDENSED_GLYPH_WIDTH 7
#define SHADOWED_GLYPH_SIZE 8
#define SHADOWED_GLYPHS_PER_ROW 31
#define SHADOWED_GLYPH_FIRST ' '
#define NARROW_GLYPH_ROW 3
#define NARROW_GLYPH_FIRST '\''
#define NARROW_GLYPH_SIZE 7
#define NARROW_GLYPH_WIDTH 6

#define SHADOW_OFFSET 1
#define SHADOW_DIMMING_SHIFT 2

extern u8 g_GlyphFileRomStart[];
extern u8 g_MessageFileRomStart[];
static ImageAsset *f_SmallFontImage;
static ImageAsset *f_MediumFontImage;
static ImageAsset *f_LargeFontImage;
static ImageAsset *f_TextGradientImages[2];
static void *f_MessageGlyphs;
static MessageTable *f_Messages;
static ImageAsset *f_TextIconImage;

#define NO_GLYPH_GROUP (-1)
#define LARGE_GLYPH_PART_ROW 7
#define LARGE_MARK_BACKSPACE 6.0f
#define LARGE_MARK_RISE 8.0f
#define MEDIUM_MARK_BACKSPACE 8.0f
#define MEDIUM_MARK_RISE 7.0f
#define SMALL_MARK_RISE 8.0f
#define MESSAGE_MARK_RISE 4.0f
#define LARGE_TEXT_MARK_BACKSPACE 6
#define LARGE_TEXT_MARK_RISE 4
#define MEDIUM_TEXT_MARK_BACKSPACE 8
#define MEDIUM_TEXT_MARK_RISE 7
#define SMALL_TEXT_MARK_BACKSPACE 6
#define SMALL_TEXT_MARK_RISE 6
#define LARGE_POINT_INSET 6
#define LARGE_POINT_WIDTH 3
#define MEDIUM_POINT_INSET 5
#define MEDIUM_POINT_WIDTH 2
#define SMALL_FONT_MOVED_CODE 0xB0
#define SMALL_FONT_MOVED_GLYPH 0x70

#define CHARACTER_CODE_COUNT 256

/* clang-format off */
static u8 f_LargeFontWidths[CHARACTER_CODE_COUNT] = {
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,  6, 12,  6, 12,
       5,  6,  7, 10,  8, 12, 10,  6,  6,  6, 12, 10,  6, 10,  6, 12,
       8,  6,  8,  7,  8,  7,  7,  9,  7,  7,  6,  6,  7, 10,  7,  7,
      12, 11, 10,  8,  9,  8,  6, 10,  9,  4,  9,  7,  7, 12, 10, 12,
       9, 11,  9,  7,  9, 11, 12, 12, 10,  9, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
       0,  9,  7,  7,  9,  9,  8,  8,  8,  4,  5,  8,  4,  8,  8,  8,
       6,  9,  6,  6,  6,  9,  9, 12,  9, 10,  7,  0,  0,  0,  0,  0,
};
/* clang-format on */
static Vtx f_LargeGlyphVertices[QUAD_VERTEX_COUNT] = {
    {{{0, 6, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, -6, 0}, 0, {0, 0x2FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{12, -6, 0}, 0, {0x2FF, 0x2FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{12, 6, 0}, 0, {0x2FF, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_LargeGlyphTopVertices[QUAD_VERTEX_COUNT] = {
    {{{0, 6, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, 0, 0}, 0, {0, 0x17F}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{12, 0, 0}, 0, {0x2FF, 0x17F}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{12, 6, 0}, 0, {0x2FF, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_LargeGlyphBottomVertices[QUAD_VERTEX_COUNT] = {
    {{{0, 0, 0}, 0, {0, 0x180}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, -6, 0}, 0, {0, 0x2FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{12, -6, 0}, 0, {0x2FF, 0x2FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{12, 0, 0}, 0, {0x2FF, 0x180}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_MediumGlyphVertices[QUAD_VERTEX_COUNT] = {
    {{{0, 5, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, -5, 0}, 0, {0, 0x27F}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{10, -5, 0}, 0, {0x27F, 0x27F}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{10, 5, 0}, 0, {0x27F, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_SmallGlyphVertices[QUAD_VERTEX_COUNT] = {
    {{{0, 4, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, -4, 0}, 0, {0, 0x1FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{8, -4, 0}, 0, {0x1FF, 0x1FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{8, 6, 0}, 0, {0x1FF, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};

void DrawMediumTextSlot(Gfx **displayListHead, s32 slot);
void ClearGlyphSprites(void);
s32 GetTextLength(READ_ONLY char *text);

char g_PrintBuffer[PRINT_BUFFER_SIZE];
ImageAsset *g_ShadowedFontImage;

FILE_STATIC s32 g_MessageCodesStart;
FILE_STATIC TextSlot g_TextSlots[TEXT_SLOT_COUNT];
FILE_STATIC u8 g_TextPalette[TEXT_PALETTE_COLOR_COUNT][ALPHA_CHANNEL];
FILE_STATIC char g_TextName[TEXT_NAME_SIZE];
FILE_STATIC GlyphSprite g_GlyphSprites[GLYPH_SPRITE_COUNT];
FILE_STATIC u8 g_GlyphGroupSizes[GLYPH_GROUP_COUNT];

static inline s16 GetShadowedGlyphLeft(s32 code) {
    return (code - SHADOWED_GLYPH_FIRST) % SHADOWED_GLYPHS_PER_ROW * SHADOWED_GLYPH_SIZE;
}

static inline s16 GetShadowedGlyphTop(s32 code) {
    return (code - SHADOWED_GLYPH_FIRST) / SHADOWED_GLYPHS_PER_ROW * SHADOWED_GLYPH_SIZE;
}

void LoadTextAssets(void) {
    EncodedAsset asset;
    u8 *header;
    s32 size;

    g_ShadowedFontImage = AllocMemory(OpenEncodedAsset(ASSET_TEXT_SHADOWED_FONT, &asset));
    ReadEncodedAsset(g_ShadowedFontImage, &asset);
    f_SmallFontImage = AllocMemory(OpenEncodedAsset(ASSET_TEXT_SMALL_FONT, &asset));
    ReadEncodedAsset(f_SmallFontImage, &asset);
    f_MediumFontImage = AllocMemory(OpenEncodedAsset(ASSET_TEXT_MEDIUM_FONT, &asset));
    ReadEncodedAsset(f_MediumFontImage, &asset);
    f_LargeFontImage = AllocMemory(OpenEncodedAsset(ASSET_TEXT_LARGE_FONT, &asset));
    ReadEncodedAsset(f_LargeFontImage, &asset);
    f_TextIconImage = AllocMemory(OpenEncodedAsset(ASSET_TEXT_ICONS, &asset));
    ReadEncodedAsset(f_TextIconImage, &asset);
    f_TextGradientImages[0] = AllocMemory(OpenEncodedAsset(ASSET_TEXT_FADING_GRADIENT, &asset));
    ReadEncodedAsset(f_TextGradientImages[0], &asset);
    f_TextGradientImages[1] = AllocMemory(OpenEncodedAsset(ASSET_TEXT_BANDED_GRADIENT, &asset));
    ReadEncodedAsset(f_TextGradientImages[1], &asset);
    header = AllocMemory(FILE_HEADER_SIZE);
    nuPiReadRom((u32)g_GlyphFileRomStart, header, FILE_HEADER_SIZE);
    size = (ReadHeaderHalfword(header, GLYPH_COUNT_OFFSET) *
               ReadHeaderHalfword(header, GLYPH_SIZE_OFFSET)) +
        FILE_HEADER_SIZE;
    f_MessageGlyphs = AllocMemory(size);
    nuPiReadRom((u32)g_GlyphFileRomStart, f_MessageGlyphs, size);
    nuPiReadRom((u32)g_MessageFileRomStart, header, FILE_HEADER_SIZE);
    size = ReadHeaderHalfword(header, MESSAGE_FILE_SIZE_OFFSET);
    FreeMemory(&header);
    f_Messages = AllocMemory(size);
    nuPiReadRom((u32)g_MessageFileRomStart, f_Messages, size);

    g_MessageCodesStart = (f_Messages->messageCount * 2) + 2;
    ClearTextSlots();
    ClearGlyphSprites();
}

void FreeTextAssets(void) {
    FreeMemory(&g_ShadowedFontImage);
    FreeMemory(&f_SmallFontImage);
    FreeMemory(&f_MediumFontImage);
    FreeMemory(&f_LargeFontImage);
    FreeMemory(&f_TextIconImage);
    FreeMemory(&f_MessageGlyphs);
    FreeMemory(&f_Messages);
}

void SetUpTextDrawing(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gSPLoadGeometryMode(gfx++, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetBlendColor(gfx++, 0, 0, 0, 1);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPSetEnvColor(gfx++, 128, 128, 255, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
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
    g_TextPalette[colorIndex][RED_CHANNEL] = red;
    g_TextPalette[colorIndex][GREEN_CHANNEL] = green;
    g_TextPalette[colorIndex][BLUE_CHANNEL] = blue;
}

void SetTextSlotValue(s32 value, s32 slot, s32 valueIndex) {
    g_TextSlots[slot].values[valueIndex] = value;
}

void SetTextName(char *name) {
    s32 letterIndex;
    char character;

    for (letterIndex = 0; letterIndex != TEXT_NAME_SIZE - 1; letterIndex++) {
        character = name[letterIndex];
        g_TextName[letterIndex] = character;
        if (character >= 'a' && character <= 'z') {
            g_TextName[letterIndex] = character + LOWERCASE_CODE_OFFSET;
        }

        if (name[letterIndex] == '\0') {
            break;
        }
    }

    g_TextName[TEXT_NAME_SIZE - 1] = '\0';
}

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
) {
    TextSlot *textSlot;
    s32 width;
    s32 height;
    s32 halfWidth;
    s32 halfHeight;

    if (messageId != NO_MESSAGE) {
        width = 0;
        height = 0;
        if (flags & TEXT_FLAG_CENTERED) {
            MeasureMessage(messageId, FONT_SIZE_LARGE, &width, &height, slot);
        }

        textSlot = &g_TextSlots[slot];
        halfWidth = width / 2;
        halfHeight = height / 2;
        g_TextSlots[slot].red = red;
        g_TextSlots[slot].green = green;
        g_TextSlots[slot].blue = blue;
        g_TextSlots[slot].alpha = alpha;
        g_TextSlots[slot].fontSize = fontSize;
        g_TextSlots[slot].flags = flags;
        g_TextSlots[slot].x = x - halfWidth;
        g_TextSlots[slot].y = y - halfHeight;
        if (textSlot->messageId != messageId) {
            g_TextSlots[slot].shownCount = 0;
        }
    }

    g_TextSlots[slot].messageId = messageId;
}

void DrawTextSlots(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 slot;

    SetUpTextDrawing(&gfx);
    for (slot = 0; slot != TEXT_SLOT_COUNT; slot++) {
        if (g_TextSlots[slot].messageId != NO_MESSAGE &&
            !(g_TextSlots[slot].flags & TEXT_FLAG_DRAWN_BY_OWNER)) {
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                g_TextSlots[slot].red,
                g_TextSlots[slot].green,
                g_TextSlots[slot].blue,
                g_TextSlots[slot].alpha
            );
            if (g_TextSlots[slot].fontSize == FONT_SIZE_MEDIUM) {
                DrawMediumTextSlot(&gfx, slot);
            }
            else {
                DrawLargeTextSlot(&gfx, slot);
            }
        }
    }

    *displayListHead = gfx;
}

void RevealTextSlot(s32 slot) {
    g_TextSlots[slot].shownCount = 0xFF;
}

Boolean IsTextSlotRevealed(s32 slot) {
    TextSlot *textSlot = &g_TextSlots[slot];

    return !(textSlot->shownCount < MessageSpanAt(textSlot->messageId)->length);
}

void DrawLargeTextSlot(Gfx **displayListHead, s32 slot) {
    Gfx *gfx = *displayListHead;
    TextSlot *textSlot = &g_TextSlots[slot];
    s32 codeIndex;
    s32 messageStart;
    s32 length;
    Boolean isIconLoaded;
    s32 x;
    s32 y;
    s32 advance;
    s32 code;
    Boolean isPrinted;
    s32 iconOffset;
    s32 score;
    s32 index;
    s32 glyphLeft;
    s32 glyphTop;
    s32 characterLeft;
    s32 characterTop;

    isIconLoaded = FALSE;
    x = textSlot->x;
    y = textSlot->y;

    length = MessageSpanAt(textSlot->messageId)->length;
    messageStart = MessageSpanAt(textSlot->messageId)->start;
    if (textSlot->flags & TEXT_FLAG_TYPED) {
        if (textSlot->shownCount < length) {
            length = textSlot->shownCount;
            textSlot->shownCount++;
            if (textSlot->flags & TEXT_FLAG_TYPING_SOUND) {
                if (textSlot->shownCount % TYPING_SOUND_INTERVAL == 0) {
                    PlaySound(SOUND_TEXT_TYPING, TYPING_SOUND_CHANNEL, TYPING_SOUND_VOLUME);
                }
            }
        }
    }

    advance =
        (textSlot->flags & TEXT_FLAG_WIDE_SPACING) ? WIDE_LARGE_FONT_ADVANCE : FONT_SIZE_LARGE;
    gDPPipeSync(gfx++);
    for (codeIndex = 0; codeIndex != length; codeIndex++) {
        code = ((u16 *)f_Messages)[messageStart + codeIndex + g_MessageCodesStart];
        if (isIconLoaded == TRUE) {
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_NONE);
            isIconLoaded = FALSE;
        }

        if (code < MESSAGE_CODE_GLYPH) {
            glyphLeft = code % FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
            glyphTop = code / FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
            gDPSetTextureImage(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                TextureAddress(ImageBody(f_LargeFontImage))
            );
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                1,
                0,
                G_TX_LOADTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPLoadSync(gfx++);
            gDPLoadTile(
                gfx++,
                G_TX_LOADTILE,
                glyphLeft << 1,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + FONT_SIZE_LARGE - 1) << 1,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_4b,
                1,
                0,
                G_TX_RENDERTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPSetTileSize(
                gfx++,
                G_TX_RENDERTILE,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            if (code >= CHARACTER_CODE_MARK_FIRST &&
                code < CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(x - LARGE_TEXT_MARK_BACKSPACE),
                    QuarterPixels(y - LARGE_TEXT_MARK_RISE),
                    QuarterPixels(x - LARGE_TEXT_MARK_BACKSPACE + FONT_SIZE_LARGE),
                    QuarterPixels(y - LARGE_TEXT_MARK_RISE + FONT_SIZE_LARGE),
                    G_TX_RENDERTILE,
                    TexelCoordinate(glyphLeft),
                    TexelCoordinate(glyphTop),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
            else {
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(x),
                    QuarterPixels(y),
                    QuarterPixels(x + FONT_SIZE_LARGE),
                    QuarterPixels(y + FONT_SIZE_LARGE),
                    G_TX_RENDERTILE,
                    TexelCoordinate(glyphLeft),
                    TexelCoordinate(glyphTop),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                x += f_LargeFontWidths[code];
            }

            gDPPipeSync(gfx++);
        }
        else if (code < MESSAGE_CODE_CONTROL) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    AddressPast(f_MessageGlyphs, FILE_HEADER_SIZE) +
                    ((code - MESSAGE_CODE_GLYPH) * MESSAGE_GLYPH_WIDTH * MESSAGE_GLYPH_HEIGHT)
                ),
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                MESSAGE_GLYPH_WIDTH,
                MESSAGE_GLYPH_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(x),
                QuarterPixels(y),
                QuarterPixels(x + FONT_SIZE_LARGE),
                QuarterPixels(y + FONT_SIZE_LARGE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            x += advance;
        }
        else {
            isPrinted = FALSE;
            iconOffset = NO_ICON;
            switch (code) {
                case MESSAGE_CODE_LINE_BREAK:
                    x = textSlot->x;
                    y += LINE_SPACING + advance;
                    break;
                case MESSAGE_CODE_A_BUTTON:
                    iconOffset = TEXT_ICON_A_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_B_BUTTON:
                    iconOffset = TEXT_ICON_B_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_UNUSED_Z_BUTTON_A:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_B:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_C:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_D:
                case MESSAGE_CODE_Z_BUTTON:
                    iconOffset = TEXT_ICON_Z_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_UP:
                    iconOffset = TEXT_ICON_C_UP * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_DOWN:
                    iconOffset = TEXT_ICON_C_DOWN * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_LEFT:
                    iconOffset = TEXT_ICON_C_LEFT * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_RIGHT:
                    iconOffset = TEXT_ICON_C_RIGHT * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_START_BUTTON:
                    iconOffset = TEXT_ICON_START_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_L_BUTTON:
                    iconOffset = TEXT_ICON_L_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_R_BUTTON:
                    iconOffset = TEXT_ICON_R_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_NAME:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%s", g_TextName);
                    break;
                case MESSAGE_CODE_SLOT_COLOR:
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        textSlot->red,
                        textSlot->green,
                        textSlot->blue,
                        textSlot->alpha
                    );
                    break;
                case MESSAGE_CODE_PALETTE_COLOR:
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        g_TextPalette[0][RED_CHANNEL],
                        g_TextPalette[0][GREEN_CHANNEL],
                        g_TextPalette[0][BLUE_CHANNEL],
                        OPAQUE_ALPHA
                    );
                    break;
                case MESSAGE_CODE_PALETTE_COLOR + 1:
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        g_TextPalette[1][RED_CHANNEL],
                        g_TextPalette[1][GREEN_CHANNEL],
                        g_TextPalette[1][BLUE_CHANNEL],
                        OPAQUE_ALPHA
                    );
                    break;
                case MESSAGE_CODE_PALETTE_COLOR + 2:
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        g_TextPalette[2][RED_CHANNEL],
                        g_TextPalette[2][GREEN_CHANNEL],
                        g_TextPalette[2][BLUE_CHANNEL],
                        OPAQUE_ALPHA
                    );
                    break;
                case MESSAGE_CODE_PALETTE_COLOR + 3:
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        g_TextPalette[3][RED_CHANNEL],
                        g_TextPalette[3][GREEN_CHANNEL],
                        g_TextPalette[3][BLUE_CHANNEL],
                        OPAQUE_ALPHA
                    );
                    break;
                case MESSAGE_CODE_VALUE:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[0]);
                    break;
                case MESSAGE_CODE_VALUE + 1:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[1]);
                    break;
                case MESSAGE_CODE_VALUE + 2:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[2]);
                    break;
                case MESSAGE_CODE_VALUE + 3:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[3]);
                    break;
                case MESSAGE_CODE_SCORE:
                    score = g_RoundHud.skinsTotals[g_CurrentShot.playerIndex];
                    isPrinted = TRUE;
                    if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                        if (score == 0) {
                            sprintf(g_PrintBuffer, "EVEN");
                        }
                        else if (score < 0) {
                            sprintf(g_PrintBuffer, "%dUP", score);
                        }
                        else {
                            sprintf(g_PrintBuffer, "%dDOWN", score);
                        }
                    }
                    else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                        sprintf(g_PrintBuffer, "%dP", score);
                    }
                    else if (score == 0) {
                        sprintf(g_PrintBuffer, "#0");
                    }
                    else {
                        sprintf(g_PrintBuffer, "%+d", score);
                    }

                    break;
            }

            if (isPrinted == TRUE) {
                for (index = 0; g_PrintBuffer[index] != '\0'; index++) {
                    characterLeft = g_PrintBuffer[index] % FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
                    characterTop = g_PrintBuffer[index] / FONT_GLYPHS_PER_ROW * FONT_SIZE_LARGE;
                    gDPSetTextureImage(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                        TextureAddress(ImageBody(f_LargeFontImage))
                    );
                    gDPSetTile(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        1,
                        0,
                        G_TX_LOADTILE,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD
                    );
                    gDPLoadSync(gfx++);
                    gDPLoadTile(
                        gfx++,
                        G_TX_LOADTILE,
                        characterLeft << 1,
                        characterTop << G_TEXTURE_IMAGE_FRAC,
                        (characterLeft + GLYPH_TILE_WIDTH - 1) << 1,
                        (characterTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
                    );
                    gDPPipeSync(gfx++);
                    gDPSetTile(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_4b,
                        1,
                        0,
                        G_TX_RENDERTILE,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD
                    );
                    gDPSetTileSize(
                        gfx++,
                        G_TX_RENDERTILE,
                        characterLeft << G_TEXTURE_IMAGE_FRAC,
                        characterTop << G_TEXTURE_IMAGE_FRAC,
                        (characterLeft + GLYPH_TILE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                        (characterTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
                    );
                    gDPPipeSync(gfx++);
                    if (g_PrintBuffer[index] >= CHARACTER_CODE_MARK_FIRST &&
                        g_PrintBuffer[index] <
                            CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                        gSPTextureRectangle(
                            gfx++,
                            QuarterPixels(x - LARGE_TEXT_MARK_BACKSPACE),
                            QuarterPixels(y - LARGE_TEXT_MARK_RISE),
                            QuarterPixels(x - LARGE_TEXT_MARK_BACKSPACE + FONT_SIZE_LARGE),
                            QuarterPixels(y - LARGE_TEXT_MARK_RISE + FONT_SIZE_LARGE),
                            G_TX_RENDERTILE,
                            TexelCoordinate(characterLeft),
                            TexelCoordinate(characterTop),
                            TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE
                        );
                    }
                    else {
                        gSPTextureRectangle(
                            gfx++,
                            QuarterPixels(x),
                            QuarterPixels(y),
                            QuarterPixels(x + FONT_SIZE_LARGE),
                            QuarterPixels(y + FONT_SIZE_LARGE),
                            G_TX_RENDERTILE,
                            TexelCoordinate(characterLeft),
                            TexelCoordinate(characterTop),
                            TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE
                        );
                        x += f_LargeFontWidths[g_PrintBuffer[index]];
                    }
                }
            }

            if (iconOffset != NO_ICON) {
                if (isIconLoaded == FALSE) {
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_TextIconImage)));
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    isIconLoaded = TRUE;
                }

                gDPSetTextureImage(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    ICON_WIDTH,
                    TextureAddress(LargeImageTexels(f_TextIconImage) + iconOffset)
                );
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    2,
                    0,
                    G_TX_LOADTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPLoadSync(gfx++);
                gDPLoadTile(
                    gfx++,
                    G_TX_LOADTILE,
                    0,
                    0,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC
                );
                gDPPipeSync(gfx++);
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    2,
                    0,
                    G_TX_RENDERTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPSetTileSize(
                    gfx++,
                    G_TX_RENDERTILE,
                    0,
                    0,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC
                );
                gDPPipeSync(gfx++);
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(x),
                    QuarterPixels(y),
                    QuarterPixels(x + FONT_SIZE_LARGE),
                    QuarterPixels(y + FONT_SIZE_LARGE),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
                x += advance;
            }
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawMediumTextSlot(Gfx **displayListHead, s32 slot) {
    Gfx *gfx = *displayListHead;
    TextSlot *textSlot = &g_TextSlots[slot];
    s32 codeIndex;
    s32 messageStart;
    s32 length;
    Boolean isIconLoaded;
    s32 x;
    s32 y;
    s32 advance;
    s32 code;
    Boolean isPrinted;
    s32 iconOffset;
    s32 score;
    s32 index;
    s32 glyphLeft;
    s32 glyphTop;
    s32 characterLeft;
    s32 characterTop;

    isIconLoaded = FALSE;
    x = textSlot->x;
    y = textSlot->y;

    length = MessageSpanAt(textSlot->messageId)->length;
    messageStart = MessageSpanAt(textSlot->messageId)->start;
    if (textSlot->flags & TEXT_FLAG_TYPED) {
        if (textSlot->shownCount < length) {
            length = textSlot->shownCount;
            textSlot->shownCount++;
        }
    }

    advance = FONT_SIZE_MEDIUM;
    if (textSlot->flags & TEXT_FLAG_WIDE_SPACING) {
        advance = FONT_SIZE_LARGE;
    }

    gDPPipeSync(gfx++);
    for (codeIndex = 0; codeIndex != length; codeIndex++) {
        code = ((u16 *)f_Messages)[messageStart + codeIndex + g_MessageCodesStart];
        if (isIconLoaded == TRUE) {
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_NONE);
            isIconLoaded = FALSE;
        }

        if (code < MESSAGE_CODE_GLYPH) {
            glyphLeft = code % FONT_GLYPHS_PER_ROW * FONT_SIZE_MEDIUM;
            glyphTop = code / FONT_GLYPHS_PER_ROW * FONT_SIZE_MEDIUM;
            gDPSetTextureImage(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                FONT_SIZE_MEDIUM * FONT_GLYPHS_PER_ROW / 2,
                TextureAddress(ImageBody(f_MediumFontImage))
            );
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                1,
                0,
                G_TX_LOADTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPLoadSync(gfx++);
            gDPLoadTile(
                gfx++,
                G_TX_LOADTILE,
                glyphLeft << 1,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + FONT_SIZE_LARGE - 1) << 1,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_4b,
                1,
                0,
                G_TX_RENDERTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPSetTileSize(
                gfx++,
                G_TX_RENDERTILE,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            if (code >= CHARACTER_CODE_MARK_FIRST &&
                code < CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(x - SLOT_MARK_BACKSPACE),
                    QuarterPixels(y - SLOT_MARK_RISE),
                    QuarterPixels(x - SLOT_MARK_BACKSPACE + FONT_SIZE_MEDIUM),
                    QuarterPixels(y - SLOT_MARK_RISE + FONT_SIZE_MEDIUM),
                    G_TX_RENDERTILE,
                    TexelCoordinate(glyphLeft),
                    TexelCoordinate(glyphTop),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
            else {
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(x),
                    QuarterPixels(y),
                    QuarterPixels(x + FONT_SIZE_MEDIUM),
                    QuarterPixels(y + FONT_SIZE_MEDIUM),
                    G_TX_RENDERTILE,
                    TexelCoordinate(glyphLeft),
                    TexelCoordinate(glyphTop),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                x += advance;
            }

            gDPPipeSync(gfx++);
        }
        else if (code < MESSAGE_CODE_CONTROL) {
            gDPPipeSync(gfx++);
            x += advance;
        }
        else {
            isPrinted = FALSE;
            iconOffset = NO_ICON;
            switch (code) {
                case MESSAGE_CODE_LINE_BREAK:
                    x = textSlot->x;
                    y += MEDIUM_SLOT_LINE_SPACING + advance;
                    break;
                case MESSAGE_CODE_A_BUTTON:
                    iconOffset = TEXT_ICON_A_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_B_BUTTON:
                    iconOffset = TEXT_ICON_B_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_UNUSED_Z_BUTTON_A:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_B:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_C:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_D:
                case MESSAGE_CODE_Z_BUTTON:
                    iconOffset = TEXT_ICON_Z_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_UP:
                    iconOffset = TEXT_ICON_C_UP * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_DOWN:
                    iconOffset = TEXT_ICON_C_DOWN * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_LEFT:
                    iconOffset = TEXT_ICON_C_LEFT * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_C_RIGHT:
                    iconOffset = TEXT_ICON_C_RIGHT * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_START_BUTTON:
                    iconOffset = TEXT_ICON_START_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_L_BUTTON:
                    iconOffset = TEXT_ICON_L_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_R_BUTTON:
                    iconOffset = TEXT_ICON_R_BUTTON * ICON_TEXEL_COUNT;
                    break;
                case MESSAGE_CODE_NAME:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%s", g_TextName);
                    break;
                case MESSAGE_CODE_VALUE:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[0]);
                    break;
                case MESSAGE_CODE_VALUE + 1:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[1]);
                    break;
                case MESSAGE_CODE_VALUE + 2:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[2]);
                    break;
                case MESSAGE_CODE_VALUE + 3:
                    isPrinted = TRUE;
                    sprintf(g_PrintBuffer, "%d", textSlot->values[3]);
                    break;
                case MESSAGE_CODE_SCORE:
                    score = g_RoundHud.skinsTotals[g_CurrentShot.playerIndex];
                    isPrinted = TRUE;
                    if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                        if (score == 0) {
                            sprintf(g_PrintBuffer, "EVEN");
                        }
                        else if (score < 0) {
                            sprintf(g_PrintBuffer, "%dUP", score);
                        }
                        else {
                            sprintf(g_PrintBuffer, "%dDOWN", score);
                        }
                    }
                    else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                        sprintf(g_PrintBuffer, "%dP", score);
                    }
                    else if (score == 0) {
                        sprintf(g_PrintBuffer, "#0");
                    }
                    else {
                        sprintf(g_PrintBuffer, "%+d", score);
                    }

                    break;
            }

            if (isPrinted == TRUE) {
                for (index = 0; g_PrintBuffer[index] != '\0'; index++) {
                    characterLeft = g_PrintBuffer[index] % FONT_GLYPHS_PER_ROW * FONT_SIZE_MEDIUM;
                    characterTop = g_PrintBuffer[index] / FONT_GLYPHS_PER_ROW * FONT_SIZE_MEDIUM;
                    gDPSetTextureImage(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        FONT_SIZE_MEDIUM * FONT_GLYPHS_PER_ROW / 2,
                        TextureAddress(ImageBody(f_MediumFontImage))
                    );
                    gDPSetTile(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        1,
                        0,
                        G_TX_LOADTILE,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD
                    );
                    gDPLoadSync(gfx++);
                    gDPLoadTile(
                        gfx++,
                        G_TX_LOADTILE,
                        characterLeft << 1,
                        characterTop << G_TEXTURE_IMAGE_FRAC,
                        (characterLeft + GLYPH_TILE_WIDTH - 1) << 1,
                        (characterTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
                    );
                    gDPPipeSync(gfx++);
                    gDPSetTile(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_4b,
                        1,
                        0,
                        G_TX_RENDERTILE,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOLOD
                    );
                    gDPSetTileSize(
                        gfx++,
                        G_TX_RENDERTILE,
                        characterLeft << G_TEXTURE_IMAGE_FRAC,
                        characterTop << G_TEXTURE_IMAGE_FRAC,
                        (characterLeft + GLYPH_TILE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                        (characterTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
                    );
                    gDPPipeSync(gfx++);
                    if (g_PrintBuffer[index] >= CHARACTER_CODE_MARK_FIRST &&
                        g_PrintBuffer[index] <
                            CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                        gSPTextureRectangle(
                            gfx++,
                            QuarterPixels(x - (FONT_SIZE_MEDIUM / 2)),
                            QuarterPixels(y - SLOT_MARK_RISE),
                            QuarterPixels(x + (FONT_SIZE_MEDIUM / 2)),
                            QuarterPixels(y - SLOT_MARK_RISE + FONT_SIZE_MEDIUM),
                            G_TX_RENDERTILE,
                            TexelCoordinate(characterLeft),
                            TexelCoordinate(characterTop),
                            TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE
                        );
                    }
                    else {
                        gSPTextureRectangle(
                            gfx++,
                            QuarterPixels(x),
                            QuarterPixels(y),
                            QuarterPixels(x + FONT_SIZE_MEDIUM),
                            QuarterPixels(y + FONT_SIZE_MEDIUM),
                            G_TX_RENDERTILE,
                            TexelCoordinate(characterLeft),
                            TexelCoordinate(characterTop),
                            TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE
                        );
                        x += advance;
                    }
                }
            }

            if (iconOffset != NO_ICON) {
                if (isIconLoaded == FALSE) {
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_TextIconImage)));
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    isIconLoaded = TRUE;
                }

                gDPSetTextureImage(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    ICON_WIDTH,
                    TextureAddress(LargeImageTexels(f_TextIconImage) + iconOffset)
                );
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    2,
                    0,
                    G_TX_LOADTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPLoadSync(gfx++);
                gDPLoadTile(
                    gfx++,
                    G_TX_LOADTILE,
                    0,
                    0,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC
                );
                gDPPipeSync(gfx++);
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    2,
                    0,
                    G_TX_RENDERTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPSetTileSize(
                    gfx++,
                    G_TX_RENDERTILE,
                    0,
                    0,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC
                );
                gDPPipeSync(gfx++);
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(x),
                    QuarterPixels(y),
                    QuarterPixels(x + FONT_SIZE_MEDIUM),
                    QuarterPixels(y + FONT_SIZE_MEDIUM),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
                x += advance;
            }
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void SetUpGradientTextDrawing(Gfx **displayListHead, s32 gradient) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gSPLoadGeometryMode(gfx++, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPSetCombineLERP(
        gfx++,
        ENVIRONMENT,
        PRIMITIVE,
        TEXEL1,
        PRIMITIVE,
        0,
        0,
        0,
        TEXEL0,
        0,
        0,
        0,
        COMBINED,
        0,
        0,
        0,
        COMBINED
    );
    gDPSetEnvColor(gfx++, 128, 128, 255, OPAQUE_ALPHA);
    gDPSetBlendColor(gfx++, 0, 0, 0, 1);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_TextGradientImages[gradient & 1])),
        GRADIENT_TMEM_ADDRESS,
        1,
        G_IM_FMT_I,
        GRADIENT_WIDTH,
        GRADIENT_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        GRADIENT_HEIGHT_MASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

s32 MeasureText(READ_ONLY char *text, s32 fontSize, TextFlags flags) {
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
                            width += f_LargeFontWidths[code];
                        }
                        else {
                            width += FONT_SIZE_LARGE;
                        }

                        break;
                }

                text++;
            }
            while (*text != '\0');
        }
    }
    else {
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

void MeasureMessage(MessageId messageId, s32 fontSize, s32 *width, s32 *height, s32 slot) {
    s32 lineWidths[MESSAGE_LINE_COUNT] = {0, 0, 0};
    s32 codeIndex;
    s32 lineCount;
    s32 line;
    s32 widest;
    s32 code;
    s32 length;
    s32 messageStart;

    lineCount = 0;

    length = MessageSpanAt(messageId)->length;
    messageStart = MessageSpanAt(messageId)->start;
    line = 0;
    for (codeIndex = 0; codeIndex != length; codeIndex++) {
        code = ((u16 *)f_Messages)[g_MessageCodesStart + messageStart + codeIndex];
        if (code < MESSAGE_CODE_GLYPH) {
            if (code < CHARACTER_CODE_MARK_FIRST ||
                code >= CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                if (fontSize == FONT_SIZE_LARGE) {
                    lineWidths[line] += f_LargeFontWidths[code];
                }
                else {
                    lineWidths[line] += fontSize;
                }
            }
        }
        else if (code >= MESSAGE_CODE_CONTROL) {
            switch (code) {
                case MESSAGE_CODE_LINE_BREAK:
                    line++;
                    lineCount += LINE_SPACING + fontSize;
                    break;
                case MESSAGE_CODE_NAME:
                    sprintf(g_PrintBuffer, "%s", g_TextName);
                    lineWidths[line] +=
                        MeasureText(g_PrintBuffer, fontSize, MEASURE_TEXT_PROPORTIONAL);
                    break;
                case MESSAGE_CODE_VALUE:
                case MESSAGE_CODE_VALUE + 1:
                case MESSAGE_CODE_VALUE + 2:
                case MESSAGE_CODE_VALUE + 3:
                    sprintf(
                        g_PrintBuffer,
                        "%d",
                        g_TextSlots[slot].values[code - MESSAGE_CODE_VALUE]
                    );
                    lineWidths[line] += GetTextLength(g_PrintBuffer) * fontSize;
                    break;
                case MESSAGE_CODE_A_BUTTON:
                case MESSAGE_CODE_B_BUTTON:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_A:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_B:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_C:
                case MESSAGE_CODE_UNUSED_Z_BUTTON_D:
                case MESSAGE_CODE_Z_BUTTON:
                case MESSAGE_CODE_C_UP:
                case MESSAGE_CODE_C_DOWN:
                case MESSAGE_CODE_C_LEFT:
                case MESSAGE_CODE_C_RIGHT:
                case MESSAGE_CODE_START_BUTTON:
                case MESSAGE_CODE_L_BUTTON:
                case MESSAGE_CODE_R_BUTTON:
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

s32 GetTextLength(READ_ONLY char *text) {
    s32 length = 0;

    while (*text != '\0') {
        text++;
        length++;
    }

    return length;
}

void DrawFixedWidthLargeText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
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
        }
        else {
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
            gDPSetTextureImage(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                TextureAddress(ImageBody(f_LargeFontImage))
            );
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                1,
                0,
                G_TX_LOADTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPLoadSync(gfx++);
            gDPLoadTile(
                gfx++,
                G_TX_LOADTILE,
                glyphLeft << 1,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH - 1) << 1,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_4b,
                1,
                0,
                G_TX_RENDERTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPSetTileSize(
                gfx++,
                G_TX_RENDERTILE,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPSetTileSize(
                gfx++,
                1,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_LARGE) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(left),
                QuarterPixels(top),
                QuarterPixels(left + FONT_SIZE_LARGE),
                QuarterPixels(top + FONT_SIZE_LARGE),
                G_TX_RENDERTILE,
                TexelCoordinate(glyphLeft),
                TexelCoordinate(glyphTop),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            x += FONT_SIZE_LARGE;
            text++;
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawLargeText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
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
            x += f_LargeFontWidths[' '];
        }
        else {
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
            gDPSetTextureImage(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                TextureAddress(ImageBody(f_LargeFontImage))
            );
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                1,
                0,
                G_TX_LOADTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPLoadSync(gfx++);
            gDPLoadTile(
                gfx++,
                G_TX_LOADTILE,
                glyphLeft << 1,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH - 1) << 1,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_4b,
                1,
                0,
                G_TX_RENDERTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPSetTileSize(
                gfx++,
                G_TX_RENDERTILE,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPSetTileSize(
                gfx++,
                1,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_LARGE) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(left),
                QuarterPixels(top),
                QuarterPixels(left + FONT_SIZE_LARGE),
                QuarterPixels(top + FONT_SIZE_LARGE),
                G_TX_RENDERTILE,
                TexelCoordinate(glyphLeft),
                TexelCoordinate(glyphTop),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            text++;
            x += f_LargeFontWidths[code];
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawMediumText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
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
        }
        else {
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
            gDPSetTextureImage(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                FONT_SIZE_MEDIUM * FONT_GLYPHS_PER_ROW / 2,
                TextureAddress(ImageBody(f_MediumFontImage))
            );
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                1,
                0,
                G_TX_LOADTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPLoadSync(gfx++);
            gDPLoadTile(
                gfx++,
                G_TX_LOADTILE,
                glyphLeft << 1,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH - 1) << 1,
                (glyphTop + FONT_SIZE_MEDIUM - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gDPSetTile(
                gfx++,
                G_IM_FMT_I,
                G_IM_SIZ_4b,
                1,
                0,
                G_TX_RENDERTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPSetTileSize(
                gfx++,
                G_TX_RENDERTILE,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + GLYPH_TILE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_MEDIUM - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(left),
                QuarterPixels(top),
                QuarterPixels(left + FONT_SIZE_MEDIUM),
                QuarterPixels(top + FONT_SIZE_MEDIUM),
                G_TX_RENDERTILE,
                TexelCoordinate(glyphLeft),
                TexelCoordinate(glyphTop),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            text++;
            x += FONT_SIZE_MEDIUM;
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawSmallText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
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
        }
        else {
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
            gDPSetTextureImage(
                gfx++,
                G_IM_FMT_IA,
                G_IM_SIZ_8b,
                FONT_SIZE_SMALL * FONT_GLYPHS_PER_ROW / 2,
                TextureAddress(ImageBody(f_SmallFontImage))
            );
            gDPSetTile(
                gfx++,
                G_IM_FMT_IA,
                G_IM_SIZ_8b,
                1,
                0,
                G_TX_LOADTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPLoadSync(gfx++);
            gDPLoadTile(
                gfx++,
                G_TX_LOADTILE,
                glyphLeft << 1,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + FONT_SIZE_SMALL - 1) << 1,
                (glyphTop + FONT_SIZE_SMALL - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gDPSetTile(
                gfx++,
                G_IM_FMT_IA,
                G_IM_SIZ_4b,
                1,
                0,
                G_TX_RENDERTILE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOLOD
            );
            gDPSetTileSize(
                gfx++,
                G_TX_RENDERTILE,
                glyphLeft << G_TEXTURE_IMAGE_FRAC,
                glyphTop << G_TEXTURE_IMAGE_FRAC,
                (glyphLeft + FONT_SIZE_SMALL - 1) << G_TEXTURE_IMAGE_FRAC,
                (glyphTop + FONT_SIZE_SMALL - 1) << G_TEXTURE_IMAGE_FRAC
            );
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(left),
                QuarterPixels(top),
                QuarterPixels(left + FONT_SIZE_SMALL),
                QuarterPixels(top + FONT_SIZE_SMALL),
                G_TX_RENDERTILE,
                TexelCoordinate(glyphLeft),
                TexelCoordinate(glyphTop),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            text++;
            x += FONT_SIZE_SMALL;
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCenteredLargeText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
    Gfx *gfx = *displayListHead;

    DrawLargeText(
        &gfx,
        x - (MeasureText(text, FONT_SIZE_LARGE, MEASURE_TEXT_PROPORTIONAL) / 2),
        y,
        text
    );
    *displayListHead = gfx;
}

void DrawCenteredMediumText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
    Gfx *gfx = *displayListHead;

    DrawMediumText(
        &gfx,
        x - (MeasureText(text, FONT_SIZE_MEDIUM, MEASURE_TEXT_PROPORTIONAL) / 2),
        y,
        text
    );
    *displayListHead = gfx;
}

void DrawCenteredSmallText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
    Gfx *gfx = *displayListHead;

    DrawSmallText(
        &gfx,
        x - (MeasureText(text, FONT_SIZE_SMALL, MEASURE_TEXT_PROPORTIONAL) / 2),
        y,
        text
    );
    *displayListHead = gfx;
}

void DrawTextStub(
    Gfx **displayListHead UNUSED,
    s16 x UNUSED,
    s16 y UNUSED,
    READ_ONLY char *text UNUSED
) {
}

void SetUpShadowedTextDrawing(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gSPLoadGeometryMode(gfx++, 0);
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
        TextureAddress(ImageBody(g_ShadowedFontImage)),
        G_IM_FMT_IA,
        SHADOWED_FONT_IMAGE_WIDTH,
        SHADOWED_FONT_IMAGE_HEIGHT,
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

void SetTextColor(Gfx **displayListHead, u8 red, u8 green, u8 blue) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, red, green, blue, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawShadowedText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
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
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(x),
            QuarterPixels(y),
            QuarterPixels(x + SHADOWED_GLYPH_SIZE),
            QuarterPixels(y + SHADOWED_GLYPH_SIZE),
            G_TX_RENDERTILE,
            TexelCoordinate(glyphLeft),
            TexelCoordinate(glyphTop),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        if (*text == '.') {
            x += SHADOWED_POINT_INSET + SHADOWED_POINT_WIDTH;
        }
        else {
            x += SHADOWED_GLYPH_WIDTH;
        }

        text++;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCenteredShadowedText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
    Gfx *gfx = *displayListHead;

    s32 left = x;
    s16 right = x;
    s32 letterIndex;

    for (letterIndex = 0; text[letterIndex] != '\0'; letterIndex++) {
        if (text[letterIndex] == '.') {
            right += SHADOWED_POINT_WIDTH;
        }
        else {
            right += SHADOWED_GLYPH_WIDTH;
        }
    }

    DrawShadowedText(&gfx, left - ((right - left) / 2), y, text);
    *displayListHead = gfx;
}

void DrawCenteredNarrowText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
    Gfx *gfx = *displayListHead;
    s32 length;
    s32 code;
    s16 glyphLeft;

    gDPPipeSync(gfx++);
    length = 0;
    while (text[length] != '\0') {
        length++;
    }

    x -= length * NARROW_GLYPH_WIDTH / 2;
    while ((code = *text) != '\0') {
        glyphLeft = (code - NARROW_GLYPH_FIRST) * SHADOWED_GLYPH_SIZE;
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(x),
            QuarterPixels(y),
            QuarterPixels(x + NARROW_GLYPH_SIZE),
            QuarterPixels(y + SHADOWED_GLYPH_SIZE),
            G_TX_RENDERTILE,
            TexelCoordinate(glyphLeft),
            TexelCoordinate(NARROW_GLYPH_ROW * SHADOWED_GLYPH_SIZE),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        text++;
        x += NARROW_GLYPH_WIDTH;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCondensedShadowedText(Gfx **displayListHead, s16 x, s16 y, READ_ONLY char *text) {
    Gfx *gfx = *displayListHead;
    s32 code;
    s16 glyphLeft;
    s16 glyphTop;

    gDPPipeSync(gfx++);
    while ((code = *text) != '\0') {
        glyphLeft = GetShadowedGlyphLeft(code);
        glyphTop = GetShadowedGlyphTop(code);
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(x),
            QuarterPixels(y),
            QuarterPixels(x + SHADOWED_GLYPH_SIZE),
            QuarterPixels(y + SHADOWED_GLYPH_SIZE),
            G_TX_RENDERTILE,
            TexelCoordinate(glyphLeft),
            TexelCoordinate(glyphTop),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        text++;
        x += CONDENSED_GLYPH_WIDTH;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void ClearGlyphSprites(void) {
    s32 spriteIndex;

    for (spriteIndex = 0; spriteIndex != GLYPH_SPRITE_COUNT; spriteIndex++) {
        g_GlyphSprites[spriteIndex].group = NO_GLYPH_GROUP;
    }
}

void PlaceGlyphGroup(s32 group, s32 fontSize, TextFlags flags, s32 x, s32 y, READ_ONLY char *text) {
    s32 spriteIndex;
    s32 halfWidth;
    s32 advance;
    s32 placedCount;

    if (fontSize == NO_FONT_SIZE) {
        for (spriteIndex = 0; spriteIndex != GLYPH_SPRITE_COUNT; spriteIndex++) {
            if (g_GlyphSprites[spriteIndex].group == group) {
                g_GlyphSprites[spriteIndex].group = NO_GLYPH_GROUP;
            }
        }

        return;
    }

    halfWidth = 0;
    if (!(flags & GLYPH_FLAG_LEFT_ALIGNED)) {
        if (flags & TEXT_FLAG_FIXED_WIDTH) {
            halfWidth = MeasureText(text, fontSize, MEASURE_TEXT_FIXED_WIDTH) / 2;
        }
        else {
            halfWidth = MeasureText(text, fontSize, MEASURE_TEXT_PROPORTIONAL) / 2;
        }
    }

    g_GlyphGroupSizes[group] = GetTextLength(text);
    for (spriteIndex = 0; spriteIndex != GLYPH_SPRITE_COUNT; spriteIndex++) {
        if (g_GlyphSprites[spriteIndex].group == group) {
            g_GlyphSprites[spriteIndex].group = NO_GLYPH_GROUP;
        }
    }

    for (spriteIndex = 0, placedCount = 0, advance = 0;
        placedCount != g_GlyphGroupSizes[group] && spriteIndex != GLYPH_SPRITE_COUNT;
        spriteIndex++) {
        if (g_GlyphSprites[spriteIndex].group == NO_GLYPH_GROUP) {
            switch (text[placedCount]) {
                case CHARACTER_CODE_OTHER_MARK_FIRST:
                case CHARACTER_CODE_OTHER_MARK_FIRST + 1:
                case CHARACTER_CODE_MARK_FIRST:
                case CHARACTER_CODE_MARK_FIRST + 1:
                    if (fontSize == FONT_SIZE_LARGE) {
                        guTranslate(
                            &g_GlyphSprites[spriteIndex].matrix,
                            advance - halfWidth + x - LARGE_MARK_BACKSPACE,
                            y + LARGE_MARK_RISE,
                            0
                        );
                    }
                    else if (fontSize == FONT_SIZE_MEDIUM) {
                        guTranslate(
                            &g_GlyphSprites[spriteIndex].matrix,
                            advance - halfWidth + x - MEDIUM_MARK_BACKSPACE,
                            y + MEDIUM_MARK_RISE,
                            0
                        );
                    }
                    else {
                        guTranslate(
                            &g_GlyphSprites[spriteIndex].matrix,
                            advance - halfWidth + x,
                            y + SMALL_MARK_RISE,
                            0
                        );
                    }

                    break;
                default:
                    guTranslate(&g_GlyphSprites[spriteIndex].matrix, advance - halfWidth + x, y, 0);
                    if (fontSize == FONT_SIZE_LARGE) {
                        if (flags & TEXT_FLAG_FIXED_WIDTH) {
                            advance += FONT_SIZE_LARGE;
                        }
                        else {
                            advance += f_LargeFontWidths[text[placedCount]];
                        }
                    }
                    else {
                        advance += fontSize;
                    }

                    break;
            }

            g_GlyphSprites[spriteIndex].code = text[placedCount];
            g_GlyphSprites[spriteIndex].fontSize = fontSize;
            g_GlyphSprites[spriteIndex].flags = flags;
            g_GlyphSprites[spriteIndex].group = group;
            placedCount++;
        }
    }
}

s32 PlaceMessageGlyphGroup(s32 group, s32 fontSize, s32 x, s32 y, MessageId messageId) {
    s32 width;
    s32 height;
    s32 spriteIndex;
    s32 messageStart;
    s32 length;
    s32 codeIndex;
    s32 advance;
    s32 code;

    if (fontSize == NO_FONT_SIZE) {
        for (spriteIndex = 0; spriteIndex != GLYPH_SPRITE_COUNT; spriteIndex++) {
            if (g_GlyphSprites[spriteIndex].group == group) {
                g_GlyphSprites[spriteIndex].group = NO_GLYPH_GROUP;
            }
        }

        return UNSET_RESULT;
    }

    messageStart = MessageSpanAt(messageId)->start;
    length = MessageSpanAt(messageId)->length;
    MeasureMessage(messageId, fontSize, &width, &height, 0);
    width /= 2;
    height /= 2;
    y += height;
    g_GlyphGroupSizes[group] = length;
    for (spriteIndex = 0; spriteIndex != GLYPH_SPRITE_COUNT; spriteIndex++) {
        if (g_GlyphSprites[spriteIndex].group == group) {
            g_GlyphSprites[spriteIndex].group = NO_GLYPH_GROUP;
        }
    }

    codeIndex = 0;
    advance = 0;
    for (spriteIndex = 0; codeIndex != length; spriteIndex++) {
        if (g_GlyphSprites[spriteIndex].group == NO_GLYPH_GROUP) {
            code = ((u16 *)f_Messages)[g_MessageCodesStart + messageStart + codeIndex];
            if (code < MESSAGE_CODE_GLYPH) {
                if (code >= CHARACTER_CODE_MARK_FIRST &&
                    code < CHARACTER_CODE_MARK_FIRST + CHARACTER_CODE_MARK_COUNT) {
                    guTranslate(
                        &g_GlyphSprites[spriteIndex].matrix,
                        advance - width + x - LARGE_MARK_BACKSPACE,
                        y + MESSAGE_MARK_RISE,
                        0
                    );
                }
                else {
                    guTranslate(&g_GlyphSprites[spriteIndex].matrix, advance - width + x, y, 0);
                    advance += f_LargeFontWidths[code];
                }

                g_GlyphSprites[spriteIndex].code = code;
                g_GlyphSprites[spriteIndex].fontSize = fontSize;
                g_GlyphSprites[spriteIndex].group = group;
            }
            else if (code >= MESSAGE_CODE_CONTROL) {
                switch (code) {
                    case MESSAGE_CODE_LINE_BREAK:
                        advance = 0;
                        y -= LINE_SPACING + fontSize;
                        break;
                    case MESSAGE_CODE_A_BUTTON:
                    case MESSAGE_CODE_B_BUTTON:
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_A:
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_B:
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_C:
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_D:
                    case MESSAGE_CODE_Z_BUTTON:
                    case MESSAGE_CODE_C_UP:
                    case MESSAGE_CODE_C_DOWN:
                    case MESSAGE_CODE_C_LEFT:
                    case MESSAGE_CODE_C_RIGHT:
                    case MESSAGE_CODE_START_BUTTON:
                    case MESSAGE_CODE_L_BUTTON:
                    case MESSAGE_CODE_R_BUTTON:
                        guTranslate(&g_GlyphSprites[spriteIndex].matrix, advance - width + x, y, 0);
                        advance += fontSize;
                        g_GlyphSprites[spriteIndex].code = code;
                        g_GlyphSprites[spriteIndex].fontSize = fontSize;
                        g_GlyphSprites[spriteIndex].group = group;
                        break;
                }
            }

            codeIndex++;
        }
    }

    return PackHalfSize(width, height);
}

#define PackTextureCoordinates(s, t) ((TexelCoordinate(s) << 16) | TexelCoordinate(t))

void DrawGlyphGroup(Gfx **displayListHead, s32 group) {
    Gfx *gfx = *displayListHead;
    s32 spriteIndex;
    Boolean isIconLoaded;
    s32 iconOffset;
    u32 code;
    s32 fontSize;
    s32 s;
    s32 t;
    s32 left;
    s32 top;
    s32 bottom;

#ifdef AVOID_UB
    s = 0;
    t = 0;
#endif
    isIconLoaded = FALSE;
    gDPPipeSync(gfx++);
    for (spriteIndex = 0; spriteIndex != GLYPH_SPRITE_COUNT; spriteIndex++) {
        if (g_GlyphSprites[spriteIndex].group == group) {
            iconOffset = NO_ICON;
            if (isIconLoaded == TRUE) {
                gDPSetCombineLERP(
                    gfx++,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0
                );
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                isIconLoaded = FALSE;
            }

            code = g_GlyphSprites[spriteIndex].code;
            if (code < MESSAGE_CODE_GLYPH) {
                s = (code % FONT_GLYPHS_PER_ROW) * g_GlyphSprites[spriteIndex].fontSize;
                t = (code / FONT_GLYPHS_PER_ROW) * g_GlyphSprites[spriteIndex].fontSize;
                gDPPipeSync(gfx++);
                if (g_GlyphSprites[spriteIndex].fontSize == FONT_SIZE_LARGE) {
                    gDPSetTextureImage(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        FONT_SIZE_LARGE * FONT_GLYPHS_PER_ROW / 2,
                        TextureAddress(ImageBody(f_LargeFontImage))
                    );
                }
                else if (g_GlyphSprites[spriteIndex].fontSize == FONT_SIZE_MEDIUM) {
                    gDPSetTextureImage(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        FONT_SIZE_MEDIUM * FONT_GLYPHS_PER_ROW / 2,
                        TextureAddress(ImageBody(f_MediumFontImage))
                    );
                }
                else {
                    gDPSetTextureImage(
                        gfx++,
                        G_IM_FMT_I,
                        G_IM_SIZ_8b,
                        FONT_SIZE_SMALL * FONT_GLYPHS_PER_ROW / 2,
                        TextureAddress(ImageBody(f_SmallFontImage))
                    );
                }

                gDPSetTile(
                    gfx++,
                    G_IM_FMT_I,
                    G_IM_SIZ_8b,
                    1,
                    0,
                    G_TX_LOADTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPLoadSync(gfx++);
                gDPLoadTile(
                    gfx++,
                    G_TX_LOADTILE,
                    s << 1,
                    t << G_TEXTURE_IMAGE_FRAC,
                    (s + GLYPH_TILE_WIDTH - 1) << 1,
                    (t + FONT_SIZE_LARGE - 1) << G_TEXTURE_IMAGE_FRAC
                );
                gDPPipeSync(gfx++);
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_I,
                    G_IM_SIZ_4b,
                    1,
                    0,
                    G_TX_RENDERTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPSetTileSize(
                    gfx++,
                    G_TX_RENDERTILE,
                    s << G_TEXTURE_IMAGE_FRAC,
                    t << G_TEXTURE_IMAGE_FRAC,
                    (s + GLYPH_TILE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                    (t + GLYPH_TILE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC
                );
            }
            else if (code >= MESSAGE_CODE_CONTROL) {
                switch (code) {
                    case MESSAGE_CODE_A_BUTTON:
                        iconOffset = TEXT_ICON_A_BUTTON * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_B_BUTTON:
                        iconOffset = TEXT_ICON_B_BUTTON * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_A:
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_B:
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_C:
                    case MESSAGE_CODE_UNUSED_Z_BUTTON_D:
                    case MESSAGE_CODE_Z_BUTTON:
                        iconOffset = TEXT_ICON_Z_BUTTON * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_C_UP:
                        iconOffset = TEXT_ICON_C_UP * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_C_DOWN:
                        iconOffset = TEXT_ICON_C_DOWN * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_C_LEFT:
                        iconOffset = TEXT_ICON_C_LEFT * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_C_RIGHT:
                        iconOffset = TEXT_ICON_C_RIGHT * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_START_BUTTON:
                        iconOffset = TEXT_ICON_START_BUTTON * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_L_BUTTON:
                        iconOffset = TEXT_ICON_L_BUTTON * ICON_TEXEL_COUNT;
                        break;
                    case MESSAGE_CODE_R_BUTTON:
                        iconOffset = TEXT_ICON_R_BUTTON * ICON_TEXEL_COUNT;
                        break;
                }

                if (isIconLoaded == FALSE) {
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_TextIconImage)));
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    isIconLoaded = TRUE;
                }

                gDPSetTextureImage(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    ICON_WIDTH,
                    TextureAddress(LargeImageTexels(f_TextIconImage) + iconOffset)
                );
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    2,
                    0,
                    G_TX_LOADTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPLoadSync(gfx++);
                gDPLoadTile(
                    gfx++,
                    G_TX_LOADTILE,
                    0,
                    0,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC
                );
                gDPPipeSync(gfx++);
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    2,
                    0,
                    G_TX_RENDERTILE,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD
                );
                gDPSetTileSize(
                    gfx++,
                    G_TX_RENDERTILE,
                    0,
                    0,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                    (ICON_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC
                );
                s = 0;
                t = 0;
            }

            gDPPipeSync(gfx++);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&g_GlyphSprites[spriteIndex].matrix),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            fontSize = g_GlyphSprites[spriteIndex].fontSize;
            top = 0;
            left = 0;
            bottom = fontSize;
            if (fontSize == FONT_SIZE_LARGE) {
                if (g_GlyphSprites[spriteIndex].flags & GLYPH_FLAG_TOP_PART) {
                    gSPVertex(gfx++, f_LargeGlyphTopVertices, QUAD_VERTEX_COUNT, 0);
                    bottom = LARGE_GLYPH_PART_ROW;
                }
                else if (g_GlyphSprites[spriteIndex].flags & GLYPH_FLAG_BOTTOM_PART) {
                    gSPVertex(gfx++, f_LargeGlyphBottomVertices, QUAD_VERTEX_COUNT, 0);
                    bottom = g_GlyphSprites[spriteIndex].fontSize;
                    top = LARGE_GLYPH_PART_ROW;
                }
                else {
                    gSPVertex(gfx++, f_LargeGlyphVertices, QUAD_VERTEX_COUNT, 0);
                }
            }
            else if (fontSize == FONT_SIZE_MEDIUM) {
                gSPVertex(gfx++, f_MediumGlyphVertices, QUAD_VERTEX_COUNT, 0);
            }
            else {
                gSPVertex(gfx++, f_SmallGlyphVertices, QUAD_VERTEX_COUNT, 0);
            }

            gSPModifyVertex(gfx++, 0, G_MWO_POINT_ST, PackTextureCoordinates(left + s, top + t));
            gSPModifyVertex(gfx++, 1, G_MWO_POINT_ST, PackTextureCoordinates(left + s, bottom + t));
            gSPModifyVertex(
                gfx++,
                2,
                G_MWO_POINT_ST,
                PackTextureCoordinates(fontSize + s, bottom + t)
            );
            gSPModifyVertex(
                gfx++,
                3,
                G_MWO_POINT_ST,
                PackTextureCoordinates(fontSize + s, top + t)
            );
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gDPPipeSync(gfx++);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void SetUpPerspectiveTextDrawing(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
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
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        TEXEL0,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        TEXEL0
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

s32 GetDigitGlyphTop(char code) {
    switch (code) {
        case ':':
            return DIGIT_GLYPH_COLON * DIGIT_GLYPH_SIZE;
        case '*':
            return DIGIT_GLYPH_TIMES * DIGIT_GLYPH_SIZE;
        case '+':
            return DIGIT_GLYPH_PLUS * DIGIT_GLYPH_SIZE;
        case '-':
            return DIGIT_GLYPH_MINUS * DIGIT_GLYPH_SIZE;
        case '#':
            return DIGIT_GLYPH_PLUS_MINUS * DIGIT_GLYPH_SIZE;
        case '!':
            return DIGIT_GLYPH_CIRCLE * DIGIT_GLYPH_SIZE;
        default:
            return (code - '0' + DIGIT_GLYPH_ZERO) * DIGIT_GLYPH_SIZE;
    }
}

const s32 g_UnusedTextData[2] = {0, 0};

void DrawDigitText(Gfx **displayListHead, s32 x, s32 y, READ_ONLY char *text) {
    Gfx *gfx = *displayListHead;
    s32 index;

    gDPPipeSync(gfx++);
#ifdef AVOID_UB
    for (index = 0; text[index] != '\0'; index++) {
#else
    for (index = 0; g_PrintBuffer[index] != '\0'; index++) {
#endif
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(x),
            QuarterPixels(y),
            QuarterPixels(x + DIGIT_GLYPH_SIZE),
            QuarterPixels(y + DIGIT_GLYPH_SIZE),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(GetDigitGlyphTop(text[index])),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        x += DIGIT_GLYPH_WIDTH;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawLargeTextWithShadow(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    READ_ONLY char *text,
    s32 envColor,
    s32 primColor,
    TextFlags flags
) {
    Gfx *gfx = *displayListHead;
    u8 env[RGB_CHANNEL_COUNT];
    u8 prim[RGB_CHANNEL_COUNT];

    env[0] = envColor >> 24;
    env[1] = envColor >> 16;
    env[2] = envColor >> 8;
    prim[0] = primColor >> 24;
    prim[1] = primColor >> 16;
    prim[2] = primColor >> 8;
    gDPPipeSync(gfx++);
    gDPSetEnvColor(
        gfx++,
        env[0] >> SHADOW_DIMMING_SHIFT,
        env[1] >> SHADOW_DIMMING_SHIFT,
        env[2] >> SHADOW_DIMMING_SHIFT,
        OPAQUE_ALPHA
    );
    gDPSetPrimColor(
        gfx++,
        0,
        0,
        prim[0] >> SHADOW_DIMMING_SHIFT,
        prim[1] >> SHADOW_DIMMING_SHIFT,
        prim[2] >> SHADOW_DIMMING_SHIFT,
        OPAQUE_ALPHA
    );
    if (flags & TEXT_FLAG_CENTERED) {
        DrawCenteredLargeText(&gfx, x + SHADOW_OFFSET, y + SHADOW_OFFSET, text);
    }
    else if (flags & TEXT_FLAG_FIXED_WIDTH) {
        DrawFixedWidthLargeText(&gfx, x + SHADOW_OFFSET, y + SHADOW_OFFSET, text);
    }
    else {
        DrawLargeText(&gfx, x + SHADOW_OFFSET, y + SHADOW_OFFSET, text);
    }

    gDPSetEnvColor(gfx++, env[0], env[1], env[2], OPAQUE_ALPHA);
    gDPSetPrimColor(gfx++, 0, 0, prim[0], prim[1], prim[2], OPAQUE_ALPHA);
    if (flags & TEXT_FLAG_CENTERED) {
        DrawCenteredLargeText(&gfx, x, y, text);
    }
    else if (flags & TEXT_FLAG_FIXED_WIDTH) {
        DrawFixedWidthLargeText(&gfx, x, y, text);
    }
    else {
        DrawLargeText(&gfx, x, y, text);
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
