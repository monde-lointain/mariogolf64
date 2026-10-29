/*
 * The help board that the menus show: a chalkboard listing the topics, with a
 * hand and arrows, and each topic's pages of text.
 */
#include "common.h"

#include "help_board.h"

#include <lib/libnusys/nusys.h>

#include "asset.h"
#include "hole_display.h"
#include "math_util.h"
#include "menu.h"
#include "menu_memory.h"
#include "screen.h"
#include "sound.h"
#include "text.h"

#define ASSET_HELP_BOARD_CHALKBOARD 0x833

/* The help board lists its topics, the play modes and the record terms, this
 * many at a time */
#define HELP_TOPIC_COUNT 17

#define HAND_SIZE 24
#define HAND_LEFT 70
/* How far the hand swings right and back */
#define HAND_SWING 8
/* A second swing like the hand's, which nothing is drawn with */
#define UNUSED_SWING 8
#define UNUSED_SWING_STEP 2
#define ARROW_COUNT 2
/* A 16x16 arrow takes four bits a texel */
#define ARROW_IMAGE_SIZE (ARROW_SIZE * ARROW_SIZE / 2)
/* The arrow image holds each animation frame this many bytes apart */
#define ARROW_FRAME_SIZE 0x200
#define ARROW_UP 0
#define ARROW_DOWN 1
#define ARROW_SIZE 16
#define ARROW_LEFT 152
#define ARROW_UP_TOP 54
#define ARROW_DOWN_TOP 200
/* The arrows change frame every this many frames */
#define ARROW_FRAME_TIME 4
#define LIST_LEFT 160

#define CHALKBOARD_PALETTE_SIZE (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)
#define CHALKBOARD_TEXELS_OFFSET (IMAGE_HEADER_SIZE + CHALKBOARD_PALETTE_SIZE)
#define CHALKBOARD_WIDTH 256
#define CHALKBOARD_STRIP_HEIGHT 8
#define CHALKBOARD_STRIP_COUNT 20
/* Each eight-row strip is drawn nine rows tall */
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
  /* 0x0 */ s32 top;
  /* 0x4 */ s32 rowSpacing;
} TextListLayout;

typedef struct TopicPageTable {
  /* 0x00 */ u8 unread[11];
  /* 0x0B */ s8 counts[HELP_TOPIC_COUNT + 1];
} TopicPageTable; /* size = 0x1D */

extern s32 D_800FE334;
static void* f_HandImages;
static void* f_ArrowImages;
static void* f_UnusedImage;
static void* f_ChalkboardImage;
/* The open topic, counted from 1, or 0 while the list shows */
static s8 f_OpenTopic = 0;
static s8 f_TopicPage = 0;
static s32 f_HandOffset = 0;
static Boolean f_IsHandReturning = FALSE;
static s32 f_ArrowFrame = 0;
static s32 f_ArrowFrameTime = 0;
static Boolean f_IsUnusedSwingReturning = FALSE;
static s32 f_UnusedSwingOffset = 0;
static s32 f_FirstListedTopic = 0;

/*
 * Indexed by topic; each topic's pages are the messages that follow its title.
 * The code reads the counts at their own address, 11 bytes into the table.
 */
static TopicPageTable f_TopicPages = {
    {0x11, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
    {1, 4, 4, 4, 4, 3, 4, 5, 4, 4, 3, 5, 4, 4, 4, 4, 3, 6}};
/* The list's first message, then each topic's title */
static s16 f_TopicTitleMessages[HELP_TOPIC_COUNT + 1] = {0x0D6, 0x0E7, 0x0EC,
    0x0F1, 0x0F6, 0x0FB, 0x0FF, 0x104, 0x10A, 0x10F, 0x114, 0x118, 0x11E, 0x123,
    0x128, 0x12D, 0x139, 0x132};
static f32 f_ListColorPhase = 0.0f;
static TextListLayout f_HelpListLayout = {84, 22};
/* Read by no routine */
static TextListLayout D_800C4350[8] = {{86, 18}, {108, 32}, {98, 24}, {104, 24},
    {106, 32}, {98, 21}, {80, 20}, {0, 0}};

void func_801FB54C(Gfx** displayListHead, s32);
void DrawChalkboard(Gfx** displayListHead);

/* Reads one of the help board's images into buffer */
void LoadHelpBoardImage(void* buffer, s32 image) {
  EncodedAsset asset;
  MenuMemory* memory = func_8005AF74();

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
  } else if (memory->helpTopicIndex + LISTED_TOPIC_COUNT - 1 <
      HELP_TOPIC_COUNT - 1) {
    f_FirstListedTopic = memory->helpTopicIndex;
  } else {
    f_FirstListedTopic = HELP_TOPIC_COUNT - LISTED_TOPIC_COUNT;
  }

  ClearTextSlots();
}

