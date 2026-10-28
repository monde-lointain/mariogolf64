/*
 * Two of the menus' screens: the clubhouse's Ring Shot records, which list each
 * course's records and saved replays, and the saved games to continue, on
 * numbered boards with each game's mode, course and players.
 */
#include "common.h"

#include "asset.h"
#include "character.h"
#include "character_slot.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "functions.h"
#include "game.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_memory.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "sram.h"
#include "text.h"
#include "variables.h"

/*
 * The saved game's 64 by 64 course image, shrunk in two strips; func_80226190
 * draws one with its own palette, func_8022644C one without
 */
#define COURSE_THUMBNAIL_WIDTH 64
#define COURSE_THUMBNAIL_STRIP_HEIGHT 32
#define COURSE_THUMBNAIL_STRIP_COUNT 2
#define COURSE_THUMBNAIL_LEFT 40
#define COURSE_THUMBNAIL_TOP 182
#define COURSE_THUMBNAIL_SHOWN_WIDTH 52
#define COURSE_THUMBNAIL_SHOWN_HEIGHT 18
/* Controllers numbered 1 to 4, 32 by 32 each */
#define ASSET_CONTROLLER_ICONS 0x63D
/* The wooden boards numbered 1 to 3, one for each saved game, 64 by 64 each */
#define ASSET_FIRST_GAME_BOARD 0x82A
#define ASSET_SECOND_GAME_BOARD 0x82B
#define ASSET_THIRD_GAME_BOARD 0x82C

/* The games that func_80222CDC continues from the save's mode tables, by the
 * save menu's slot */
enum SaveMenuGame {
  SAVE_MENU_GAME_TOURNAMENT,
  SAVE_MENU_GAME_GET_CHARACTER,
  SAVE_MENU_GAME_STROKE
};

/* The image listing the game modes, in orange */
#define ASSET_MENU_GAME_MODES 0x528

extern s32 D_80230A90_ovl_01;
extern s32 D_80230A94_ovl_01;
extern s32 D_80230A98_ovl_01;

/* The slots that the save menu steps through, which hold -1 when empty */
#define SAVE_MENU_SLOT_COUNT 5

/* The most bytes of each palette file that func_80220BB0 keeps */
#define SAVE_MENU_FILE_SIZE 0x200

/* The images of g_CourseIconAssets that func_8021F6E0 reads, before one of its
 * own */
#define SAVE_MENU_IMAGE_COUNT 6
/* The 64x64 CI8 icons of D_8023B680, which func_802213C8 shrinks to 32x24 in
 * two strips */
#define SAVE_ICON_WIDTH 64
#define SAVE_ICON_STRIP_HEIGHT 32
#define SAVE_ICON_STRIP_COUNT 2
#define SAVE_ICON_SHOWN_WIDTH 32
#define SAVE_ICON_SHOWN_STRIP_HEIGHT 12

/* The game modes that have an image of their own, in image order, then mini
 * golf's */
#define SAVE_MENU_MODE_COUNT 10
#define SAVE_MENU_MODE_MINI_GOLF 10

extern const char D_80231A3C_ovl_01[];

void func_80221CC0_ovl_01(void);

/* The Ring Shot stars that each level of func_8022181C takes */
#define RING_SHOT_STARS_PER_LEVEL 4

/* What func_80221EBC reads for the chosen game, and the saved games' frame */
#define SAVE_MENU_CHOSEN_IMAGE_COUNT 2
#define ASSET_SAVE_MENU_FRAME 0x70D
/*
 * D_8023B678 holds the digits 1 to 6, 16 wide each, followed by the word
 * COURSE. func_80220E34 draws the word, then the course's digit overlapping its
 * end.
 */
#define COURSE_LABEL_WIDTH 136
#define COURSE_LABEL_HEIGHT 16
#define COURSE_DIGIT_WIDTH 16
#define COURSE_WORD_WIDTH 40
#define COURSE_WORD_OFFSET_X -5
#define COURSE_DIGIT_OFFSET_X 33

/* The image of each game mode that a saved game can be in */
typedef struct {
  /* 0x0 */ s16 unk0;
  /* 0x2 */ u16 assetId;
} SaveMenuModeImage; /* size = 0x4 */

/* The Ring Shot records' text slots */
#define RING_SHOT_TEXT_SLOT_COUNT 6
/* The row after the courses, for the saved Ring Shot replays */
#define RING_SHOT_REPLAY_ROW RING_SHOT_COURSE_COUNT
/* The furthest a course's list of characters scrolls */
#define RING_SHOT_LAST_FIRST_CHARACTER 14

extern s8 g_PlayerControllerIndices[MAX_PLAYER_COUNT];
extern u16 g_CourseIconAssets[];
extern u16 g_MiniGolfCourseIconAssets[];
extern s16 g_MiniGolfDiscFiles[];
extern void* D_801052C4[2];
extern u8 D_80230B4E_ovl_01;
extern void* D_8012D430[CHARACTER_COUNT];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern s32 D_8012F480[MAX_PLAYER_COUNT];
extern u8 D_801B70F0[MAXCONTROLLERS];

/* What D_80230B40 holds for a player with no controller, or for the computer */
#define NO_CONTROLLER -1
#define COMPUTER_CONTROLLER MAXCONTROLLERS

extern s32
    D_8023BFC0_ovl_01[MAX_PLAYER_COUNT]; /* Frames until each character fidgets,
                                            or -1 while it does */

extern f32 D_80230D18_ovl_01[3];
extern Vec3 D_80230D28_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT];
extern s16 D_80230B40_ovl_01[MAX_PLAYER_COUNT];
extern s16 D_80230B48_ovl_01;
extern s16 D_80230B4A_ovl_01;
extern u8 D_80230B4C_ovl_01;
extern u8 D_80230B4D_ovl_01;
extern s32 D_80230B50_ovl_01[SAVED_GAME_COUNT];
extern Unk68 D_800FEDD8[MAX_PLAYER_COUNT];
extern s32 D_80230B88_ovl_01[SAVE_MENU_MODE_COUNT];
extern SaveMenuModeImage D_80230B5C_ovl_01[];
/* The mode icon of each save menu mode, mini golf last */
extern s32 D_80230BB0_ovl_01[SAVE_MENU_MODE_MINI_GOLF + 1];
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern u8 D_8023BFF0_ovl_01[CHARACTER_COUNT][SAVE_MENU_FILE_SIZE];
extern u8 D_801052E8;
extern s32 D_800FE334;

