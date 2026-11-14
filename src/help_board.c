#include "help_board.h"

#include <nusys.h>
#include <ultra64.h>

#include "asset.h"
#include "c_language.h"
#include "character_model.h"
#include "compiler.h"
#include "gbi_helpers.h"
#include "hole_loader.h"
#include "main_menu.h"
#include "math_util.h"
#include "memory_address.h"
#include "menu.h"
#include "menu_memory.h"
#include "screen.h"
#include "sky_and_hud_animations.h"
#include "sound.h"
#include "text.h"

#define ASSET_HELP_BOARD_CHALKBOARD 0x833

#define HELP_TOPIC_COUNT 17

#define HAND_LEFT 70
#define HAND_SWING 8
#define UNUSED_SWING 8
#define UNUSED_SWING_STEP 2
#define TOPIC_ARROW_COUNT 2
#define ARROW_LEFT 152
#define ARROW_UP_TOP 54
#define ARROW_DOWN_TOP 200
#define ARROW_FRAME_DURATION 4
#define LIST_LEFT 160

#define CHALKBOARD_PALETTE_SIZE (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)
#define CHALKBOARD_TEXELS_OFFSET (IMAGE_HEADER_SIZE + CHALKBOARD_PALETTE_SIZE)
#define CHALKBOARD_WIDTH 256
#define CHALKBOARD_STRIP_HEIGHT 8
#define CHALKBOARD_STRIP_COUNT 20
#define CHALKBOARD_DRAWN_STRIP_HEIGHT 9
#define CHALKBOARD_LEFT 36
#define CHALKBOARD_TOP 40

#define UNUSED_IMAGE_WIDTH 128
#define UNUSED_IMAGE_HEIGHT 64
#define UNUSED_IMAGE_LEFT 40
#define UNUSED_IMAGE_TOP 50
#define UNUSED_IMAGE_RIGHT 280
#define UNUSED_IMAGE_BOTTOM 230

#define HELP_TEXT_SLOT_COUNT 7
#define HELP_TEXT_SLOT_TITLE 0
#define HELP_TEXT_SLOT_PAGE 1
#define TOPIC_TITLE_X 48
#define TOPIC_TITLE_Y 72
#define TOPIC_PAGE_X 52
#define TOPIC_PAGE_Y 105
#define LISTED_TOPIC_COUNT 5

typedef struct TextListLayout {
    s32 top;
    s32 rowSpacing;
} TextListLayout;

typedef struct TopicPageTable {
    u8 unread[11];
    s8 counts[HELP_TOPIC_COUNT + 1];
} TopicPageTable;

static ImageAsset *f_HandImages;
static ImageAsset *f_ArrowImages;
static ImageAsset *f_UnusedImage;
static ImageAsset *f_ChalkboardImage;
static s8 f_OpenTopic = 0;
static s8 f_TopicPage = 0;
static s32 f_HandOffset = 0;
static Boolean f_IsHandReturning = FALSE;
static s32 f_ArrowFrame = 0;
static s32 f_ArrowFrameTime = 0;
static Boolean f_IsUnusedSwingReturning = FALSE;
static s32 f_UnusedSwingOffset = 0;
static s32 f_FirstListedTopic = 0;