/* Clears every text slot of the help board */
static inline void ClearHelpText(void) {
  s32 index;

  for (index = 0; index != HELP_TEXT_SLOT_COUNT; index++) {
    SetTextSlot(0, 0, NO_MESSAGE, 1, index, 1, 0xFF, 0xFF, 0xFF, 0xFF);
  }
}

static inline void SetHelpTextColors(void) {
  SetTextPaletteColor(0xFF, 0xFF, 0, 0);
  SetTextPaletteColor(0xFF, 0xFF, 0, 1);
  SetTextPaletteColor(0xFF, 0x7F, 0x7F, 2);
  SetTextPaletteColor(0x3F, 0xFF, 0x3F, 3);
}

/* Shows the open topic's title and its current page */
static inline void ShowTopicPage(void) {
  SetTextSlot(
      0, 0, NO_MESSAGE, 1, HELP_TEXT_SLOT_TITLE, 1, 0xFF, 0xFF, 0xFF, 0xFF);
  SetTextSlot(
      0, 0, NO_MESSAGE, 1, HELP_TEXT_SLOT_PAGE, 1, 0xFF, 0xFF, 0xFF, 0xFF);
  SetHelpTextColors();
  SetTextSlot(TOPIC_TITLE_X, TOPIC_TITLE_Y, f_TopicTitleMessages[f_OpenTopic],
      1, HELP_TEXT_SLOT_TITLE, 2, 0xFF, 0xFF, 0xFF, 0xFF);
  SetHelpTextColors();
  SetTextSlot(TOPIC_PAGE_X, TOPIC_PAGE_Y,
      f_TopicTitleMessages[f_OpenTopic] + f_TopicPage, 1, HELP_TEXT_SLOT_PAGE,
      3, 0xFF, 0xFF, 0xFF, 0xFF);
}

/* Opens and pages through topics with A, closes them or leaves with B, and
 * moves through the list */
void UpdateHelpBoard(void) {
  s32 index;
  MenuMemory* memory = func_8005AF74();

  if (IsHudAnimationDone() != 1) {
    return;
  }

  if (g_MenuRepeatButtons & A_BUTTON) {
    if (f_OpenTopic == 0) {
      f_OpenTopic = memory->helpTopicIndex + 1;
      ClearHelpText();
      PlaySound(0x5D, 0xF, 0x50);
      f_TopicPage++;
      ShowTopicPage();
    } else if (f_TopicPage < f_TopicPages.counts[f_OpenTopic]) {
      if (IsTextSlotRevealed(HELP_TEXT_SLOT_PAGE)) {
        PlaySound(0x5D, 0xF, 0x50);
        f_TopicPage++;
        ShowTopicPage();
      } else {
        RevealTextSlot(HELP_TEXT_SLOT_TITLE);
        RevealTextSlot(HELP_TEXT_SLOT_PAGE);
      }
    } else if (IsTextSlotRevealed(HELP_TEXT_SLOT_PAGE)) {
      ClearHelpText();
      f_OpenTopic = 0;
      f_TopicPage = 0;
    } else {
      RevealTextSlot(HELP_TEXT_SLOT_TITLE);
      RevealTextSlot(HELP_TEXT_SLOT_PAGE);
    }
  } else if (g_MenuRepeatButtons & B_BUTTON) {
    PlaySound(0x5F, 0xF, SOUND_FULL_SCALE);
    if (f_OpenTopic <= 0) {
      StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
      if (memory->unk46 == 0) {
        g_MenuScreen = 0x1C;
      } else {
        g_MenuScreen = 2;
      }

      StopLoaderThread();
      FreeAllCharacterSlots();
      D_800FE334 = 0;
      f_OpenTopic = 0;
      ClearHelpText();
    } else if (IsTextSlotRevealed(HELP_TEXT_SLOT_TITLE) &&
        IsTextSlotRevealed(HELP_TEXT_SLOT_PAGE)) {
      f_OpenTopic = 0;
      f_TopicPage = 0;
      ClearHelpText();
    } else {
      RevealTextSlot(HELP_TEXT_SLOT_TITLE);
      RevealTextSlot(HELP_TEXT_SLOT_PAGE);
    }
  } else if (g_MenuRepeatButtons & U_JPAD) {
    if (f_OpenTopic == 0) {
      PlaySound(0x5C, 0xF, 0x64);
      if (f_FirstListedTopic == memory->helpTopicIndex) {
        if (f_FirstListedTopic != 0) {
          f_FirstListedTopic--;
          memory->helpTopicIndex--;
        }
      } else {
        memory->helpTopicIndex--;
      }

      ClearHelpText();
    }
  } else if (g_MenuRepeatButtons & D_JPAD) {
    if (f_OpenTopic == 0) {
      PlaySound(0x5C, 0xF, 0x64);
      if (f_FirstListedTopic + LISTED_TOPIC_COUNT - 1 ==
          memory->helpTopicIndex) {
        if (f_FirstListedTopic + LISTED_TOPIC_COUNT - 1 <
            HELP_TOPIC_COUNT - 1) {
          f_FirstListedTopic++;
          memory->helpTopicIndex++;
        }
      } else {
        memory->helpTopicIndex++;
      }

      ClearHelpText();
    }
  }
}