s16 func_80220D68_ovl_01(s8 startSlot, u8 isForward);
s16 func_80220DF8_ovl_01(void);
s32 func_8022181C_ovl_01(void);
void func_80210C74_ovl_01(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void func_802123E0_ovl_01(Gfx** displayListHead, s32 x, s32 y, u8 sign);
void func_80212C54_ovl_01(Gfx** displayListHead, void* image, s32* blinkFrame,
    s32* blinkTimer, s32 width, s32 centerX, s32 centerY);
void func_80212FEC_ovl_01(Gfx** displayListHead, void* image, s32 blinkFrame,
    s32 direction, s32 centerX, s32 centerY);
/* func_8021F124 takes a u8 drawMode, but this file calls it without narrowing
 * the argument */
void func_8021F124_ovl_01(Gfx** displayListHead, void* texels, void* palette,
    s32 left, s32 top, s16 character, s32 drawMode);
void func_80220E34_ovl_01(Gfx** displayListHead, s32 courseIndex, s32 left,
    s32 top, u8 showsCourseWord);
void func_802213C8_ovl_01(
    Gfx** displayListHead, s32 imageIndex, s32 left, s32 top);

static void* D_8023B670_ovl_01;
static void* D_8023B674_ovl_01;
static void* D_8023B678_ovl_01;
static void* D_8023B67C_ovl_01;
static void** D_8023B680_ovl_01;
static s16 D_8023B684_ovl_01[SAVE_MENU_SLOT_COUNT];

/* Read by no routine */
static u8 D_8023B690_ovl_01[0x3D0];

static Mat4 D_8023BA60_ovl_01;
static Mat4 D_8023BAA0_ovl_01;
static Mtx D_8023BAE0_ovl_01[MAX_PLAYER_COUNT][2];
static Mtx D_8023BCE0_ovl_01;
static f32 D_8023BD20_ovl_01[MAX_PLAYER_COUNT]; /* Each character's scale */
static LookAt D_8023BD30_ovl_01[2];
static void** D_8023BD70_ovl_01;
static void** D_8023BD74_ovl_01;
static void* D_8023BD78_ovl_01;
static void* D_8023BD7C_ovl_01;
static void* D_8023BD80_ovl_01;
static void* D_8023BD84_ovl_01;

/* Read by no routine */
static u8 D_8023BD88_ovl_01[0x4];

static void* D_8023BD8C_ovl_01;
static void* D_8023BD90_ovl_01;

/* Read by no routine */
static u8 D_8023BD94_ovl_01[0x4];

static void* D_8023BD98_ovl_01;
static void* D_8023BD9C_ovl_01;
static void* D_8023BDA0_ovl_01;
static s16 D_8023BDA4_ovl_01[MAX_PLAYER_COUNT];
static s32 D_8023BDAC_ovl_01;
static s32 D_8023BDB0_ovl_01;

/* Read by no routine; 2 objects, as one would be aligned past its start */
static u8 D_8023BDB4_ovl_01[0x4];
static u8 D_8023BDB8_ovl_01[0x88];

/* Not static: the original left it to the linker's common block */
u8 D_8023E3F0_ovl_01[SAVE_MENU_SLOT_COUNT][SAVE_MENU_FILE_SIZE];

void func_8021F5F0_ovl_01(void* buffer, s16 bufferKind) {
  EncodedAsset asset;

  switch (bufferKind) {
    case 1:
      D_8023B670_ovl_01 = buffer;
      break;
    case 2:
      D_8023B674_ovl_01 = buffer;
      OpenEncodedAsset(0x5F7, &asset);
      ReadEncodedAsset(D_8023B674_ovl_01, &asset);
      break;
    case 3:
      D_8023B678_ovl_01 = buffer;
      OpenEncodedAsset(0x70E, &asset);
      ReadEncodedAsset(D_8023B678_ovl_01, &asset);
      break;
    case 4:
      D_8023B67C_ovl_01 = buffer;
      OpenEncodedAsset(0x720, &asset);
      ReadEncodedAsset(D_8023B67C_ovl_01, &asset);
      D_80230A90_ovl_01 = 0;
      D_80230A94_ovl_01 = 0;
      D_80230A98_ovl_01 = 0;
      break;
  }
}

void func_8021F6E0_ovl_01(void** buffers) {
  EncodedAsset asset;
  s16 imageIndex;

  D_8023B680_ovl_01 = buffers;
  for (imageIndex = 0; imageIndex != SAVE_MENU_IMAGE_COUNT; imageIndex++) {
    OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
    ReadEncodedAsset(D_8023B680_ovl_01[imageIndex], &asset);
  }

  OpenEncodedAsset(0x944, &asset);
  ReadEncodedAsset(D_8023B680_ovl_01[SAVE_MENU_IMAGE_COUNT], &asset);
}

void func_8021F79C_ovl_01(void) {
  MenuMemory* memory = func_8005AF74();
  SaveData* save = GetSaveData();
  s32 openCourseCount = func_8022181C_ovl_01();
  s32 index;
  s32 movedSlot;

  if (openCourseCount <= 0) {
    openCourseCount = 1;
  }

  if (openCourseCount > RING_SHOT_COURSE_COUNT) {
    openCourseCount = RING_SHOT_COURSE_COUNT;
  }

  if (IsHudAnimationDone() == TRUE) {
    if (GetWipeKind() != WIPE_NONE && D_801052E8 == 0) {
      if (GetWipeProgress() == 0) {
        for (index = 0; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        g_MenuScreen = MENU_SCREEN_EXIT;
        g_GameState.unk31 = 2;
        SetGameFlag(GAME_FLAG_UNK1E);
      }
    } else if (GetWipeKind() == WIPE_NONE || D_801052E8 != 1) {
      if (g_MenuPressedButtons & A_BUTTON) {
        if (memory->ringShotRow == RING_SHOT_REPLAY_ROW &&
            func_80220DF8_ovl_01() != -1) {
          PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
          index = memory->ringShotReplaySlot;
          memory->holeIndex = save->unk1DF0[D_8023B684_ovl_01[index]].unk35;
          memory->courseIndex = save->unk1DF0[D_8023B684_ovl_01[index]].unk34;
          D_800FE334 = 0;
          LoadWipeShape((s8)save->unk1DF0[index].unk34);
          StartWipe(WIPE_CLOSING);
          D_801052E8 = 0;
        }
      } else if (g_MenuPressedButtons & B_BUTTON) {
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
        for (index = 0; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, index, 1, 0xFF, 0xFF, 0xFF, 0xFF);
        }
      } else if (g_MenuRepeatButtons & L_JPAD) {
        for (index = 1; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (memory->ringShotRow == RING_SHOT_REPLAY_ROW) {
          memory->ringShotRow = openCourseCount - 1;
          return;
        }

        memory->ringShotRow--;
        if (memory->ringShotRow < 0) {
          memory->ringShotRow = RING_SHOT_REPLAY_ROW;
        }
      } else if (g_MenuRepeatButtons & R_JPAD) {
        for (index = 1; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        memory->ringShotRow++;
        if (memory->ringShotRow > RING_SHOT_REPLAY_ROW) {
          memory->ringShotRow = 0;
          return;
        }

        if (memory->ringShotRow > openCourseCount - 1) {
          memory->ringShotRow = RING_SHOT_REPLAY_ROW;
        }
      } else if (g_MenuRepeatButtons & U_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        index = memory->ringShotRow;
        if (index == RING_SHOT_REPLAY_ROW) {
          if (memory->ringShotReplaySlot > 0) {
            movedSlot = func_80220D68_ovl_01(memory->ringShotReplaySlot, FALSE);
            if (movedSlot != -1) {
              memory->ringShotReplaySlot = movedSlot;
            }

            if (memory->ringShotReplaySlot < 0) {
              memory->ringShotReplaySlot = SAVE_MENU_SLOT_COUNT - 1;
            }
          }
        } else if (memory->ringShotFirstCharacters[index] > 0) {
          memory->ringShotFirstCharacters[index]--;
        }
      } else if (g_MenuRepeatButtons & D_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        index = memory->ringShotRow;
        if (index == RING_SHOT_REPLAY_ROW) {
          if (memory->ringShotReplaySlot < SAVE_MENU_SLOT_COUNT - 1) {
            movedSlot = func_80220D68_ovl_01(memory->ringShotReplaySlot, TRUE);
            if (movedSlot != -1) {
              memory->ringShotReplaySlot = movedSlot;
            }

            if (memory->ringShotReplaySlot >= SAVE_MENU_SLOT_COUNT) {
              memory->ringShotReplaySlot = 0;
            }
          }
        } else if (memory->ringShotFirstCharacters[index] <
            RING_SHOT_LAST_FIRST_CHARACTER) {
          memory->ringShotFirstCharacters[index]++;
        }
      }
    }
  }
}

/*
 * func_8021FC5C draws the Ring Shot records. A row of icons, one for each open
 * course and one for the saved replays, sits above a shaded panel, with the
 * blinking arrows around the chosen one. For a course the panel shows each
 * hole's number over a column of four characters from ringShotFirstCharacters
 * on, crossed out and darkened where the hole's unk1344 entry for them is 0,
 * with arrows where the list scrolls further. For the replays it shows each
 * slot's character face, course and a message for its course and hole, or
 * dashes where it is empty, and the chosen slot's message glows in shifting
 * colors. Message 0x1E plus the row is shown below the panel.
 */
#define RING_SHOT_BOARD_KIND 11
#define RING_SHOT_PANEL_LEFT 24
#define RING_SHOT_PANEL_TOP 70
#define RING_SHOT_PANEL_RIGHT 302
#define RING_SHOT_PANEL_BOTTOM 202
#define RING_SHOT_STRIP_WIDTH 200
#define RING_SHOT_STRIP_HEIGHT 10
#define RING_SHOT_STRIP_COUNT 3
#define RING_SHOT_STRIP_LEFT 28
#define RING_SHOT_STRIP_RIGHT 298
#define RING_SHOT_STRIP_TOP 204
#define RING_SHOT_STRIP_SPACING 8
#define RING_SHOT_ROW_COUNT (RING_SHOT_REPLAY_ROW + 1)
/* Each row's icon is drawn this many times over in the same place */
#define RING_SHOT_ICON_DRAW_COUNT 3
#define RING_SHOT_FIRST_ICON_X 22
#define RING_SHOT_REPLAY_ICON_X 268
#define RING_SHOT_ICON_SPACING 42
#define RING_SHOT_ICON_OFFSET_X 18
#define RING_SHOT_ICON_TOP 48
#define RING_SHOT_CHOSEN_ICON_DROP 8
#define RING_SHOT_ARROW_WIDTH 20
#define RING_SHOT_FIRST_ARROW_X 40
#define RING_SHOT_REPLAY_ARROW_X 284
#define RING_SHOT_ARROW_OFFSET_X 36
#define RING_SHOT_ARROW_Y 60
#define RING_SHOT_COLUMN_SPACING 42
#define RING_SHOT_HOLE_NUMBER_X 48
#define RING_SHOT_HOLE_NUMBER_Y 76
#define RING_SHOT_SHOWN_CHARACTER_COUNT 4
#define RING_SHOT_FACE_X 46
#define RING_SHOT_FACE_Y 94
#define RING_SHOT_FACE_SPACING 26
#define RING_SHOT_CROSS_X 52
#define RING_SHOT_CROSS_Y 100
/* The scroll arrows are drawn in this many columns */
#define RING_SHOT_SCROLL_COLUMN_COUNT 1
#define RING_SHOT_SCROLL_ARROW_X 36
#define RING_SHOT_UP_ARROW_Y 100
#define RING_SHOT_DOWN_ARROW_Y 190
#define RING_SHOT_SLOT_SPACING 25
#define RING_SHOT_REPLAY_FACE_X 44
#define RING_SHOT_REPLAY_FACE_Y 74
#define RING_SHOT_REPLAY_COURSE_X 76
#define RING_SHOT_REPLAY_COURSE_Y 82
#define RING_SHOT_EMPTY_DASH_COUNT 13
#define RING_SHOT_EMPTY_DASH_X 52
#define RING_SHOT_EMPTY_DASH_SPACING 18
#define RING_SHOT_EMPTY_DASH_Y 80
#define RING_SHOT_SLOT_NUMBER_X 26
#define RING_SHOT_SLOT_NUMBER_Y 82
#define RING_SHOT_TITLE_X 36
#define RING_SHOT_TITLE_Y 211
#define MESSAGE_FIRST_RING_SHOT_TITLE 0x1E
#define RING_SHOT_REPLAY_TEXT_X 124
#define RING_SHOT_REPLAY_TEXT_Y 84
#define MESSAGE_FIRST_RING_SHOT_HOLE 0x4B
#define RING_SHOT_TEXT_FONT 1
#define RING_SHOT_REPLAY_TEXT_FONT 2
/* The chosen replay's colors run around a circle of hues, a step per update,
 * with its glow a quarter as bright */
#define REPLAY_HUE_STEP DEGREES_TO_RADIANS(5)
#define REPLAY_HUE_MIDDLE 127
#define REPLAY_HUE_SWING 127.0f
#define REPLAY_GLOW_DIVISOR 4

/* The order that the Ring Shot records list the characters in */
extern u8 D_80230AA0_ovl_01[SAVE_UNK1344_ENTRY_COUNT];
/* Each open course count's icon positions, of which only being nonzero, for
 * a shown icon, is read */
extern s16 D_80230AE4_ovl_01[RING_SHOT_COURSE_COUNT][RING_SHOT_ROW_COUNT];
extern f32 D_80230AE0_ovl_01;

/* Doesn't match: the original gives several values other saved registers,
 * and keeps memory on the stack */
#ifdef NON_MATCHING
void func_8021FC5C_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  /* Counts the strips, holds each face's draw mode and counts the replay
   * slots, as the original's shared register needs */
  s16 index = 0;
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s32 openCourseCount = func_8022181C_ovl_01();
  s16 row;
  s16 column;
  /* Counts the faces shown in a column, or an empty slot's dashes */
  s16 itemIndex;
  s16 listIndex;
  s16 drawIndex;
  s16 drop;
  s32 character;
  s8 replayCharacter;
  s32 red;
  s32 green;
  s32 blue;
  s32 redSwing;
  s32 greenSwing;
  s32 blueSwing;
  s16 replayCourse;
  s16 replayHole;

  func_801FB54C_ovl_01(&gfx, RING_SHOT_BOARD_KIND);

  gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, RING_SHOT_PANEL_LEFT << 2,
      RING_SHOT_PANEL_TOP << 2, RING_SHOT_PANEL_RIGHT << 2,
      RING_SHOT_PANEL_BOTTOM << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);

  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023B670_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  do {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023B670_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            (index * RING_SHOT_STRIP_WIDTH * RING_SHOT_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, RING_SHOT_STRIP_WIDTH, RING_SHOT_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, RING_SHOT_STRIP_LEFT << 2,
        (RING_SHOT_STRIP_TOP + (index * RING_SHOT_STRIP_SPACING)) << 2,
        RING_SHOT_STRIP_RIGHT << 2,
        (RING_SHOT_STRIP_TOP + RING_SHOT_STRIP_SPACING +
            (index * RING_SHOT_STRIP_SPACING))
            << 2,
        G_TX_RENDERTILE, 0, 0,
        (RING_SHOT_STRIP_WIDTH << 10) /
            (RING_SHOT_STRIP_RIGHT - RING_SHOT_STRIP_LEFT),
        (RING_SHOT_STRIP_HEIGHT << 10) / RING_SHOT_STRIP_SPACING);
    gDPPipeSync(gfx++);
    index++;
  } while (index != RING_SHOT_STRIP_COUNT);

  if (openCourseCount <= 0) {
    openCourseCount = 1;
  }

  if (openCourseCount > RING_SHOT_COURSE_COUNT) {
    openCourseCount = RING_SHOT_COURSE_COUNT;
  }

  for (row = 0; row != RING_SHOT_ROW_COUNT; row++) {
    drop = (row == memory->ringShotRow) * RING_SHOT_CHOSEN_ICON_DROP;
    if ((row < openCourseCount) || (row == RING_SHOT_REPLAY_ROW)) {
      if (D_80230AE4_ovl_01[openCourseCount - 1][row] != 0) {
        for (drawIndex = 0; drawIndex != RING_SHOT_ICON_DRAW_COUNT;
            drawIndex++) {
          if (row == 0) {
            func_802213C8_ovl_01(
                &gfx, 0, RING_SHOT_FIRST_ICON_X, RING_SHOT_ICON_TOP + drop);
          } else if (row == RING_SHOT_REPLAY_ROW) {
            func_802213C8_ovl_01(&gfx, RING_SHOT_REPLAY_ROW,
                RING_SHOT_REPLAY_ICON_X, RING_SHOT_ICON_TOP + drop);
          } else {
            func_802213C8_ovl_01(&gfx, row,
                (row * RING_SHOT_ICON_SPACING) + RING_SHOT_ICON_OFFSET_X,
                RING_SHOT_ICON_TOP + drop);
          }

          gDPPipeSync(gfx++);
        }
      }
    }

    if (row == memory->ringShotRow) {
      if (row == 0) {
        func_80212C54_ovl_01(&gfx, D_8023B67C_ovl_01, &D_80230A90_ovl_01,
            &D_80230A94_ovl_01, RING_SHOT_ARROW_WIDTH, RING_SHOT_FIRST_ARROW_X,
            RING_SHOT_ARROW_Y);
      } else if (row == RING_SHOT_REPLAY_ROW) {
        func_80212C54_ovl_01(&gfx, D_8023B67C_ovl_01, &D_80230A90_ovl_01,
            &D_80230A94_ovl_01, RING_SHOT_ARROW_WIDTH, RING_SHOT_REPLAY_ARROW_X,
            RING_SHOT_ARROW_Y);
      } else {
        func_80212C54_ovl_01(&gfx, D_8023B67C_ovl_01, &D_80230A90_ovl_01,
            &D_80230A94_ovl_01, RING_SHOT_ARROW_WIDTH,
            (row * RING_SHOT_ICON_SPACING) + RING_SHOT_ARROW_OFFSET_X,
            RING_SHOT_ARROW_Y);
      }
    }
  }

  if (memory->ringShotRow != RING_SHOT_REPLAY_ROW) {
    for (column = 0; column != RING_SHOT_HOLE_COUNT; column++) {
      func_80210C74_ovl_01(&gfx,
          RING_SHOT_HOLE_NUMBER_X + (column * RING_SHOT_COLUMN_SPACING),
          RING_SHOT_HOLE_NUMBER_Y, column + 1, MENU_NUMBER_PLAIN);

      row = memory->ringShotRow;
      listIndex = memory->ringShotFirstCharacters[row];
      for (itemIndex = 0; itemIndex != RING_SHOT_SHOWN_CHARACTER_COUNT;
          itemIndex++) {
        character = D_80230AA0_ovl_01[listIndex];
        index = FACE_PLAIN;
        if (save->unk1344[row][column][character] == 0) {
          index = FACE_DARKENED;
        }

        func_8021F124_ovl_01(&gfx, D_8012D430[character],
            D_8023BFF0_ovl_01[character],
            RING_SHOT_FACE_X + (column * RING_SHOT_COLUMN_SPACING),
            RING_SHOT_FACE_Y + (itemIndex * RING_SHOT_FACE_SPACING), character,
            index);
        if (index == FACE_DARKENED) {
          func_802123E0_ovl_01(&gfx,
              RING_SHOT_CROSS_X + (column * RING_SHOT_COLUMN_SPACING),
              RING_SHOT_CROSS_Y + (itemIndex * RING_SHOT_FACE_SPACING),
              MENU_SIGN_TIMES);
        }

        listIndex++;
      }
    }

    for (column = 0; column != RING_SHOT_SCROLL_COLUMN_COUNT; column++) {
      if (memory->ringShotFirstCharacters[memory->ringShotRow] > 0) {
        func_80212FEC_ovl_01(&gfx, D_8023B67C_ovl_01, D_80230A90_ovl_01,
            MENU_ARROW_UP,
            RING_SHOT_SCROLL_ARROW_X + (column * RING_SHOT_COLUMN_SPACING),
            RING_SHOT_UP_ARROW_Y);
      }

      if (memory->ringShotFirstCharacters[memory->ringShotRow] <
          RING_SHOT_LAST_FIRST_CHARACTER) {
        func_80212FEC_ovl_01(&gfx, D_8023B67C_ovl_01, D_80230A90_ovl_01,
            MENU_ARROW_DOWN,
            RING_SHOT_SCROLL_ARROW_X + (column * RING_SHOT_COLUMN_SPACING),
            RING_SHOT_DOWN_ARROW_Y);
      }
    }
  } else {
    for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
      if (D_8023B684_ovl_01[index] != -1) {
        replayCharacter = save->unk1DF0[D_8023B684_ovl_01[index]].unk37;
        func_8021F124_ovl_01(&gfx, D_8012D430[replayCharacter],
            D_8023E3F0_ovl_01[index], RING_SHOT_REPLAY_FACE_X,
            RING_SHOT_REPLAY_FACE_Y + (index * RING_SHOT_SLOT_SPACING),
            replayCharacter, FACE_PLAIN);
        func_80220E34_ovl_01(&gfx,
            (s8)save->unk1DF0[D_8023B684_ovl_01[index]].unk34,
            RING_SHOT_REPLAY_COURSE_X,
            RING_SHOT_REPLAY_COURSE_Y + (index * RING_SHOT_SLOT_SPACING), TRUE);
      } else {
        for (itemIndex = 0; itemIndex != RING_SHOT_EMPTY_DASH_COUNT;
            itemIndex++) {
          func_802123E0_ovl_01(&gfx,
              RING_SHOT_EMPTY_DASH_X +
                  (itemIndex * RING_SHOT_EMPTY_DASH_SPACING),
              RING_SHOT_EMPTY_DASH_Y + (index * RING_SHOT_SLOT_SPACING),
              MENU_SIGN_DASH);
        }
      }

      func_80210C74_ovl_01(&gfx, RING_SHOT_SLOT_NUMBER_X,
          RING_SHOT_SLOT_NUMBER_Y + (index * RING_SHOT_SLOT_SPACING), index + 1,
          MENU_NUMBER_PLAIN);
    }
  }

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  SetTextSlot(RING_SHOT_TITLE_X, RING_SHOT_TITLE_Y,
      MESSAGE_FIRST_RING_SHOT_TITLE + memory->ringShotRow, RING_SHOT_TEXT_FONT,
      0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
  if (memory->ringShotRow == RING_SHOT_REPLAY_ROW) {
    for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
      if (D_8023B684_ovl_01[index] != -1) {
        D_80230AE0_ovl_01 += REPLAY_HUE_STEP;

        redSwing = sinf(D_80230AE0_ovl_01) * REPLAY_HUE_SWING;
        blueSwing = sinf(D_80230AE0_ovl_01 + (M_PI / 2)) * REPLAY_HUE_SWING;
        red = redSwing + REPLAY_HUE_MIDDLE;
        blue = blueSwing + REPLAY_HUE_MIDDLE;
        greenSwing = sinf(D_80230AE0_ovl_01 + M_PI) * REPLAY_HUE_SWING;
        green = greenSwing + REPLAY_HUE_MIDDLE;
        if (index == memory->ringShotReplaySlot) {
          gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 255);
          gDPSetEnvColor(gfx++, red / REPLAY_GLOW_DIVISOR,
              green / REPLAY_GLOW_DIVISOR, blue / REPLAY_GLOW_DIVISOR, 255);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
          gDPSetEnvColor(gfx++, 0, 0, 0, 255);
        }

        replayCourse = (s8)save->unk1DF0[D_8023B684_ovl_01[index]].unk34;
        replayHole = (s8)save->unk1DF0[D_8023B684_ovl_01[index]].unk35;
        if (index == memory->ringShotReplaySlot) {
          SetTextSlot(RING_SHOT_REPLAY_TEXT_X,
              RING_SHOT_REPLAY_TEXT_Y + (index * RING_SHOT_SLOT_SPACING),
              ((replayCourse * RING_SHOT_HOLE_COUNT) +
                  MESSAGE_FIRST_RING_SHOT_HOLE) +
                  replayHole,
              RING_SHOT_REPLAY_TEXT_FONT, index + 1, 0, red, green, blue, 255);
        } else {
          SetTextSlot(RING_SHOT_REPLAY_TEXT_X,
              RING_SHOT_REPLAY_TEXT_Y + (index * RING_SHOT_SLOT_SPACING),
              ((replayCourse * RING_SHOT_HOLE_COUNT) +
                  MESSAGE_FIRST_RING_SHOT_HOLE) +
                  replayHole,
              RING_SHOT_REPLAY_TEXT_FONT, index + 1, 0, 255, 255, 255, 255);
        }
      }
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8021FC5C_ovl_01);
#endif

void func_80220A30_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s8 slot;

  InitSram();
  TransferSaveData(save, FALSE);
  for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
    D_8023B684_ovl_01[slot] = -1;
  }

  for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
    if ((s16)func_80220E28_ovl_01(&save->unk1DF0[slot])) {
      D_8023B684_ovl_01[slot] = slot;
    } else {
      D_8023B684_ovl_01[slot] = -1;
    }
  }

  if (D_8023B684_ovl_01[memory->ringShotReplaySlot] == -1) {
    for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
      if (D_8023B684_ovl_01[slot] != -1) {
        memory->ringShotReplaySlot = slot;
      }
    }
  }
}