static TopicPageTable f_TopicPages = {
    {0x11, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
    {1, 4, 4, 4, 4, 3, 4, 5, 4, 4, 3, 5, 4, 4, 4, 4, 3, 6},
};
static MessageId16 f_TopicTitleMessages[HELP_TOPIC_COUNT + 1] = {
    0x0D6,
    0x0E7,
    0x0EC,
    0x0F1,
    0x0F6,
    0x0FB,
    0x0FF,
    0x104,
    0x10A,
    0x10F,
    0x114,
    0x118,
    0x11E,
    0x123,
    0x128,
    0x12D,
    0x139,
    0x132,
};
static f32 f_ListColorPhase = 0.0f;
static TextListLayout f_HelpListLayout = {84, 22};
static TextListLayout f_UnusedTextListLayouts[8] UNUSED =
    {{86, 18}, {108, 32}, {98, 24}, {104, 24}, {106, 32}, {98, 21}, {80, 20}, {0, 0}};

void DrawChalkboard(Gfx **displayListHead);

void LoadHelpBoardImage(void *buffer, HelpBoardImage image) {
    EncodedAsset asset;
    MenuMemory *memory = GetMenuMemory();

    switch (image) {
        case HELP_BOARD_IMAGE_HAND:
            f_HandImages = buffer;
            OpenEncodedAsset(ASSET_POINTING_HAND, &asset);
            ReadEncodedAsset(f_HandImages, &asset);
            break;
        case HELP_BOARD_IMAGE_ARROWS:
            f_ArrowImages = buffer;
            OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
            ReadEncodedAsset(f_ArrowImages, &asset);
            break;
        case HELP_BOARD_IMAGE_CHALKBOARD:
            f_ChalkboardImage = buffer;
            OpenEncodedAsset(ASSET_HELP_BOARD_CHALKBOARD, &asset);
            ReadEncodedAsset(f_ChalkboardImage, &asset);
            break;
    }

    f_OpenTopic = 0;
    f_TopicPage = 0;
    f_HandOffset = 0;
    f_IsHandReturning = FALSE;
    f_ArrowFrame = 0;
    f_ArrowFrameTime = 0;
    f_IsUnusedSwingReturning = FALSE;
    f_UnusedSwingOffset = 0;
    if (memory->helpTopicIndex < LISTED_TOPIC_COUNT) {
        f_FirstListedTopic = 0;
    }
    else if (memory->helpTopicIndex + LISTED_TOPIC_COUNT - 1 < HELP_TOPIC_COUNT - 1) {
        f_FirstListedTopic = memory->helpTopicIndex;
    }
    else {
        f_FirstListedTopic = HELP_TOPIC_COUNT - LISTED_TOPIC_COUNT;
    }

    ClearTextSlots();
}

static inline void ClearHelpText(void) {
    s32 index;

    for (index = 0; index != HELP_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(
            0,
            0,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            index,
            TEXT_FLAG_TYPED,
            0xFF,
            0xFF,
            0xFF,
            OPAQUE_ALPHA
        );
    }
}

static inline void SetHelpTextColors(void) {
    SetTextPaletteColor(0xFF, 0xFF, 0, 0);
    SetTextPaletteColor(0xFF, 0xFF, 0, 1);
    SetTextPaletteColor(0xFF, 0x7F, 0x7F, 2);
    SetTextPaletteColor(0x3F, 0xFF, 0x3F, 3);
}

static inline void ShowTopicPage(void) {
    SetTextSlot(
        0,
        0,
        NO_MESSAGE,
        TEXT_SLOT_LARGE_FONT,
        HELP_TEXT_SLOT_TITLE,
        TEXT_FLAG_TYPED,
        0xFF,
        0xFF,
        0xFF,
        OPAQUE_ALPHA
    );
    SetTextSlot(
        0,
        0,
        NO_MESSAGE,
        TEXT_SLOT_LARGE_FONT,
        HELP_TEXT_SLOT_PAGE,
        TEXT_FLAG_TYPED,
        0xFF,
        0xFF,
        0xFF,
        OPAQUE_ALPHA
    );
    SetHelpTextColors();
    SetTextSlot(
        TOPIC_TITLE_X,
        TOPIC_TITLE_Y,
        f_TopicTitleMessages[f_OpenTopic],
        TEXT_SLOT_LARGE_FONT,
        HELP_TEXT_SLOT_TITLE,
        TEXT_FLAG_TYPING_SOUND,
        0xFF,
        0xFF,
        0xFF,
        OPAQUE_ALPHA
    );
    SetHelpTextColors();
    SetTextSlot(
        TOPIC_PAGE_X,
        TOPIC_PAGE_Y,
        f_TopicTitleMessages[f_OpenTopic] + f_TopicPage,
        TEXT_SLOT_LARGE_FONT,
        HELP_TEXT_SLOT_PAGE,
        TEXT_FLAG_TYPED | TEXT_FLAG_TYPING_SOUND,
        0xFF,
        0xFF,
        0xFF,
        OPAQUE_ALPHA
    );
}

void UpdateHelpBoard(void) {
    MenuMemory *memory = GetMenuMemory();

    if (IsHudAnimationDone() != 1) {
        return;
    }

    if (g_MenuRepeatButtons & A_BUTTON) {
        if (f_OpenTopic == 0) {
            f_OpenTopic = memory->helpTopicIndex + 1;
            ClearHelpText();
            PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            f_TopicPage++;
            ShowTopicPage();
        }
        else if (f_TopicPage < f_TopicPages.counts[f_OpenTopic]) {
            if (IsTextSlotRevealed(HELP_TEXT_SLOT_PAGE)) {
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                f_TopicPage++;
                ShowTopicPage();
            }
            else {
                RevealTextSlot(HELP_TEXT_SLOT_TITLE);
                RevealTextSlot(HELP_TEXT_SLOT_PAGE);
            }
        }
        else if (IsTextSlotRevealed(HELP_TEXT_SLOT_PAGE)) {
            ClearHelpText();
            f_OpenTopic = 0;
            f_TopicPage = 0;
        }
        else {
            RevealTextSlot(HELP_TEXT_SLOT_TITLE);
            RevealTextSlot(HELP_TEXT_SLOT_PAGE);
        }
    }
    else if (g_MenuRepeatButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        if (f_OpenTopic <= 0) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            if (!memory->isTransmitShown) {
                g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
            }
            else {
                g_MenuScreen = MENU_SCREEN_MAIN;
            }

            StopLoaderThread();
            FreeAllCharacterSlots();
            g_IsChosenHoleLoadStarted = FALSE;
            f_OpenTopic = 0;
            ClearHelpText();
        }
        else if (IsTextSlotRevealed(HELP_TEXT_SLOT_TITLE) &&
            IsTextSlotRevealed(HELP_TEXT_SLOT_PAGE)) {
            f_OpenTopic = 0;
            f_TopicPage = 0;
            ClearHelpText();
        }
        else {
            RevealTextSlot(HELP_TEXT_SLOT_TITLE);
            RevealTextSlot(HELP_TEXT_SLOT_PAGE);
        }
    }
    else if (g_MenuRepeatButtons & U_JPAD) {
        if (f_OpenTopic == 0) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (f_FirstListedTopic == memory->helpTopicIndex) {
                if (f_FirstListedTopic != 0) {
                    f_FirstListedTopic--;
                    memory->helpTopicIndex--;
                }
            }
            else {
                memory->helpTopicIndex--;
            }

            ClearHelpText();
        }
    }
    else if (g_MenuRepeatButtons & D_JPAD) {
        if (f_OpenTopic == 0) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (f_FirstListedTopic + LISTED_TOPIC_COUNT - 1 == memory->helpTopicIndex) {
                if (f_FirstListedTopic + LISTED_TOPIC_COUNT - 1 < HELP_TOPIC_COUNT - 1) {
                    f_FirstListedTopic++;
                    memory->helpTopicIndex++;
                }
            }
            else {
                memory->helpTopicIndex++;
            }

            ClearHelpText();
        }
    }
}