/*
 * Whether a topic list arrow is shown: up while topics are above the
 * list, down while some are below. It is a macro, as an inline function
 * compiles differently from the original.
 */
#define IsTopicArrowShown(index) \
  ((((index) == ARROW_UP) && (f_FirstListedTopic > 0)) || \
      (((index) == ARROW_DOWN) && \
          (f_FirstListedTopic < HELP_TOPIC_COUNT - LISTED_TOPIC_COUNT)))

/* Draws the chalkboard, the topic list with its hand and arrows, and the text
 */
void DrawHelpBoard(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = func_8005AF74();
  s32 index;
  s32 handTop;
  s32 handLeft;
  s32 arrowTop;
  s32 arrowLeft;
  s32 red;
  s32 green;
  s32 blue;
  s32 topic;

  func_801FB54C(&gfx, 0xB);
  DrawChalkboard(&gfx);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
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
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)f_HandImages + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureTile_4b(gfx++,
        ((u32)f_HandImages + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
        HAND_SIZE, 0, 0, 0, HAND_SIZE - 1, HAND_SIZE - 1, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    if (f_IsHandReturning == FALSE) {
      f_HandOffset++;
      if (f_HandOffset == HAND_SWING) {
        f_IsHandReturning = TRUE;
      }
    } else if (f_IsHandReturning == TRUE) {
      f_HandOffset--;
      if (f_HandOffset == 0) {
        f_IsHandReturning = FALSE;
      }
    } else {
      f_HandOffset = 0;
      f_IsHandReturning = FALSE;
    }

    handLeft = f_HandOffset + HAND_LEFT;
    handTop = f_HelpListLayout.top +
        ((memory->helpTopicIndex - f_FirstListedTopic) *
            f_HelpListLayout.rowSpacing);
    gSPScisTextureRectangle(gfx++, handLeft << 2, handTop << 2,
        (handLeft + HAND_SIZE) << 2, (handTop + HAND_SIZE) << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    f_ArrowFrameTime++;
    if (f_ArrowFrameTime == ARROW_FRAME_TIME) {
      f_ArrowFrameTime = 0;
      f_ArrowFrame ^= 1;
    }

    if (f_IsUnusedSwingReturning == FALSE) {
      f_UnusedSwingOffset += UNUSED_SWING_STEP;
      if (f_UnusedSwingOffset == UNUSED_SWING) {
        f_IsUnusedSwingReturning = TRUE;
      }
    } else if (f_IsUnusedSwingReturning == TRUE) {
      f_UnusedSwingOffset -= UNUSED_SWING_STEP;
      if (f_UnusedSwingOffset == 0) {
        f_IsUnusedSwingReturning = FALSE;
      }
    } else {
      f_UnusedSwingOffset = 0;
      f_IsUnusedSwingReturning = FALSE;
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)f_ArrowImages + IMAGE_HEADER_SIZE) & ~7);
    for (index = 0; index != ARROW_COUNT; index++) {
      if (IsTopicArrowShown(index)) {
        gDPLoadTextureTile_4b(gfx++,
            (u32)((u8*)f_ArrowImages +
                (index * ARROW_IMAGE_SIZE + PALETTED_TEXELS_OFFSET +
                    f_ArrowFrame * ARROW_FRAME_SIZE)) &
                ~7,
            G_IM_FMT_CI, ARROW_SIZE, 0, 0, 0, ARROW_SIZE - 1, ARROW_SIZE - 1, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        arrowLeft = ARROW_LEFT;
        arrowTop = ARROW_DOWN_TOP;
        if (index == ARROW_UP) {
          arrowTop = ARROW_UP_TOP;
        }

        gSPScisTextureRectangle(gfx++, arrowLeft << 2, arrowTop << 2,
            (arrowLeft + ARROW_SIZE) << 2, (arrowTop + ARROW_SIZE) << 2,
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
      }
    }
  }

  gDPPipeSync(gfx++);
  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  if (f_OpenTopic == 0) {
    for (index = 0, topic = f_FirstListedTopic; index != LISTED_TOPIC_COUNT;
        index++, topic++) {
      f_ListColorPhase += DEGREES_TO_RADIANS(5);
      red = (s32)(sinf(f_ListColorPhase) * 127.0f) + 127;
      blue = (s32)(sinf(f_ListColorPhase + (M_PI / 2)) * 127.0f) + 127;
      green = (s32)(sinf(f_ListColorPhase + M_PI) * 127.0f) + 127;
      if (topic == memory->helpTopicIndex) {
        SetTextSlot(LIST_LEFT,
            f_HelpListLayout.top + (index * f_HelpListLayout.rowSpacing),
            f_TopicTitleMessages[0] + topic, 1, index, 0x10, red, green, blue,
            0xFF);
      } else {
        SetTextSlot(LIST_LEFT,
            f_HelpListLayout.top + (index * f_HelpListLayout.rowSpacing),
            f_TopicTitleMessages[f_OpenTopic] + topic, 1, index, 0x10, 0xFF,
            0xFF, 0xFF, 0xFF);
      }
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws a tinted intensity image across the board; nothing calls this or loads
 * its image */
void DrawUnusedHelpBoardImage(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

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
  gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0,
      TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0);
  gDPLoadTextureBlock_4b(gfx++, ((u32)f_UnusedImage + IMAGE_HEADER_SIZE) & ~7,
      G_IM_FMT_I, UNUSED_IMAGE_WIDTH, UNUSED_IMAGE_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, UNUSED_IMAGE_LEFT << 2, UNUSED_IMAGE_TOP << 2,
      UNUSED_IMAGE_RIGHT << 2, UNUSED_IMAGE_BOTTOM << 2, G_TX_RENDERTILE, 0, 0,
      (UNUSED_IMAGE_WIDTH << 10) / (UNUSED_IMAGE_RIGHT - UNUSED_IMAGE_LEFT),
      (UNUSED_IMAGE_HEIGHT << 10) / (UNUSED_IMAGE_BOTTOM - UNUSED_IMAGE_TOP));
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws the chalkboard in strips, stretching each strip a little taller */
void DrawChalkboard(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripTop;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF | AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL,
      G_RM_OPA_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPLoadTLUT_pal256(gfx++, ((u32)f_ChalkboardImage + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != CHALKBOARD_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)f_ChalkboardImage + CHALKBOARD_TEXELS_OFFSET +
            (stripIndex * CHALKBOARD_WIDTH * CHALKBOARD_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, CHALKBOARD_WIDTH, CHALKBOARD_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = CHALKBOARD_TOP + (stripIndex * CHALKBOARD_DRAWN_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++, CHALKBOARD_LEFT << 2, stripTop << 2,
        (CHALKBOARD_LEFT + CHALKBOARD_WIDTH) << 2,
        (stripTop + CHALKBOARD_DRAWN_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE,
        (CHALKBOARD_STRIP_HEIGHT << 10) / CHALKBOARD_DRAWN_STRIP_HEIGHT);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