void func_80220BB0_ovl_01(void) {
  EncodedAsset asset;
  AssetFile file;
  SaveData* save = GetSaveData();
  s32 index;
  s32 character;
  s32 palette;

  for (index = 0; index != CHARACTER_COUNT; index++) {
    OpenEncodedAsset(D_802308A4_ovl_01[index], &asset);
    ReadEncodedAsset(D_8012D430[index], &asset);
    OpenAssetFile(D_802308C8_ovl_01[index][0], &file);
    ReadAssetFile(D_8023BFF0_ovl_01[index], GetAssetFileSize(&file), &file);
  }

  for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
    if (D_8023B684_ovl_01[index] != -1) {
      character = (s8)save->unk1DF0[D_8023B684_ovl_01[index]].unk37;
      palette = (s8)save->unk1DF0[D_8023B684_ovl_01[index]].unk39;
      if (character < 0) {
        character = 0;
      }

      if (character >= CHARACTER_COUNT) {
        character = CHARACTER_COUNT - 1;
      }

      if (palette < 0) {
        palette = 0;
      }

      if (palette >= CHARACTER_PALETTE_COUNT) {
        palette = CHARACTER_PALETTE_COUNT - 1;
      }
    } else {
      palette = 0;
      character = 0;
    }

    OpenAssetFile(D_802308C8_ovl_01[character][palette], &file);
    ReadAssetFile(D_8023E3F0_ovl_01[index], GetAssetFileSize(&file), &file);
  }
}