#define IsTopicArrowShown(index) \
    ((((index) == MENU_ARROW_UP) && (f_FirstListedTopic > 0)) || \
        (((index) == MENU_ARROW_DOWN) && \
            (f_FirstListedTopic < HELP_TOPIC_COUNT - LISTED_TOPIC_COUNT)))

void DrawHelpBoard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    s32 index;
    s32 handTop;
    s32 handLeft;
    s32 arrowTop;
    s32 arrowLeft;
    s32 red;
    s32 green;
    s32 blue;
    s32 topic;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    DrawChalkboard(&gfx);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    if (f_OpenTopic == 0) {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_HandImages)));
        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(ImageTexels(f_HandImages)),
            G_IM_FMT_CI,
            POINTING_HAND_SIZE,
            0,
            0,
            0,
            POINTING_HAND_SIZE - 1,
            POINTING_HAND_SIZE - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        if (f_IsHandReturning == FALSE) {
            f_HandOffset++;
            if (f_HandOffset == HAND_SWING) {
                f_IsHandReturning = TRUE;
            }
        }
        else if (f_IsHandReturning == TRUE) {
            f_HandOffset--;
            if (f_HandOffset == 0) {
                f_IsHandReturning = FALSE;
            }
        }
        else {
            f_HandOffset = 0;
            f_IsHandReturning = FALSE;
        }

        handLeft = f_HandOffset + HAND_LEFT;
        handTop = f_HelpListLayout.top +
            ((memory->helpTopicIndex - f_FirstListedTopic) * f_HelpListLayout.rowSpacing);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(handLeft),
            QuarterPixels(handTop),
            QuarterPixels(handLeft + POINTING_HAND_SIZE),
            QuarterPixels(handTop + POINTING_HAND_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
        f_ArrowFrameTime++;
        if (f_ArrowFrameTime == ARROW_FRAME_DURATION) {
            f_ArrowFrameTime = 0;
            f_ArrowFrame ^= 1;
        }

        if (f_IsUnusedSwingReturning == FALSE) {
            f_UnusedSwingOffset += UNUSED_SWING_STEP;
            if (f_UnusedSwingOffset == UNUSED_SWING) {
                f_IsUnusedSwingReturning = TRUE;
            }
        }
        else if (f_IsUnusedSwingReturning == TRUE) {
            f_UnusedSwingOffset -= UNUSED_SWING_STEP;
            if (f_UnusedSwingOffset == 0) {
                f_IsUnusedSwingReturning = FALSE;
            }
        }
        else {
            f_UnusedSwingOffset = 0;
            f_IsUnusedSwingReturning = FALSE;
        }

        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_ArrowImages)));
        for (index = 0; index != TOPIC_ARROW_COUNT; index++) {
            if (IsTopicArrowShown(index)) {
                gDPLoadTextureTile_4b(
                    gfx++,
                    TextureAddress(
                        (u8 *)f_ArrowImages +
                        (index * GREEN_ARROW_IMAGE_SIZE + PALETTED_TEXELS_OFFSET +
                            f_ArrowFrame * GREEN_ARROW_FRAME_SIZE)
                    ),
                    G_IM_FMT_CI,
                    GREEN_ARROW_SIZE,
                    0,
                    0,
                    0,
                    GREEN_ARROW_SIZE - 1,
                    GREEN_ARROW_SIZE - 1,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                arrowLeft = ARROW_LEFT;
                arrowTop = ARROW_DOWN_TOP;
                if (index == MENU_ARROW_UP) {
                    arrowTop = ARROW_UP_TOP;
                }

                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(arrowLeft),
                    QuarterPixels(arrowTop),
                    QuarterPixels(arrowLeft + GREEN_ARROW_SIZE),
                    QuarterPixels(arrowTop + GREEN_ARROW_SIZE),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
            }
        }
    }

    gDPPipeSync(gfx++);
    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    if (f_OpenTopic == 0) {
        for (index = 0, topic = f_FirstListedTopic; index != LISTED_TOPIC_COUNT; index++, topic++) {
            f_ListColorPhase += DegreesToRadians(5);
            red = (s32)(sinf(f_ListColorPhase) * 127.0f) + 127;
            blue = (s32)(sinf(f_ListColorPhase + (M_PI / 2)) * 127.0f) + 127;
            green = (s32)(sinf(f_ListColorPhase + M_PI) * 127.0f) + 127;
            if (topic == memory->helpTopicIndex) {
                SetTextSlot(
                    LIST_LEFT,
                    f_HelpListLayout.top + (index * f_HelpListLayout.rowSpacing),
                    f_TopicTitleMessages[0] + topic,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    TEXT_FLAG_CENTERED,
                    red,
                    green,
                    blue,
                    OPAQUE_ALPHA
                );
            }
            else {
                SetTextSlot(
                    LIST_LEFT,
                    f_HelpListLayout.top + (index * f_HelpListLayout.rowSpacing),
                    f_TopicTitleMessages[f_OpenTopic] + topic,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    TEXT_FLAG_CENTERED,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawUnusedHelpBoardImage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 220, 230, 210);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_UnusedImage)),
        G_IM_FMT_I,
        UNUSED_IMAGE_WIDTH,
        UNUSED_IMAGE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(UNUSED_IMAGE_LEFT),
        QuarterPixels(UNUSED_IMAGE_TOP),
        QuarterPixels(UNUSED_IMAGE_RIGHT),
        QuarterPixels(UNUSED_IMAGE_BOTTOM),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(UNUSED_IMAGE_WIDTH, UNUSED_IMAGE_RIGHT - UNUSED_IMAGE_LEFT),
        TexelRunStep(UNUSED_IMAGE_HEIGHT, UNUSED_IMAGE_BOTTOM - UNUSED_IMAGE_TOP)
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawChalkboard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF | AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_ChalkboardImage)));
    for (stripIndex = 0; stripIndex != CHALKBOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                AddressPast(f_ChalkboardImage, CHALKBOARD_TEXELS_OFFSET) +
                (stripIndex * CHALKBOARD_WIDTH * CHALKBOARD_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            CHALKBOARD_WIDTH,
            CHALKBOARD_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = CHALKBOARD_TOP + (stripIndex * CHALKBOARD_DRAWN_STRIP_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(CHALKBOARD_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(CHALKBOARD_LEFT + CHALKBOARD_WIDTH),
            QuarterPixels(stripTop + CHALKBOARD_DRAWN_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TexelRunStep(CHALKBOARD_STRIP_HEIGHT, CHALKBOARD_DRAWN_STRIP_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}