s16 func_80220D68_ovl_01(s8 startSlot, u8 isForward) {
  s32 slot = startSlot;
  s32 step;
  s32 tryCount;

  if (slot < 0) {
    return -1;
  }

  if (slot >= SAVE_MENU_SLOT_COUNT) {
    return -1;
  }

  step = isForward ? 1 : -1;
  slot += step;
  for (tryCount = 0; tryCount != SAVE_MENU_SLOT_COUNT - 1; tryCount++) {
    if (slot >= SAVE_MENU_SLOT_COUNT) {
      slot = 0;
    }

    if (slot < 0) {
      slot = SAVE_MENU_SLOT_COUNT - 1;
    }

    if (D_8023B684_ovl_01[slot] != -1) {
      break;
    }

    slot += step;
  }

  if (tryCount == SAVE_MENU_SLOT_COUNT - 1) {
    return -1;
  }

  return slot;
}

s16 func_80220DF8_ovl_01(void) {
  return D_8023B684_ovl_01[func_8005AF74()->ringShotReplaySlot];
}

s32 func_80220E28_ovl_01(Unk68* record) {
  return (u8)record->unk36 >> 7;
}

void func_80220E34_ovl_01(Gfx** displayListHead, s32 courseIndex, s32 left,
    s32 top, u8 showsCourseWord) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B678_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureTile_4b(gfx++,
      ((u32)D_8023B678_ovl_01 + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
      COURSE_LABEL_WIDTH, COURSE_LABEL_HEIGHT, 0, 0, COURSE_LABEL_WIDTH - 1,
      COURSE_LABEL_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  if (showsCourseWord == TRUE) {
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (left + COURSE_WORD_OFFSET_X) << 2, top << 2,
        (left + COURSE_WORD_OFFSET_X + COURSE_WORD_WIDTH) << 2,
        (top + COURSE_LABEL_HEIGHT) << 2, G_TX_RENDERTILE,
        (COURSE_DIGIT_WIDTH * COURSE_COUNT) << 5, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  }

  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, (left + COURSE_DIGIT_OFFSET_X) << 2, top << 2,
      (left + COURSE_DIGIT_OFFSET_X + COURSE_DIGIT_WIDTH) << 2,
      (top + COURSE_LABEL_HEIGHT) << 2, G_TX_RENDERTILE,
      (courseIndex * COURSE_DIGIT_WIDTH) << 5, 0, 1 << 10, 1 << 10);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

void func_802213C8_ovl_01(
    Gfx** displayListHead, s32 imageIndex, s32 left, s32 top) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(
      gfx++, ((u32)D_8023B680_ovl_01[imageIndex] + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != SAVE_ICON_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023B680_ovl_01[imageIndex] + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * SAVE_ICON_WIDTH * SAVE_ICON_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, SAVE_ICON_WIDTH, SAVE_ICON_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2,
        (top + (stripIndex * SAVE_ICON_SHOWN_STRIP_HEIGHT)) << 2,
        (left + SAVE_ICON_SHOWN_WIDTH) << 2,
        (top + (stripIndex * SAVE_ICON_SHOWN_STRIP_HEIGHT) +
            SAVE_ICON_SHOWN_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0, (SAVE_ICON_WIDTH << 10) / SAVE_ICON_SHOWN_WIDTH,
        (SAVE_ICON_STRIP_HEIGHT << 10) / SAVE_ICON_SHOWN_STRIP_HEIGHT);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

s32 func_8022181C_ovl_01(void) {
  SaveData* save = GetSaveData();
  s32 starCount = 0;
  s32 courseIndex;
  s32 holeIndex;

  for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
    for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
      starCount += save->hasRingShotStar[courseIndex][holeIndex] > 0;
    }
  }

  if (starCount >= RING_SHOT_STARS_PER_LEVEL * 5) {
    return 6;
  }

  if (starCount >= RING_SHOT_STARS_PER_LEVEL * 4) {
    return 5;
  }

  if (starCount >= RING_SHOT_STARS_PER_LEVEL * 3) {
    return 4;
  }

  if (starCount >= RING_SHOT_STARS_PER_LEVEL * 2) {
    return 3;
  }

  if (starCount >= RING_SHOT_STARS_PER_LEVEL) {
    return 2;
  }

  return 1;
}

/* Doesn't match: the original keeps the saved game's place in a saved register
 */
#ifdef NON_MATCHING
void func_802218C0_ovl_01(void* buffer, u8 bufferKind) {
  EncodedAsset asset;
  AssetFile file;
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s32 gameIndex;
  void** bufferSlot;
  void** imageSlot;
  s32 miniGolfIndex;

  switch (bufferKind) {
    case SAVE_GAME_BUFFER_CONTROLLER_ICONS:
      D_8023BD80_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_CONTROLLER_ICONS, &asset);
      ReadEncodedAsset(D_8023BD80_ovl_01, &asset);
      break;
    case SAVE_GAME_BUFFER_PLAYER_LABELS:
      D_8023BD84_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
      ReadEncodedAsset(D_8023BD84_ovl_01, &asset);
      break;
    case SAVE_GAME_BUFFER_FIRST_BOARD:
      imageSlot = &D_8023BD98_ovl_01;
      *imageSlot = buffer;
      OpenEncodedAsset(ASSET_FIRST_GAME_BOARD, &asset);
      ReadEncodedAsset(*imageSlot, &asset);
      break;
    case SAVE_GAME_BUFFER_SECOND_BOARD:
      imageSlot = &D_8023BD9C_ovl_01;
      *imageSlot = buffer;
      OpenEncodedAsset(ASSET_SECOND_GAME_BOARD, &asset);
      ReadEncodedAsset(*imageSlot, &asset);
      break;
    case SAVE_GAME_BUFFER_THIRD_BOARD:
      imageSlot = &D_8023BDA0_ovl_01;
      *imageSlot = buffer;
      OpenEncodedAsset(ASSET_THIRD_GAME_BOARD, &asset);
      ReadEncodedAsset(*imageSlot, &asset);
      break;
    case SAVE_GAME_BUFFER_COURSE:
      bufferSlot = &D_8023BD8C_ovl_01;
      *bufferSlot = buffer;
      gameIndex = memory->savedGameIndex;
      if (gameIndex < 0) {
        gameIndex = 0;
      }

      if (gameIndex >= SAVED_GAME_COUNT) {
        gameIndex = SAVED_GAME_COUNT - 1;
      }

      if (!save->savedGames[gameIndex].isValid) {
        break;
      }

      if (!save->savedGames[gameIndex].state.isMiniGolf) {
        OpenEncodedAsset(
            g_CourseIconAssets[save->savedGames[gameIndex].state.courseIndex],
            &asset);
        ReadEncodedAsset(*bufferSlot, &asset);
      } else {
        miniGolfIndex = save->savedGames[gameIndex].state.courseIndex -
            MINI_GOLF_COURSE_INDEX;
        OpenEncodedAsset(g_MiniGolfCourseIconAssets[miniGolfIndex], &asset);
        ReadEncodedAsset(*bufferSlot, &asset);
        OpenAssetFile(g_MiniGolfDiscFiles[miniGolfIndex], &file);
        ReadAssetFile(D_801052C4[0], GetAssetFileSize(&file), &file);
      }

      break;
    case SAVE_GAME_BUFFER_CHOSEN_COURSE:
      bufferSlot = &D_8023BD90_ovl_01;
      *bufferSlot = buffer;
      gameIndex = memory->savedGameIndex;
      if (gameIndex < 0) {
        gameIndex = 0;
      }

      if (gameIndex >= SAVED_GAME_COUNT) {
        gameIndex = SAVED_GAME_COUNT - 1;
      }

      if (!save->savedGames[gameIndex].isValid) {
        break;
      }

      if (!save->savedGames[gameIndex].state.isMiniGolf) {
        OpenEncodedAsset(
            g_CourseIconAssets[save->savedGames[gameIndex].state.courseIndex],
            &asset);
        ReadEncodedAsset(*bufferSlot, &asset);
      } else {
        miniGolfIndex = save->savedGames[gameIndex].state.courseIndex -
            MINI_GOLF_COURSE_INDEX;
        OpenEncodedAsset(g_MiniGolfCourseIconAssets[miniGolfIndex], &asset);
        ReadEncodedAsset(*bufferSlot, &asset);
        OpenAssetFile(g_MiniGolfDiscFiles[miniGolfIndex], &file);
        ReadAssetFile(D_801052C4[0], GetAssetFileSize(&file), &file);
      }

      break;
  }

  D_80230B4E_ovl_01 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802218C0_ovl_01);
#endif

void func_80221B20_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s8 gameIndex;
  s8 modeIndex;

  for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
    D_80230B50_ovl_01[gameIndex] = -1;
  }

  D_80230B4D_ovl_01 = 0;
  InitSram();
  TransferSaveData(save, FALSE);
  for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
    if (save->savedGames[gameIndex].isValid) {
      if (save->savedGames[gameIndex].state.isMiniGolf) {
        D_80230B50_ovl_01[gameIndex] = SAVE_MENU_MODE_MINI_GOLF;
      } else {
        for (modeIndex = 0; modeIndex != SAVE_MENU_MODE_COUNT; modeIndex++) {
          if (save->savedGames[gameIndex].state.gameMode ==
              D_80230B88_ovl_01[modeIndex]) {
            D_80230B50_ovl_01[gameIndex] = modeIndex;
            break;
          }
        }
      }
    }
  }

  for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
    if (D_80230B50_ovl_01[gameIndex] != -1) {
      break;
    }
  }

  if (gameIndex == SAVED_GAME_COUNT) {
    memory->savedGameIndex = 0;
  } else {
    memory->savedGameIndex = gameIndex;
  }

  func_80221CC0_ovl_01();
}

/* Doesn't match: the original sets the first loop's counter later and keeps the
 * player count in other registers */
#ifdef NON_MATCHING
void func_80221CC0_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s8 playerIndex;
  s8 playerCount;
  s8 humanCount;
  s8 gameIndex;
  UnkB8* savedPlayers;

  for (playerIndex = 0; playerIndex != MAX_PLAYER_COUNT; playerIndex++) {
    D_80230B40_ovl_01[playerIndex] = -1;
    D_8023BDA4_ovl_01[playerIndex] = -1;
  }

  D_80230B48_ovl_01 = 0;
  gameIndex = memory->savedGameIndex;
  playerCount = save->savedGames[gameIndex].state.playerCount;
  savedPlayers = save->savedPlayers[gameIndex];
  D_80230B4A_ovl_01 = save->savedGames[gameIndex].state.playerCount;
  for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
    if ((s8)savedPlayers[playerIndex].unk1E[0]) {
      D_80230B40_ovl_01[playerIndex] = MAXCONTROLLERS;
      D_80230B4A_ovl_01--;
    } else {
      D_80230B40_ovl_01[playerIndex] = -1;
    }
  }

  humanCount = 0;
  for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
    if (D_80230B40_ovl_01[playerIndex] != MAXCONTROLLERS) {
      D_8023BDA4_ovl_01[humanCount] = playerIndex;
      humanCount++;
    }
  }

  D_80230B48_ovl_01 = 0;
  D_80230B4C_ovl_01 = 0;
  for (playerIndex = 0; playerIndex != MAXCONTROLLERS; playerIndex++) {
    D_801B70F0[playerIndex] = 0;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80221CC0_ovl_01);
#endif

/*
 * Doesn't match: the menu memory pointer and the mode image table take
 * each other's saved registers.
 */
#ifdef NON_MATCHING
void func_80221EBC_ovl_01(void** buffers, u8 bufferKind) {
  EncodedAsset asset;
  MenuMemory* memory = func_8005AF74();
  s32 imageIndex;
  s32 mode;
  SaveMenuModeImage* modeImages;

  switch (bufferKind) {
    case SAVE_MENU_BUFFER_SLOTS:
      D_8023BD70_ovl_01 = buffers;
      modeImages = D_80230B5C_ovl_01;
      for (imageIndex = 0; imageIndex != SAVED_GAME_COUNT; imageIndex++) {
        mode = D_80230B50_ovl_01[imageIndex];
        if (mode == -1) {
          mode = 0;
        }

        OpenEncodedAsset(modeImages[mode].assetId, &asset);
        ReadEncodedAsset(D_8023BD70_ovl_01[imageIndex], &asset);
      }

      OpenEncodedAsset(ASSET_SAVE_MENU_FRAME, &asset);
      ReadEncodedAsset(D_8023BD70_ovl_01[SAVED_GAME_COUNT], &asset);
      break;
    case SAVE_MENU_BUFFER_CHOSEN:
      D_8023BD74_ovl_01 = buffers;
      modeImages = D_80230B5C_ovl_01;
      for (imageIndex = 0; imageIndex != SAVE_MENU_CHOSEN_IMAGE_COUNT;
          imageIndex++) {
        mode = D_80230B50_ovl_01[memory->savedGameIndex];
        if (mode == -1) {
          mode = 0;
        }

        OpenEncodedAsset(modeImages[mode].assetId, &asset);
        ReadEncodedAsset(D_8023BD74_ovl_01[imageIndex], &asset);
      }

      break;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80221EBC_ovl_01);
#endif

void func_80222038_ovl_01(void* buffer, u8 bufferIndex) {
  EncodedAsset asset;

  switch (bufferIndex) {
    case 1:
      D_8023BD78_ovl_01 = buffer;
      break;
    case 2:
      D_8023BD7C_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
      ReadEncodedAsset(D_8023BD7C_ovl_01, &asset);
      break;
  }
}

/* The saved-games screen's text slots */
#define SAVED_GAME_TEXT_SLOT_COUNT 4
/* How many updates the boards take to turn to the next saved game */
#define SAVED_GAME_TURN_FRAMES 7

/* The saved game the boards show */
#define CHOSEN_GAME (save->savedGames[memory->savedGameIndex])
/*
 * Reads the chosen saved game's mode icon, then its course image into the
 * course buffer not on screen, which then becomes the one on screen
 */
#define LOAD_CHOSEN_GAME_IMAGES() \
  OpenEncodedAsset( \
      g_ModeIconAnimations[( \
          u8)D_80230BB0_ovl_01[D_80230B50_ovl_01[memory->savedGameIndex]]] \
                          [D_8022F8C0_ovl_01], \
      &asset); \
  ReadEncodedAsset(D_8023BD74_ovl_01[D_8022F8C4_ovl_01], &asset); \
  if (CHOSEN_GAME.isValid) { \
    if (!CHOSEN_GAME.state.isMiniGolf) { \
      OpenEncodedAsset( \
          g_CourseIconAssets[CHOSEN_GAME.state.courseIndex], &asset); \
      ReadEncodedAsset((&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01 ^ 1], &asset); \
    } else { \
      miniGolfIndex = CHOSEN_GAME.state.courseIndex - MINI_GOLF_COURSE_INDEX; \
      OpenEncodedAsset(g_MiniGolfCourseIconAssets[miniGolfIndex], &asset); \
      ReadEncodedAsset((&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01 ^ 1], &asset); \
      OpenAssetFile(g_MiniGolfDiscFiles[miniGolfIndex], &file); \
      ReadAssetFile(D_801052C4[0], GetAssetFileSize(&file), &file); \
    } \
\
    D_80230B4E_ovl_01 ^= 1; \
  }

/*
 * Handles the saved-games screen's buttons. The left and right buttons turn
 * the boards to another saved game. On a game with a mode, each A press gives
 * the next human player the controller that pressed it (all of them get
 * controller 1 when only one is plugged in); once every human has one, A
 * closes the wipe to continue the game. B steps back through the players,
 * then leaves for the title screen. A press while the boards turn finishes
 * the turn instead.
 *
 * Doesn't match: the original merges the two mini golf course loads and
 * keeps both other course loads, where this merges the other course loads;
 * it also counts the controller loops through the controller table's address.
 */
#ifdef NON_MATCHING
void func_8022209C_ovl_01(void) {
  MenuMemory* memory = func_8005AF74();
  SaveData* save = GetSaveData();
  EncodedAsset asset;
  AssetFile file;
  u8 unusedStack[0xB0];
  s32 slot;
  s32 controller;
  s32 humanIndex;
  s32 miniGolfIndex;

  if (GetWipeKind() != WIPE_NONE) {
    for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
      SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    if (GetWipeProgress() == 0) {
      StopLoaderThread();
      FreeAllCharacterSlots();
      D_800FE334 = 0;
      for (slot = 1; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
        SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      }

      g_MenuScreen = MENU_SCREEN_EXIT;
      for (slot = 0; slot != MAX_PLAYER_COUNT; slot++) {
        g_PlayerControllerIndices[slot] = D_80230B40_ovl_01[slot];
      }

      func_801F6A38_ovl_01();
      ResumeBets();
    }
  } else if (IsHudAnimationDone() == TRUE) {
    if (D_80230B4D_ovl_01) {
      if (g_MenuPressedButtons & A_BUTTON) {
        if (memory->savedGameTurn != 0) {
          memory->savedGameTurn = 0;
          return;
        }

        FreeAllCharacterSlots();
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        LoadWipeShape(
            save->savedGames[memory->savedGameIndex].state.courseIndex);
        StartWipe(WIPE_CLOSING);
        func_80222F2C_ovl_01();
        for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        D_80230B4D_ovl_01 = FALSE;
        return;
      }

      if (g_MenuPressedButtons & B_BUTTON) {
        if (memory->savedGameTurn != 0) {
          memory->savedGameTurn = 0;
        }

        D_80230B4D_ovl_01 = FALSE;
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        if (D_80230B4A_ovl_01 == 1 || g_ControllerCount == 1) {
          D_80230B48_ovl_01 = 0;
          for (slot = 0; slot != D_80230B4A_ovl_01; slot++) {
            D_80230B40_ovl_01[D_8023BDA4_ovl_01[slot]] = NO_CONTROLLER;
          }

          for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
            SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
          }

          D_80230B4C_ovl_01 = FALSE;
          return;
        }

        D_80230B48_ovl_01--;
        D_80230B40_ovl_01[D_8023BDA4_ovl_01[D_80230B48_ovl_01]] = NO_CONTROLLER;
      }
    } else if (g_MenuPressedButtons & A_BUTTON) {
      if (memory->savedGameTurn != 0) {
        PlaySound(D_80230B50_ovl_01[memory->savedGameIndex] != -1
                ? SOUND_MENU_CONFIRM
                : SOUND_MENU_BLOCKED,
            MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        memory->savedGameTurn = 0;
        return;
      }

      if (D_80230B50_ovl_01[memory->savedGameIndex] != -1) {
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        if (!D_80230B4C_ovl_01) {
          D_80230B4C_ovl_01 = TRUE;
          if (D_80230B4A_ovl_01 == 1) {
            for (controller = 0; controller != g_ControllerCount;
                controller++) {
              if (g_MenuControllerData[controller].button & A_BUTTON) {
                D_80230B4D_ovl_01 = TRUE;
                D_80230B40_ovl_01[D_8023BDA4_ovl_01[D_80230B48_ovl_01]] =
                    controller;
                D_80230B48_ovl_01++;
                for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
                  SetTextSlot(
                      0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                }

                break;
              }
            }
          } else if (g_ControllerCount == 1) {
            for (humanIndex = D_80230B48_ovl_01;
                humanIndex != D_80230B4A_ovl_01; humanIndex++) {
              D_80230B40_ovl_01[D_8023BDA4_ovl_01[D_80230B48_ovl_01]] = 0;
              D_80230B48_ovl_01++;
            }

            D_80230B4D_ovl_01 = TRUE;
            for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
              SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            }
          }

          for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
            SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
          }

          return;
        }

        for (controller = 0; controller != g_ControllerCount; controller++) {
          if (g_MenuControllerData[controller].button & A_BUTTON) {
            D_80230B40_ovl_01[D_8023BDA4_ovl_01[D_80230B48_ovl_01]] =
                controller;
            D_80230B48_ovl_01++;
            break;
          }
        }

        if (D_80230B48_ovl_01 == D_80230B4A_ovl_01) {
          D_80230B4D_ovl_01 = TRUE;
          for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
            SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
          }
        }
      } else {
        PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      }
    } else if (g_MenuPressedButtons & B_BUTTON) {
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
      if (memory->savedGameTurn != 0) {
        memory->savedGameTurn = 0;
      }

      if (D_80230B48_ovl_01 <= 0) {
        StartHudAnimation(HUD_ANIMATION_FADE_OUT);
        for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        g_MenuScreen = MENU_SCREEN_TITLE;
        StopLoaderThread();
        FreeAllCharacterSlots();
        D_800FE334 = 0;
      }

      D_80230B48_ovl_01--;
      D_80230B40_ovl_01[D_8023BDA4_ovl_01[D_80230B48_ovl_01]] = NO_CONTROLLER;
      if (D_80230B48_ovl_01 <= 0) {
        D_80230B4C_ovl_01 = FALSE;
        for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }
      }
    } else if (memory->savedGameTurn == 0) {
      if (g_MenuRepeatButtons & L_JPAD) {
        for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        memory->savedGameIndex--;
        if (memory->savedGameIndex < 0) {
          memory->savedGameIndex = SAVED_GAME_COUNT - 1;
        }

        memory->savedGameTurn = SAVED_GAME_TURN_FRAMES;
        D_8022F8C0_ovl_01 = 0;
        func_80221CC0_ovl_01();
        LOAD_CHOSEN_GAME_IMAGES();
      } else if (g_MenuRepeatButtons & R_JPAD) {
        for (slot = 0; slot != SAVED_GAME_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }

        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        memory->savedGameIndex++;
        if (memory->savedGameIndex >= SAVED_GAME_COUNT) {
          memory->savedGameIndex = 0;
        }

        memory->savedGameTurn = -SAVED_GAME_TURN_FRAMES;
        func_80221CC0_ovl_01();
        D_8022F8C0_ovl_01 = 0;
        LOAD_CHOSEN_GAME_IMAGES();
      }
    } else {
      if (memory->savedGameTurn < 0) {
        memory->savedGameTurn++;
      }

      if (memory->savedGameTurn > 0) {
        memory->savedGameTurn--;
      }
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8022209C_ovl_01);
#endif

s32 func_80222C94_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  SavedGame* game = &save->savedGames[memory->savedGameIndex];

  return -(game->isValid == FALSE);
}

s32 func_80222CDC_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  SavedGame* game;
  UnkB8* players;
  Unk68* records;
  s32 playerCount = 0;
  s32 index;

  switch (memory->savedGameIndex) {
    case SAVE_MENU_GAME_TOURNAMENT:
      game = &save->modeGames[MODE5_GAME_INDEX];
      records = &save->modeRecords[MODE5_RECORD_INDEX];
      players = &save->modePlayers[MODE5_PLAYER_INDEX];
      playerCount = MODE5_PLAYER_COUNT;
      break;
    case SAVE_MENU_GAME_GET_CHARACTER:
      game = &save->modeGames[MODE4_GAME_INDEX];
      records = &save->modeRecords[MODE4_RECORD_INDEX];
      players = &save->modePlayers[MODE4_PLAYER_INDEX];
      playerCount = MODE4_PLAYER_COUNT;
      break;
    case SAVE_MENU_GAME_STROKE:
      game = &save->modeGames[MODE0_GAME_INDEX];
      records = &save->modeRecords[MODE0_RECORD_INDEX];
      players = &save->modePlayers[MODE0_PLAYER_INDEX];
      playerCount = MODE0_PLAYER_COUNT;
      break;
  }

  if (!game->isValid) {
    return -1;
  }

  if (game->state.playerCount > playerCount) {
    return -1;
  }

  playerCount = game->state.playerCount;
  for (index = 0; index != playerCount; index++) {
    g_Players[index] = players[index];
    D_800FEDD8[index] = records[index];
  }

  g_GameState = game->state;
  g_GameState.unk0 = 1;
  SetGameFlag(GAME_FLAG_UNK1E);
  g_HoleSetup = game->hole;
  g_GameState.unk30 = 2;
  return 0;
}

s32 func_80222F2C_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  UnkB8* savedPlayer;
  s32 playerIndex;
  s32 character;
  u16 controller;

  if (D_80230B50_ovl_01[memory->savedGameIndex] == -1) {
    return -1;
  }

  if (SaveOrContinueGame(memory->savedGameIndex, TRUE) == -1) {
    return -1;
  }

  savedPlayer = save->savedPlayers[memory->savedGameIndex];
  for (playerIndex = 0; playerIndex != MAX_PLAYER_COUNT; playerIndex++) {
    controller = D_80230B40_ovl_01[playerIndex];
    g_PlayerControllerIndices[playerIndex] = controller;
    g_Players[playerIndex].controllerIndex = controller;
    sprintf(g_Players[playerIndex].name, D_80231A3C_ovl_01, savedPlayer->name);
    character = (s8)savedPlayer->characterIndex;
    D_8012F480[playerIndex] = character;
    g_Players[playerIndex].characterIndex = character;
    LoadPlayerFace(
        playerIndex, character, (s8)g_Players[playerIndex].paletteIndex);
    savedPlayer++;
  }

  if (!IsGameFlagSet(GAME_FLAG_KEEP_SAVED_GAMES)) {
    memset(&save->savedGames[memory->savedGameIndex], 0, sizeof(SavedGame));
  }

  return 0;
}

const char D_80231A3C_ovl_01[] = "%s";

/* The backdrop layers that func_801FC60C draws behind the saved games */
#define SAVED_GAMES_BOARD_KIND 0x1B

/* The numbered boards of D_8023BD98, 64 by 64 CI8 images loaded in two strips
 */
#define GAME_BOARD_WIDTH 64
#define GAME_BOARD_STRIP_HEIGHT 32
#define GAME_BOARD_STRIP_COUNT 2
/* The animated mode icon of D_8023BD74, drawn on the board */
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
/* Where the mode icon sits on the board */
#define MODE_ICON_OFFSET_X 8
#define MODE_ICON_OFFSET_Y 14

/*
 * The boards stand on a circle, seen from above, that each update of
 * MenuMemory.savedGameTurn turns by a step. They are projected like the
 * course wheel, from a nearer eye.
 */
#define BOARD_EYE_HEIGHT 70.0f
#define BOARD_EYE_DISTANCE 60.0f
#define BOARD_RADIUS_X 48.0f
#define BOARD_RADIUS_Z 40.0f
#define BOARD_TURN_STEP_DEGREES 15
#define GAME_BOARD_HALF_SIZE 32.0f
/* The mode icons on the boards not chosen are smaller, and lower */
#define MODE_ICON_HALF_SIZE 24.0f
#define MODE_ICON_DROP 4
/* The boards not chosen, and the chosen one while it turns, are darkened */
#define UNCHOSEN_BOARD_SHADE 127
/* The bob that a fourth board would have, a step of its phase per update */
#define BOARD_BOB_PHASE_STEP 0.1f
#define BOARD_BOB_HEIGHT 8.0f
/* The 200 by 10 strips of D_8023BD78, drawn narrower by the player count */
#define SAVED_GAME_STRIP_WIDTH 200
#define SAVED_GAME_STRIP_HEIGHT 10
#define SAVED_GAME_STRIP_COUNT 3
#define SAVED_GAME_STRIP_TOP 152
#define SAVED_GAME_STRIP_LEFT 80
#define SAVED_GAME_TWO_PLAYER_STRIP_LEFT 70
#define SAVED_GAME_STRIP_SHOWN_WIDTH 160
#define SAVED_GAME_FEW_PLAYER_STRIP_SHOWN_WIDTH 180
#define SAVED_GAME_ARROW_GAP 80
#define SAVED_GAME_ARROW_Y 124
/* The rows of the game modes' image, 192 by 20 CI4 each */
#define GAME_MODE_LABEL_WIDTH 192
#define GAME_MODE_LABEL_HEIGHT 20
#define GAME_MODE_LABEL_IMAGE_SIZE \
  ((GAME_MODE_LABEL_WIDTH * GAME_MODE_LABEL_HEIGHT) / 2)
#define GAME_MODE_LABEL_X 160
#define SINGLE_PLAYER_GAME_MODE_LABEL_X 170
#define GAME_MODE_LABEL_TOP 156
#define SAVED_GAME_COURSE_X 110
#define SAVED_GAME_COURSE_Y 187
/* A tournament shows a message of each course in place of its name */
#define MESSAGE_FIRST_TOURNAMENT_COURSE 0x6F
/* Mini golf courses from this one use Peach's Castle's name */
#define FIRST_PEACHS_CASTLE_MINI_GOLF_COURSE \
  (MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD)
/* Shows the slot's first value, here the saved hole's number */
#define SAVED_GAME_HOLE_X 180
#define SAVED_GAME_HOLE_Y 203
#define MESSAGE_SAVED_GAME_HOLE 0x76
/* Shown while the next human player is given a controller, then once every
 * human has one */
#define CONTROLLER_PROMPT_X 70
#define CONTINUE_PROMPT_X 54
#define PROMPT_Y 184
#define MESSAGE_CONTROLLER_PROMPT 0x37
#define MESSAGE_CONTINUE_PROMPT 0x38
/* Shown when the chosen saved game is empty */
#define EMPTY_GAME_TEXT_X 100
#define EMPTY_GAME_TEXT_Y 186
#define MESSAGE_EMPTY_SAVED_GAME 0x26

extern f32 D_80230C34_ovl_01;
/* How far left of the label's place each row of the game modes' image is drawn
 */
extern s32 D_80230CEC_ovl_01[SAVE_MENU_MODE_MINI_GOLF + 1];

void func_801FC60C_ovl_01(Gfx** displayListHead, s32 layers);
void func_8021C200_ovl_01(Gfx** displayListHead, s32 gap, s32 centerY);
void func_80224AFC_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 halfSize, s32 scale);
void func_80225358_ovl_01(Gfx** displayListHead);
void func_80225960_ovl_01(Gfx** displayListHead);
void func_80226190_ovl_01(Gfx** displayListHead);
void func_8022644C_ovl_01(Gfx** displayListHead);

/*
 * Draws the saved-games screen: the characters, the three numbered boards
 * turning on their circle with each game's mode icon, the chosen game's mode,
 * course and hole, and the prompt for giving out the controllers.
 */
/*
 * Doesn't match: the original keeps the menu memory pointer on the stack
 * and the mode icons' texel step in a saved register, and doesn't keep
 * the degrees-to-radians factor in a register.
 */
#ifdef NON_MATCHING
void func_802230C0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  u16 perspNorm;
  /* Sized like the course wheel's, of which the boards use three */
  f32 boardX[COURSE_COUNT];
  f32 boardY[COURSE_COUNT];
  f32 boardScale[COURSE_COUNT];
  s32 unusedSlots[COURSE_COUNT];
  s32 drawOrder[COURSE_COUNT];
  u8 unusedStack[8];
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
  s32 index;
  s32 otherIndex;
  s32 key;
  f32 angle;
  f32 circleX;
  s32 slot;
  s32 halfSize;
  s32 step;
  s32 iconHalfSize;
  s32 iconStep;
  f32 bob;
  s32 stripLeft;
  s32 stripShownWidth;
  s32 gameIndex;
  s32 courseIndex;
  s32 holeIndex;
  s32* chosenMode;
  s32* labelOffset;
  s32 labelX;
  f32 radiusX;
  s32 boardDegrees;

  func_801FC60C_ovl_01(&gfx, SAVED_GAMES_BOARD_KIND);
  BuildDynamicTextures(&gfx);
  func_80225358_ovl_01(&gfx);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  func_80225960_ovl_01(&gfx);

  guPerspectiveF(D_8023BA60_ovl_01, &perspNorm, WHEEL_FIELD_OF_VIEW,
      WHEEL_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE, 1.0f);
  guLookAtF(D_8023BAA0_ovl_01, 0.0f, BOARD_EYE_HEIGHT, BOARD_EYE_DISTANCE, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  SetCpuProjectionF(D_8023BA60_ovl_01, D_8023BAA0_ovl_01, g_Viewports);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);

  radiusX = BOARD_RADIUS_X;
  boardDegrees = DEGREES_PER_TURN / SAVED_GAME_COUNT;
  for (index = 0; index != SAVED_GAME_COUNT; index++) {
    ScreenPoint point;

    angle = -DEGREES_TO_RADIANS(
        (((memory->savedGameIndex - index) % SAVED_GAME_COUNT) * boardDegrees) +
        (memory->savedGameTurn * BOARD_TURN_STEP_DEGREES));
    circleX = sinf(angle) * radiusX;
    if (ProjectPoint(circleX, 0.0f, cosf(angle) * BOARD_RADIUS_Z, &point) < 0) {
      boardX[index] = point.x;
      boardY[index] = point.y - WHEEL_RAISE;
      boardScale[index] = point.scaleX;
    }
  }

  for (otherIndex = 0; otherIndex != SAVED_GAME_COUNT; otherIndex++) {
    drawOrder[otherIndex] = otherIndex;
  }

  /* Draw the farthest boards first, so the nearer ones cover them */
  for (otherIndex = 1; otherIndex != SAVED_GAME_COUNT; otherIndex++) {
    key = drawOrder[otherIndex];
    for (index = otherIndex - 1;
        index >= 0 && boardY[drawOrder[index]] > boardY[key]; index--) {
      drawOrder[index + 1] = drawOrder[index];
    }

    drawOrder[index + 1] = key;
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

  D_80230C34_ovl_01 += BOARD_BOB_PHASE_STEP;
  for (index = 0; index != SAVED_GAME_COUNT; index++) {
    slot = drawOrder[index];
    iconHalfSize = boardScale[slot] * MODE_ICON_HALF_SIZE;
    halfSize = boardScale[slot] * GAME_BOARD_HALF_SIZE;
    D_8023BDB0_ovl_01 = iconHalfSize;
    iconStep = (f32)(MODE_ICON_STRIP_HEIGHT << 10) / iconHalfSize;
    D_8023BDAC_ovl_01 = iconStep;
    step = (f32)(GAME_BOARD_STRIP_HEIGHT << 10) / halfSize;
    /* The loop stops before SAVED_GAME_COUNT, so no board bobs */
    if (index == SAVED_GAME_COUNT) {
      bob = sinf(D_80230C34_ovl_01) * BOARD_BOB_HEIGHT;
    } else {
      bob = 0.0f;
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    if ((memory->savedGameIndex != slot) || (memory->savedGameTurn != 0)) {
      if (memory->savedGameIndex == slot) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      } else {
        gDPSetCombineMode(
            gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, UNCHOSEN_BOARD_SHADE, UNCHOSEN_BOARD_SHADE,
            UNCHOSEN_BOARD_SHADE, 255);
      }

      gDPLoadTLUT_pal256(
          gfx++, ((u32)(&D_8023BD98_ovl_01)[slot] + IMAGE_HEADER_SIZE) & ~7);
      for (otherIndex = 0; otherIndex != GAME_BOARD_STRIP_COUNT; otherIndex++) {
        gDPLoadTextureBlock(gfx++,
            ((u32)(&D_8023BD98_ovl_01)[slot] + LARGE_PALETTED_TEXELS_OFFSET +
                (otherIndex * GAME_BOARD_WIDTH * GAME_BOARD_STRIP_HEIGHT)) &
                ~7,
            G_IM_FMT_CI, G_IM_SIZ_8b, GAME_BOARD_WIDTH, GAME_BOARD_STRIP_HEIGHT,
            0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (s32)(boardX[slot] - halfSize) << 2,
            (s32)(boardY[slot] - halfSize + (otherIndex * halfSize) + bob) << 2,
            (s32)(boardX[slot] + halfSize) << 2,
            (s32)(boardY[slot] + (otherIndex * halfSize) + bob) << 2,
            G_TX_RENDERTILE, 0, 0, step, step);
        gDPPipeSync(gfx++);
      }

      if (D_80230B50_ovl_01[slot] != -1) {
        gDPLoadTLUT_pal256(
            gfx++, ((u32)D_8023BD70_ovl_01[slot] + IMAGE_HEADER_SIZE) & ~7);
        for (otherIndex = 0; otherIndex != MODE_ICON_STRIP_COUNT;
            otherIndex++) {
          gDPLoadTextureBlock(gfx++,
              ((u32)D_8023BD70_ovl_01[slot] + LARGE_PALETTED_TEXELS_OFFSET +
                  (otherIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)) &
                  ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT,
              0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          gSPScisTextureRectangle(gfx++,
              (s32)(boardX[slot] - iconHalfSize) << 2,
              ((s32)(boardY[slot] - iconHalfSize + (otherIndex * iconHalfSize) +
                   bob) +
                  MODE_ICON_DROP)
                  << 2,
              (s32)(boardX[slot] + iconHalfSize) << 2,
              ((s32)(boardY[slot] + (otherIndex * iconHalfSize) + bob) +
                  MODE_ICON_DROP)
                  << 2,
              G_TX_RENDERTILE, 0, 0, iconStep, iconStep);
          gDPPipeSync(gfx++);
        }
      }
    } else {
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      func_80224AFC_ovl_01(&gfx, boardX[slot] - halfSize,
          boardY[slot] - halfSize + bob, halfSize, step);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);

  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023BD78_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  for (index = 0; index != SAVED_GAME_STRIP_COUNT; index++) {
    if (playerCount == 1) {
      stripLeft = SAVED_GAME_STRIP_LEFT;
      stripShownWidth = SAVED_GAME_FEW_PLAYER_STRIP_SHOWN_WIDTH;
    } else if (playerCount == 2) {
      stripLeft = SAVED_GAME_TWO_PLAYER_STRIP_LEFT;
      stripShownWidth = SAVED_GAME_FEW_PLAYER_STRIP_SHOWN_WIDTH;
    } else {
      stripLeft = SAVED_GAME_STRIP_LEFT;
      stripShownWidth = SAVED_GAME_STRIP_SHOWN_WIDTH;
    }

    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023BD78_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            (index * SAVED_GAME_STRIP_WIDTH * SAVED_GAME_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, SAVED_GAME_STRIP_WIDTH,
        SAVED_GAME_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, stripLeft << 2,
        (SAVED_GAME_STRIP_TOP + (index * SAVED_GAME_STRIP_HEIGHT)) << 2,
        (stripLeft + stripShownWidth) << 2,
        (SAVED_GAME_STRIP_TOP + SAVED_GAME_STRIP_HEIGHT +
            (index * SAVED_GAME_STRIP_HEIGHT))
            << 2,
        G_TX_RENDERTILE, 0, 0, (SAVED_GAME_STRIP_WIDTH << 10) / stripShownWidth,
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  func_8021C200_ovl_01(&gfx, SAVED_GAME_ARROW_GAP, SAVED_GAME_ARROW_Y);
  chosenMode = &D_80230B50_ovl_01[memory->savedGameIndex];
  if (*chosenMode != -1) {
    labelOffset = &D_80230CEC_ovl_01[D_80230BB0_ovl_01[*chosenMode]];
    gDPLoadTLUT_pal16(
        gfx++, 0, ((u32)D_8023BD7C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(gfx++,
        ((u32)D_8023BD7C_ovl_01 + PALETTED_TEXELS_OFFSET +
            (D_80230BB0_ovl_01[*chosenMode] * GAME_MODE_LABEL_IMAGE_SIZE)) &
            ~7,
        G_IM_FMT_CI, GAME_MODE_LABEL_WIDTH, GAME_MODE_LABEL_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    labelX = (playerCount == 1) ? SINGLE_PLAYER_GAME_MODE_LABEL_X
                                : GAME_MODE_LABEL_X;
    gSPScisTextureRectangle(gfx++, (labelX - *labelOffset) << 2,
        GAME_MODE_LABEL_TOP << 2,
        (labelX - *labelOffset + GAME_MODE_LABEL_WIDTH) << 2,
        (GAME_MODE_LABEL_TOP + GAME_MODE_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  if (func_80222C94_ovl_01() != 0) {
    SetTextSlot(EMPTY_GAME_TEXT_X, EMPTY_GAME_TEXT_Y, MESSAGE_EMPTY_SAVED_GAME,
        1, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
  } else if (!D_80230B4C_ovl_01) {
    gameIndex = memory->savedGameIndex;
    if (gameIndex < 0) {
      gameIndex = 0;
    }

    if (gameIndex >= SAVED_GAME_COUNT) {
      gameIndex = SAVED_GAME_COUNT - 1;
    }

    if (save->savedGames[gameIndex].isValid) {
      if (!save->savedGames[gameIndex].state.isMiniGolf) {
        func_80226190_ovl_01(&gfx);
      } else {
        func_8022644C_ovl_01(&gfx);
      }
    }

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    courseIndex = CHOSEN_GAME.state.courseIndex;
    holeIndex = CHOSEN_GAME.state.holeIndex;
    if (CHOSEN_GAME.state.gameMode == GAME_MODE_TOURNAMENT) {
      SetTextSlot(SAVED_GAME_COURSE_X, SAVED_GAME_COURSE_Y,
          MESSAGE_FIRST_TOURNAMENT_COURSE + courseIndex, 1, 0, 0, 0xFF, 0xFF,
          0xFF, 0xFF);
    } else {
      if (CHOSEN_GAME.state.isMiniGolf == TRUE) {
        courseIndex = (courseIndex >= FIRST_PEACHS_CASTLE_MINI_GOLF_COURSE)
            ? COURSE_RECORD_PEACHS_CASTLE
            : COURSE_RECORD_LUIGIS_GARDEN;
      }

      sprintf(
          g_PrintBuffer, D_80231A3C_ovl_01, g_CourseRecords[courseIndex].name);
      DrawLargeText(
          &gfx, SAVED_GAME_COURSE_X, SAVED_GAME_COURSE_Y, g_PrintBuffer);
    }

    SetTextSlotValue(holeIndex + 1, 1, 0);
    SetTextSlot(SAVED_GAME_HOLE_X, SAVED_GAME_HOLE_Y, MESSAGE_SAVED_GAME_HOLE,
        1, 1, 0, 0xFF, 0xFF, 0xFF, 0xFF);
  } else if (GetWipeKind() == 0) {
    if (D_80230B4D_ovl_01) {
      SetTextSlot(CONTINUE_PROMPT_X, PROMPT_Y, MESSAGE_CONTINUE_PROMPT, 1, 0, 1,
          0xFF, 0xFF, 0xFF, 0xFF);
    } else {
      SetTextSlotValue(D_8023BDA4_ovl_01[D_80230B48_ovl_01] + 1, 0, 0);

      /* The prompt names the player in the player's own colour */
      if (D_8023BDA4_ovl_01[D_80230B48_ovl_01] == 0) {
        SetTextPaletteColor(255, 127, 127, 0);
      }

      if (D_8023BDA4_ovl_01[D_80230B48_ovl_01] == 1) {
        SetTextPaletteColor(0, 255, 255, 0);
      }

      if (D_8023BDA4_ovl_01[D_80230B48_ovl_01] == 2) {
        SetTextPaletteColor(255, 255, 0, 0);
      }

      if (D_8023BDA4_ovl_01[D_80230B48_ovl_01] == 3) {
        SetTextPaletteColor(63, 255, 63, 0);
      }

      SetTextSlot(CONTROLLER_PROMPT_X, PROMPT_Y, MESSAGE_CONTROLLER_PROMPT, 1,
          0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
      for (index = 1; index != SAVED_GAME_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      }
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802230C0_ovl_01);
#endif

void func_80224AFC_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
  MenuMemory* memory = func_8005AF74();
  Gfx* gfx = *displayListHead;
  s32 iconHalfSize = D_8023BDB0_ovl_01;
  s32 iconScale = D_8023BDAC_ovl_01;
  EncodedAsset asset;
  s32 stripIndex;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  gDPLoadTLUT_pal256(gfx++,
      ((u32)(&D_8023BD98_ovl_01)[memory->savedGameIndex] + IMAGE_HEADER_SIZE) &
          ~7);
  for (stripIndex = 0; stripIndex != GAME_BOARD_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)(&D_8023BD98_ovl_01)[memory->savedGameIndex] +
            LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * GAME_BOARD_WIDTH * GAME_BOARD_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, GAME_BOARD_WIDTH, GAME_BOARD_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2,
        (top + (stripIndex * halfSize)) << 2, (left + halfSize + halfSize) << 2,
        (top + halfSize + (stripIndex * halfSize)) << 2, G_TX_RENDERTILE, 0, 0,
        scale, scale);
    gDPPipeSync(gfx++);
  }

  if (D_80230B50_ovl_01[memory->savedGameIndex] != -1) {
    image = D_8023BD74_ovl_01[D_8022F8C4_ovl_01];
    gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
              (stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, (left + MODE_ICON_OFFSET_X) << 2,
          (top + (stripIndex * iconHalfSize) + MODE_ICON_OFFSET_Y) << 2,
          (left + iconHalfSize + iconHalfSize + MODE_ICON_OFFSET_X) << 2,
          (top + iconHalfSize + (stripIndex * iconHalfSize) +
              MODE_ICON_OFFSET_Y)
              << 2,
          G_TX_RENDERTILE, 0, 0, iconScale, iconScale);
      gDPPipeSync(gfx++);
    }

    D_8022F8C5_ovl_01++;
    if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
      iconIndex = D_80230BB0_ovl_01[D_80230B50_ovl_01[memory->savedGameIndex]];
      D_8022F8C5_ovl_01 = 0;
      D_8022F8C0_ovl_01++;
      if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
        D_8022F8C0_ovl_01 = 0;
      }

      if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] ==
          END_OF_ICON_FRAMES) {
        D_8022F8C0_ovl_01 = 0;
      }

      D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
      OpenEncodedAsset(
          g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01], &asset);
      ReadEncodedAsset(D_8023BD74_ovl_01[D_8022F8C4_ovl_01], &asset);
    }
  }

  *displayListHead = gfx;
}

/*
 * Doesn't match: the original computes each fidget timer's address with
 * the table's address in $v1 and the scaled index in $v0, the other way
 * round.
 */
#ifdef NON_MATCHING
void func_80225358_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
  UnkB8* players = save->savedPlayers[memory->savedGameIndex];
  u16 playerIndex;
  s32* fidgetTimer;
  s32 raise;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  s32 unused[12];
  Mat4 transform;
  Mat4 viewing;

  if (!save->savedGames[memory->savedGameIndex].isValid) {
    return;
  }

  switch (playerCount) {
    case 1:
      D_8023BD20_ovl_01[0] = 5.0f;
      break;
    case 2:
      D_8023BD20_ovl_01[0] = 4.0f;
      D_8023BD20_ovl_01[1] = 4.0f;
      break;
    case 3:
    case 4:
      D_8023BD20_ovl_01[0] = 3.5f;
      D_8023BD20_ovl_01[1] = 3.4f;
      D_8023BD20_ovl_01[2] = 3.4f;
      D_8023BD20_ovl_01[3] = 3.5f;
      break;
    default:
      D_8023BD20_ovl_01[0] = 4.0f;
      D_8023BD20_ovl_01[1] = 4.0f;
      D_8023BD20_ovl_01[2] = 4.0f;
      D_8023BD20_ovl_01[3] = 4.0f;
      break;
  }

  guOrtho(&D_8023BCE0_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CHARACTERS_NEAR,
      MENU_CHARACTERS_FAR, MENU_CHARACTERS_ORTHO_SCALE);
  guLookAtReflectF(viewing, &D_8023BD30_ovl_01[g_DrawBufferIndex], 0.0f,
      MENU_CHARACTERS_EYE_Y, MENU_CHARACTERS_EYE_Z, 0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 0.0f);
  gSPLookAt(gfx++, &D_8023BD30_ovl_01[g_DrawBufferIndex]);
  for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
    fidgetTimer = &D_8023BFC0_ovl_01[playerIndex];
    if (*fidgetTimer > 0) {
      (*fidgetTimer)--;
    }

    if (*fidgetTimer == 0) {
      *fidgetTimer = -1;
      SetAnimationLooping(playerIndex, FALSE);
      SetNextAnimation(playerIndex, MENU_ANIMATION_FIDGET);
    }

    if (*fidgetTimer == -1) {
      if (GetAnimationEndFrame(playerIndex, MENU_ANIMATION_FIDGET) ==
          GetAnimationFrame(playerIndex)) {
        SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
        SetAnimationLooping(playerIndex, TRUE);
        *fidgetTimer = guRandom() % FIDGET_DELAY_RANGE;
      }
    }

    if (!D_801B70F0[playerIndex]) {
      if (IsCharacterSlotBusy(playerIndex)) {
        FreeCharacterSlot(playerIndex);
      } else {
        LoadCharacter(playerIndex,
            (s8)players[playerIndex].characterIndex + CHARACTER_COUNT,
            (s8)players[playerIndex].paletteIndex);
        D_801B70F0[playerIndex] = TRUE;
        SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
        SetAnimationSpeed(playerIndex, 1.0f);
      }
    }

    rotation[0] = DEGREES_TO_RADIANS(D_80230D18_ovl_01[0]);
    rotation[1] = DEGREES_TO_RADIANS(D_80230D18_ovl_01[1]);
    rotation[2] = 0.0f;
    raise = ((s8)players[playerIndex].characterIndex == CHARACTER_BABY_MARIO)
        ? BABY_MARIO_RAISE
        : 0;
    translation[0] = D_80230D28_ovl_01[playerCount - 1][playerIndex][0];
    translation[1] = D_80230D28_ovl_01[playerCount - 1][playerIndex][1] + raise;
    translation[2] = D_80230D28_ovl_01[playerCount - 1][playerIndex][2];
    scale[0] = D_8023BD20_ovl_01[playerIndex];
    scale[1] = D_8023BD20_ovl_01[playerIndex];
    scale[2] = D_8023BD20_ovl_01[playerIndex];
    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(
        transform, &D_8023BAE0_ovl_01[playerIndex][g_DrawBufferIndex]);
    AnimateCharacter(playerIndex);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023BCE0_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&D_8023BAE0_ovl_01[playerIndex][g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
    DrawCharacter(playerIndex);
    *displayListHead = g_DisplayListHead;
    gfx = *displayListHead;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80225358_ovl_01);
#endif

/* Where a saved game's screen puts each player's controller icon and label, by
 * the player count */
extern s32 D_80230DE8_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT][2];

void func_80225960_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
  s32 position[2];
  s32 playerIndex;
  s32 labelIndex;
  s16 controller;
  void* image;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  image = D_8023BD80_ovl_01;
  gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
    if (D_80230B40_ovl_01[playerIndex] != NO_CONTROLLER) {
      position[0] = D_80230DE8_ovl_01[playerCount - 1][playerIndex][0];
      position[1] = D_80230DE8_ovl_01[playerCount - 1][playerIndex][1];
      controller = D_80230B40_ovl_01[playerIndex];
      if (controller != COMPUTER_CONTROLLER) {
        gDPLoadTextureBlock(gfx++,
            ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
                (controller * CONTROLLER_ICON_IMAGE_SIZE)) &
                ~7,
            G_IM_FMT_CI, G_IM_SIZ_8b, CONTROLLER_ICON_SIZE,
            CONTROLLER_ICON_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, position[0] << 2, position[1] << 2,
            (position[0] + CONTROLLER_ICON_SIZE) << 2,
            (position[1] + CONTROLLER_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
            TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
      }
    }
  }

  image = D_8023BD84_ovl_01;
  gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
    position[0] = D_80230DE8_ovl_01[playerCount - 1][playerIndex][0];
    position[1] = D_80230DE8_ovl_01[playerCount - 1][playerIndex][1];
    if (D_80230B40_ovl_01[playerIndex] == COMPUTER_CONTROLLER) {
      if (save->savedGames[memory->savedGameIndex].state.gameMode ==
          GAME_MODE_GET_CHARACTER) {
        labelIndex = PLAYER_LABEL_COM;
      } else {
        labelIndex = playerIndex + PLAYER_LABEL_COMPUTER_FIRST;
      }

      gDPLoadTextureBlock_4b(gfx++,
          ((u32)image + PALETTED_TEXELS_OFFSET +
              (labelIndex * PLAYER_LABEL_IMAGE_SIZE)) &
              ~7,
          G_IM_FMT_CI, PLAYER_LABEL_WIDTH, PLAYER_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
      gDPLoadTextureBlock_4b(gfx++,
          ((u32)image + PALETTED_TEXELS_OFFSET +
              (playerIndex * PLAYER_LABEL_IMAGE_SIZE)) &
              ~7,
          G_IM_FMT_CI, PLAYER_LABEL_WIDTH, PLAYER_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }

    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (position[0] + PLAYER_LABEL_OFFSET_X) << 2,
        (position[1] + PLAYER_LABEL_OFFSET_Y) << 2,
        (position[0] + PLAYER_LABEL_OFFSET_X + PLAYER_LABEL_WIDTH) << 2,
        (position[1] + PLAYER_LABEL_OFFSET_Y + PLAYER_LABEL_HEIGHT) << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

void func_80226190_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripTop;

  GetSaveData();
  func_8005AF74();
  gDPLoadTLUT_pal256(gfx++,
      ((u32)(&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != COURSE_THUMBNAIL_STRIP_COUNT;
      stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)(&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01] +
            LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * COURSE_THUMBNAIL_WIDTH *
                COURSE_THUMBNAIL_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_THUMBNAIL_WIDTH,
        COURSE_THUMBNAIL_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop =
        COURSE_THUMBNAIL_TOP + (stripIndex * COURSE_THUMBNAIL_SHOWN_HEIGHT);
    gSPScisTextureRectangle(gfx++, COURSE_THUMBNAIL_LEFT << 2, stripTop << 2,
        (COURSE_THUMBNAIL_LEFT + COURSE_THUMBNAIL_SHOWN_WIDTH) << 2,
        (stripTop + COURSE_THUMBNAIL_SHOWN_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        (COURSE_THUMBNAIL_WIDTH << 10) / COURSE_THUMBNAIL_SHOWN_WIDTH,
        (COURSE_THUMBNAIL_STRIP_HEIGHT << 10) / COURSE_THUMBNAIL_SHOWN_HEIGHT);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

void func_8022644C_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripTop;
  void* palette;

  GetSaveData();
  func_8005AF74();
  palette = D_801052C4[0];
  gDPLoadTLUT_pal256(gfx++, (u32)palette & ~7);
  for (stripIndex = 0; stripIndex != COURSE_THUMBNAIL_STRIP_COUNT;
      stripIndex++) {
    /* The course and chosen course buffers are adjacent; D_80230B4E picks one
     */
    gDPLoadTextureBlock(gfx++,
        ((u32)(&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01] +
            (stripIndex * COURSE_THUMBNAIL_WIDTH *
                COURSE_THUMBNAIL_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_THUMBNAIL_WIDTH,
        COURSE_THUMBNAIL_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop =
        COURSE_THUMBNAIL_TOP + (stripIndex * COURSE_THUMBNAIL_SHOWN_HEIGHT);
    gSPScisTextureRectangle(gfx++, COURSE_THUMBNAIL_LEFT << 2, stripTop << 2,
        (COURSE_THUMBNAIL_LEFT + COURSE_THUMBNAIL_SHOWN_WIDTH) << 2,
        (stripTop + COURSE_THUMBNAIL_SHOWN_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        (COURSE_THUMBNAIL_WIDTH << 10) / COURSE_THUMBNAIL_SHOWN_WIDTH,
        (COURSE_THUMBNAIL_STRIP_HEIGHT << 10) / COURSE_THUMBNAIL_SHOWN_HEIGHT);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
