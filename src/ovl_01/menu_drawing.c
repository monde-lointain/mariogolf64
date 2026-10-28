/*
 * Draws most of the menu screens: the main menu, the clubhouse, the options,
 * the help and birdie badge screens, and screens 5, 6, 8, 18, 20, 22, 23 and
 * 26. It also draws the menus' labels, the chosen mode's icon, the players'
 * characters and the waving Toad, and holds a debug character viewer and a
 * debug caddy select.
 */
#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "character.h"
#include "character_slot.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "game.h"
#include "game_flags.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "variables.h"

/* The STATUS, HOW TO PLAY, CODE ENTRY and OPTIONS labels, 128 by 20 each, plain
 * in D_801B5CD0 and lit in D_8012D420 */
#define MAIN_CHOICE_LABEL_WIDTH 128
#define MAIN_CHOICE_LABEL_HEIGHT 20
#define MAIN_CHOICE_LABEL_IMAGE_SIZE \
  (MAIN_CHOICE_LABEL_WIDTH * MAIN_CHOICE_LABEL_HEIGHT / 2)

/* Labels in the shadowed font */
extern char g_ContinueLabelText[];
extern char g_BirdieBadgeLabelText[];

extern s32 D_8022F8C8_ovl_01;
extern s8 D_8022F8CD_ovl_01;
extern u8 D_8022F8CC_ovl_01; /* Which of D_801B93E0's buffers holds the star
                                frame shown */
extern void* D_801B93E0[];
extern s32 D_8022F848_ovl_01[]; /* The star's frames */
extern u8 D_8022F8CE_ovl_01;
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern void* D_8010526C[2];
extern void* D_800FBE30[2];

/* The 24x24 icon that func_80207E28 animates, such as the birdie badge; each
 * frame shows for two calls */
#define ANIMATED_ICON_SIZE 24
#define ANIMATED_ICON_IMAGE_SIZE (ANIMATED_ICON_SIZE * ANIMATED_ICON_SIZE / 2)
#define ANIMATED_ICON_FRAME_COUNT 6
#define ANIMATED_ICON_FRAME_DURATION 2
/* func_802081E8 draws a row of spinning stars, one per hole, from D_801B93E0,
 * and enlarges the chosen hole's */
#define STAR_WIDTH 32
#define STAR_HEIGHT 24
#define STAR_FRAME_COUNT 15
#define MAX_STAR_COUNT 6
#define CHOSEN_STAR_GROWTH 8
/* func_80200A40 draws the chosen mode's icon, 64x64 CI8, in two strips */
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
/* Where func_802004A0 draws the mode icon, unscaled */
#define MODE_ICON_LEFT 168
#define MODE_ICON_TOP 96

/* func_80200EF0 draws the players' characters, and in Get Character the rival
 * as a spinning silhouette */
#define NO_RIVAL -1
#define RIVAL_SLOT 1
#define RIVAL_SPIN_STEP 10.0f
#define MENU_CHARACTER_LAYOUT_COUNT 4
#define MENU_CHARACTER_SLOT_COUNT 5

extern f32 D_8022FA54_ovl_01;
extern f32 D_8022FA58_ovl_01;
extern f32 D_8022FA5C_ovl_01;
extern f32 D_8022FA68_ovl_01[MENU_CHARACTER_LAYOUT_COUNT]
                            [MENU_CHARACTER_SLOT_COUNT][3];
extern s32
    D_8022FB58_ovl_01[CHARACTER_COUNT]; /* The character for each grid mark */
extern Mtx D_80231D60_ovl_01[MAX_PLAYER_COUNT][2];
extern Mtx D_80231F60_ovl_01;
extern f32 D_80231FA0_ovl_01[MAX_PLAYER_COUNT];
extern s32 D_8023BFC0_ovl_01[MAX_PLAYER_COUNT];
extern u8 D_801B70F0[MAXCONTROLLERS];

/* func_80208F8C draws a frame of the waving Toad (assets 0x710 onward), 48x51
 * CI8, in two strips */
#define TOAD_WIDTH 48
#define TOAD_STRIP_HEIGHT 30
#define TOAD_LAST_STRIP_HEIGHT 21
#define TOAD_STRIP_COUNT 2

/* Where the debug caddy select draws its title and names */
#define CADDY_SELECT_TITLE_X 64
#define CADDY_SELECT_TITLE_Y 32
#define CADDY_SELECT_NAME_X 96
#define CADDY_SELECT_FIRST_Y 64
#define CADDY_SELECT_LINE_HEIGHT 8

extern char* D_8022C0D0_ovl_01[CADDY_COUNT];
extern const char D_8023176C_ovl_01[];

/* The D_8022BE88 phase in which the course select's banners slide the other way
 */
#define MENU_PHASE_UNK7 7

/*
 * The mode select's boards turn on a circle like the saved games' boards,
 * wider for a single player; the chosen one shows its animated mode icon once
 * it stops turning
 */
#define MODE_SELECT_BOARD_KIND 0x1B
#define MODE_SELECT_LIGHT_YAW 0
#define MODE_SELECT_LIGHT_PITCH 0x3000
#define MODE_BOARD_EYE_HEIGHT 70.0f
#define MODE_BOARD_EYE_DISTANCE 60.0f
#define MODE_BOARD_RADIUS_X 48.0f
#define SINGLE_PLAYER_MODE_BOARD_RADIUS_X 64.0f
#define MODE_BOARD_RADIUS_Z 40.0f
#define MODE_BOARD_HALF_SIZE 32.0f
/* The boards not chosen are darkened */
#define UNCHOSEN_MODE_BOARD_SHADE 127
/* The bob that a board past the last would have, a step of its phase per
 * update */
#define MODE_BOARD_BOB_PHASE_STEP 0.1f
#define MODE_BOARD_BOB_HEIGHT 8.0f
/* The strips of D_801B60D0 below the boards, drawn wider for a single player */
#define MODE_BANNER_TOP 152
#define MODE_BANNER_LEFT 80
#define MODE_BANNER_SHOWN_WIDTH 160
#define SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH 180
/* The rows of the modes' label image, 192 by 20 CI4 each */
#define MODE_LABEL_WIDTH 192
#define MODE_LABEL_HEIGHT 20
#define MODE_LABEL_IMAGE_SIZE ((MODE_LABEL_WIDTH * MODE_LABEL_HEIGHT) / 2)
#define MODE_LABEL_X 160
#define SINGLE_PLAYER_MODE_LABEL_X 170
#define MODE_LABEL_TOP 156
#define MODE_ARROW_GAP 64
#define MODE_ARROW_Y 124
/* A single player's Tournament shows the birdie badges won, and Ring Shot the
 * stars won, centered by the count's number of digits */
#define WON_COUNT_ICON_X 224
#define WON_COUNT_ICON_Y 64
#define WON_COUNT_X 250
#define WON_COUNT_Y 72
#define WON_COUNT_DIGIT_WIDTH 16

extern s8 D_800C1FF4[MAX_PLAYER_COUNT];
extern s8 D_800C1FF8[MAX_PLAYER_COUNT];
extern void* D_800FC898;
extern void* D_800FF180[2];
extern void* D_8012F4B0[7];
extern f32 D_8022F978_ovl_01;
/* Where each player's controller icon and label go, by the player count */
extern s32 D_8022F97C_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT][2];
/* How far left of the label's place each row of the modes' label image is
 * drawn */
extern s32 D_8022FA28_ovl_01[MODE_ICON_COUNT];
extern Mat4 D_80231CA0_ovl_01;
extern Mat4 D_80231CE0_ovl_01;
extern LookAt D_80231D20_ovl_01[2];

/*
 * The course select's water plane, D_8022FD48, blends the water image and the
 * scrolling layer, which scroll different ways, and is fogged white
 */
#define WATER_PLANE_DISTANCE 300.0f
#define WATER_FIELD_OF_VIEW 45.0f
#define WATER_PERSPECTIVE_SCALE 4.0f
#define WATER_FOG_START 980
#define WATER_FOG_END 1050
#define WATER_BLEND_ALPHA 127
#define WATER_TEXTURE_SIZE 32
#define WATER_TEXTURE_MASK 5
#define WATER_TEXTURE_SHIFT 2
#define WATER_LAYER_TMEM 0x100
#define WATER_LAYER_TILE 1
#define WATER_TILE_SPAN 64
#define WATER_VERTEX_COUNT 5
/* The course select's wheel is seen from closer than the other wheels */
#define COURSE_SELECT_EYE_HEIGHT 50.0f
#define COURSE_SELECT_EYE_DISTANCE 60.0f
/* The front course's image bobs, a step of its phase per update, and the
 * chosen one updates the depth buffer at a fixed depth */
#define FRONT_COURSE_BOB_PHASE_STEP 0.1f
#define FRONT_COURSE_BOB_HEIGHT 8.0f
#define CHOSEN_COURSE_DEPTH 0x7FFF
/* The course after the last open one is grayed out, with a panel over it
 * typing the points it needs */
#define LOCKED_COURSE_SHADE 96
#define LOCKED_PANEL_LEFT 108
#define LOCKED_PANEL_TOP 100
#define LOCKED_PANEL_RIGHT 212
#define LOCKED_PANEL_BOTTOM 160
#define LOCKED_COURSE_TEXT_X 111
#define LOCKED_COURSE_TEXT_Y 116
#define MESSAGE_POINTS_TO_UNLOCK 0xC2
/* The course's birdie badge icon and count, centered by its number of digits
 */
#define COURSE_BADGE_ICON_X 226
#define COURSE_BADGE_ICON_Y 200
#define COURSE_BADGE_COUNT_X 252
#define COURSE_BADGE_COUNT_Y 208
/* The holes badges of D_80106100, 24 by 24 CI4 each: the chosen one is lit,
 * and the Bet badge cycles its color unless D_800C2BD5 is set */
#define HOLES_BADGE_BET 0
#define HOLES_BADGE_COUNT 4
#define HOLES_BADGE_SIZE 24
#define HOLES_BADGE_IMAGE_SIZE ((HOLES_BADGE_SIZE * HOLES_BADGE_SIZE) / 2)
#define UNCHOSEN_HOLES_BADGE_SHADE 127
#define BET_BADGE_COLOR_STEP (M_PI / 18)
#define BET_BADGE_COLOR_MIDPOINT 127
#define BET_BADGE_COLOR_SWING 127.0f
#define BET_BADGE_ENV_DIVISOR 4
/* func_801FB54C's layers drawn behind the course select, and in front of it
 * and of screens 20 and 22 */
#define COURSE_SELECT_BACK_BOARD_KIND 1
#define COURSE_SELECT_FRONT_BOARD_KIND 10
/* Screens 20 and 22 slide their pages this far, a step per update */
#define BET_SLIDE_STEP 32
#define BET_SLIDE_DISTANCE 512

typedef struct ImageCorner {
  s16 x;
  s16 y;
} ImageCorner;

extern Mtx D_802321F0_ovl_01;
extern Mtx D_80232230_ovl_01;
extern Mat4 D_80232270_ovl_01;
extern Mat4 D_802322B0_ovl_01;
extern s32 D_8022FD98_ovl_01; /* The water's scroll */
extern Vtx D_8022FD48_ovl_01[WATER_VERTEX_COUNT];
extern f32 D_8022FBC0_ovl_01; /* The front course's bob phase */
extern f32 D_8022FD40_ovl_01; /* The Bet badge's color phase */
extern f32 D_8022FD44_ovl_01; /* The chosen mini-golf image's center Y */
extern ImageCorner D_8022FC20_ovl_01[HOLES_BADGE_COUNT];
/* Each course's trophies' corners, by the number of trophies */
extern ImageCorner D_8022FC48_ovl_01[COURSE_COUNT][TOURNAMENT_PLACE_COUNT]
                                    [TOURNAMENT_PLACE_COUNT];
extern void* D_801F4A20;
extern void* D_801049A0[][MINI_GOLF_DISC_IMAGE_COUNT];
extern void* D_801052C4[MINI_GOLF_DISC_IMAGE_COUNT];
extern u8 D_8022F8DD_ovl_01;
extern void* D_80106100;
extern u8 D_800C2BD5;
extern void* D_800FBDFC[2];
extern u8 D_8022BE80_ovl_01; /* Which of D_800FBDFC's buffers is shown */
extern TrophySize D_800C20E0[COURSE_COUNT];
extern u8 D_8022BE89_ovl_01;
extern s32 D_8022BE84_ovl_01;
extern s32 D_802322F0_ovl_01;
extern s32 D_802322F4_ovl_01;

void StartLoadingChosenHole(void);
void func_801FC60C_ovl_01(Gfx** displayListHead, s32 layers);
void func_80200A40_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 halfSize, s32 scale);
void func_80200EF0_ovl_01(Gfx** displayListHead);
void func_802047F0_ovl_01(Gfx** displayListHead);
void func_80207E28_ovl_01(Gfx** displayListHead, void* image, s32 x, s32 y);
void func_802081E8_ovl_01(Gfx** displayListHead, s32 left, s32 top,
    u8 starCount, u8 enlargesChosenHole);
void func_80210DF4_ovl_01(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void func_8021826C_ovl_01(
    Gfx** displayListHead, s32 slide, u8 isSlideReversed, u8 isSettled);
void func_80219BD4_ovl_01(Gfx** displayListHead, s32 slide,
    u8 reverseVerticalSlide, u8 canShowPrompt);
s32 func_8021B618_ovl_01(Gfx** displayListHead, u8 slide);
void func_8021BE88_ovl_01(Gfx** displayListHead);
void func_8021C200_ovl_01(Gfx** displayListHead, s32 gap, s32 centerY);
void func_8021CBDC_ovl_01(Gfx** displayListHead, s32 centerY);

/*
 * Draws the mode select of screens 5, 23 and 26: the characters, the modes'
 * boards turning on their circle, the chosen mode's label, each player's
 * controller icon and label, and for a single player the birdie badges or
 * stars won.
 */
/* Doesn't match: the boards' strips and the mode label schedule their
 * address loads differently */
#ifdef NON_MATCHING
void func_801FE4D0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  u16 perspNorm;
  /* Sized like the course wheel's */
  f32 boardX[WHEEL_SLOT_LIMIT];
  f32 boardY[WHEEL_SLOT_LIMIT];
  f32 boardScale[WHEEL_SLOT_LIMIT];
  s32 unusedSlots[WHEEL_SLOT_LIMIT];
  s32 drawOrder[WHEEL_SLOT_LIMIT];
  s32 position[2];
  SaveData* save = GetSaveData();
  s16 badgeCount = 0;
  s16 starCount;
  s32 modeCount;
  s32 index;
  s32 otherIndex;
  s32 key;
  f32 radiusX;
  s32 angleStep;
  f32 angle;
  f32 circleX;
  /* A board's turns from the chosen one, then the slot of the board drawn */
  s32 slot;
  s32 halfSize;
  s32 step;
  f32 bob;
  u8 areCharactersDrawn;
  s32 labelIndex;
  s32 labelX;
  s32* labelOffset;
  void* image;
  s32 countShift;
  s32 courseIndex;
  s32 holeIndex;

  g_HoleSetup.conditions.lightYaw = MODE_SELECT_LIGHT_YAW;
  g_HoleSetup.conditions.lightPitch = MODE_SELECT_LIGHT_PITCH;
  func_801FC60C_ovl_01(&gfx, MODE_SELECT_BOARD_KIND);
  guPerspectiveF(D_80231CA0_ovl_01, &perspNorm, WHEEL_FIELD_OF_VIEW,
      WHEEL_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE, 1.0f);
  guLookAtReflectF(D_80231CE0_ovl_01, &D_80231D20_ovl_01[g_DrawBufferIndex],
      0.0f, MODE_BOARD_EYE_HEIGHT, MODE_BOARD_EYE_DISTANCE, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f);
  gSPLookAt(gfx++, &D_80231D20_ovl_01[g_DrawBufferIndex]);
  SetCpuProjectionF(D_80231CA0_ovl_01, D_80231CE0_ovl_01, g_Viewports);
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

  modeCount = D_8022BE30_ovl_01[g_ChosenPlayerCount - 1];
  for (index = 0; index != modeCount; index++) {
    ScreenPoint point;

    radiusX = MODE_BOARD_RADIUS_X;
    if (g_ChosenPlayerCount == 1) {
      radiusX = SINGLE_PLAYER_MODE_BOARD_RADIUS_X;
    }

    slot = (D_801323A0.modeChoice - index) % modeCount;
    if (modeCount != 0) {
      angleStep = DEGREES_PER_TURN / modeCount;
    } else {
      angleStep = DEGREES_PER_TURN;
    }

    angle = -DEGREES_TO_RADIANS(
        (slot * angleStep) + (D_801323A0.modeSpin * DEGREES_PER_SPIN_STEP));
    circleX = sinf(angle) * radiusX;
    if (ProjectPoint(circleX, 0.0f, cosf(angle) * MODE_BOARD_RADIUS_Z, &point) <
        0) {
      boardX[index] = point.x;
      boardY[index] = point.y - WHEEL_RAISE;
      boardScale[index] = point.scaleX;
    }
  }

  for (otherIndex = 0; otherIndex != modeCount; otherIndex++) {
    drawOrder[otherIndex] = otherIndex;
  }

  /* Draw the farthest boards first, so the nearer ones cover them */
  for (otherIndex = 1; otherIndex != modeCount; otherIndex++) {
    key = drawOrder[otherIndex];
    for (index = otherIndex - 1;
        (index >= 0) && (boardY[drawOrder[index]] > boardY[key]); index--) {
      drawOrder[index + 1] = drawOrder[index];
    }

    drawOrder[index + 1] = key;
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

  areCharactersDrawn = FALSE;
  D_8022F978_ovl_01 += MODE_BOARD_BOB_PHASE_STEP;
  for (index = 0; index != modeCount; index++) {
    slot = drawOrder[index];
    halfSize = boardScale[slot] * MODE_BOARD_HALF_SIZE;
    step = (f32)(MODE_ICON_STRIP_HEIGHT << 10) / halfSize;
    /* The loop stops before modeCount, so no board bobs */
    if (index == modeCount) {
      bob = sinf(D_8022F978_ovl_01) * MODE_BOARD_BOB_HEIGHT;
    } else {
      bob = 0.0f;
    }

    gDPLoadTLUT_pal256(gfx++, ((u32)D_8012F4B0[slot] + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    if ((D_801323A0.modeChoice == slot) && (D_801323A0.modeSpin == 0)) {
      if (!areCharactersDrawn) {
        func_80200EF0_ovl_01(&gfx);
        areCharactersDrawn = TRUE;
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
      }

      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      func_80200A40_ovl_01(&gfx, boardX[slot] - halfSize,
          boardY[slot] - halfSize + bob, halfSize, step);
    } else {
      if (D_801323A0.modeChoice == slot) {
        /* The characters go behind the chosen board, and change the state */
        if (!areCharactersDrawn) {
          func_80200EF0_ovl_01(&gfx);
          gDPPipeSync(gfx++);
          gDPPipeSync(gfx++);
          gDPSetCycleType(gfx++, G_CYC_1CYCLE);
          gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
          gDPPipeSync(gfx++);
          gDPSetTexturePersp(gfx++, G_TP_NONE);
          gDPPipeSync(gfx++);
          gDPSetTextureFilter(gfx++, G_TF_BILERP);
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
          gDPPipeSync(gfx++);
          gDPSetTextureLUT(gfx++, G_TT_RGBA16);
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
          gDPLoadTLUT_pal256(
              gfx++, ((u32)D_8012F4B0[slot] + IMAGE_HEADER_SIZE) & ~7);
          areCharactersDrawn = TRUE;
        }
      } else {
        gDPSetCombineMode(
            gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, UNCHOSEN_MODE_BOARD_SHADE,
            UNCHOSEN_MODE_BOARD_SHADE, UNCHOSEN_MODE_BOARD_SHADE, 255);
      }

      for (otherIndex = 0; otherIndex != MODE_ICON_STRIP_COUNT; otherIndex++) {
        gDPLoadTextureBlock(gfx++,
            ((u32)D_8012F4B0[slot] + LARGE_PALETTED_TEXELS_OFFSET +
                (otherIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)) &
                ~7,
            G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT,
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
    }
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  BuildDynamicTextures(&gfx);

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);

  gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
  if (g_ChosenPlayerCount == 1) {
    for (index = 0; index != MENU_BANNER_STRIP_COUNT; index++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
              (index * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, MODE_BANNER_LEFT << 2,
          (MODE_BANNER_TOP + (index * MENU_BANNER_STRIP_HEIGHT)) << 2,
          (MODE_BANNER_LEFT + SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH) << 2,
          (MODE_BANNER_TOP + MENU_BANNER_STRIP_HEIGHT +
              (index * MENU_BANNER_STRIP_HEIGHT))
              << 2,
          G_TX_RENDERTILE, 0, 0,
          (MENU_BANNER_WIDTH << 10) / SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH,
          TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  } else {
    for (index = 0; index != MENU_BANNER_STRIP_COUNT; index++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
              (index * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, MODE_BANNER_LEFT << 2,
          (MODE_BANNER_TOP + (index * MENU_BANNER_STRIP_HEIGHT)) << 2,
          (MODE_BANNER_LEFT + MODE_BANNER_SHOWN_WIDTH) << 2,
          (MODE_BANNER_TOP + MENU_BANNER_STRIP_HEIGHT +
              (index * MENU_BANNER_STRIP_HEIGHT))
              << 2,
          G_TX_RENDERTILE, 0, 0,
          (MENU_BANNER_WIDTH << 10) / MODE_BANNER_SHOWN_WIDTH, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  labelIndex =
      D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
          (labelIndex * MODE_LABEL_IMAGE_SIZE)) &
          ~7,
      G_IM_FMT_CI, MODE_LABEL_WIDTH, MODE_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);

  labelOffset = &D_8022FA28_ovl_01[labelIndex];
  labelX =
      (g_ChosenPlayerCount == 1) ? SINGLE_PLAYER_MODE_LABEL_X : MODE_LABEL_X;
  gSPScisTextureRectangle(gfx++, (labelX - *labelOffset) << 2,
      MODE_LABEL_TOP << 2, (labelX - *labelOffset + MODE_LABEL_WIDTH) << 2,
      (MODE_LABEL_TOP + MODE_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);

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

  image = D_800FF180[0];
  gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  for (index = 0; index != g_ChosenPlayerCount; index++) {
    if (D_800C1FF4[index] != NO_CONTROLLER_INDEX) {
      position[0] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][0];
      position[1] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][1];
      gDPLoadTextureBlock(gfx++,
          ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
              (D_800C1FF8[index] * CONTROLLER_ICON_IMAGE_SIZE)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, CONTROLLER_ICON_SIZE, CONTROLLER_ICON_SIZE,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, position[0] << 2, position[1] << 2,
          (position[0] + CONTROLLER_ICON_SIZE) << 2,
          (position[1] + CONTROLLER_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  image = D_800FF180[1];
  gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  for (index = 0; index != g_ChosenPlayerCount; index++) {
    position[0] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][0];
    position[1] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][1];
    if (D_800C1FF4[index] == NO_CONTROLLER_INDEX) {
      gDPLoadTextureBlock_4b(gfx++,
          ((u32)image + PALETTED_TEXELS_OFFSET +
              ((index + PLAYER_LABEL_COMPUTER_FIRST) *
                  PLAYER_LABEL_IMAGE_SIZE)) &
              ~7,
          G_IM_FMT_CI, PLAYER_LABEL_WIDTH, PLAYER_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
      gDPLoadTextureBlock_4b(gfx++,
          ((u32)image + PALETTED_TEXELS_OFFSET +
              (index * PLAYER_LABEL_IMAGE_SIZE)) &
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

  func_8021C200_ovl_01(&gfx, MODE_ARROW_GAP, MODE_ARROW_Y);

  if ((g_ChosenPlayerCount == 1) &&
      (D_8022BEBC_ovl_01[0][D_801323A0.modeChoice] == MODE_ICON_TOURNAMENT)) {
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
      for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        if (save->unk990.birdieBadgeCounts[courseIndex][holeIndex] > 0) {
          badgeCount++;
        }
      }
    }

    countShift = 0;
    if (badgeCount < 10) {
      countShift = WON_COUNT_DIGIT_WIDTH;
    }

    if ((badgeCount >= 10) && (badgeCount < 100)) {
      countShift = 0;
    }

    if (badgeCount >= 100) {
      countShift = -WON_COUNT_DIGIT_WIDTH;
    }

    func_80207E28_ovl_01(
        &gfx, D_800FC898, WON_COUNT_ICON_X + countShift, WON_COUNT_ICON_Y);
    gDPPipeSync(gfx++);
    func_80210DF4_ovl_01(&gfx, WON_COUNT_X + countShift, WON_COUNT_Y,
        badgeCount, MENU_NUMBER_TIMES);
    gDPPipeSync(gfx++);
  }

  if ((g_ChosenPlayerCount == 1) &&
      (D_8022BEBC_ovl_01[0][D_801323A0.modeChoice] == MODE_ICON_RING_SHOT)) {
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT;
        courseIndex++) {
      for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
        if (save->hasRingShotStar[courseIndex][holeIndex] > 0) {
          starCount++;
        }
      }
    }

    countShift = 0;
    if (starCount < 10) {
      countShift = WON_COUNT_DIGIT_WIDTH;
    }

    /* This tests the badge count, which is zero here, for the star count */
    if ((starCount >= 10) && (badgeCount < 100)) {
      countShift = 0;
    }

    if (starCount >= 100) {
      countShift = -WON_COUNT_DIGIT_WIDTH;
    }

    func_802081E8_ovl_01(
        &gfx, WON_COUNT_ICON_X + countShift, WON_COUNT_ICON_Y, 1, FALSE);
    gDPPipeSync(gfx++);
    func_80210DF4_ovl_01(&gfx, WON_COUNT_X + countShift, WON_COUNT_Y, starCount,
        MENU_NUMBER_TIMES);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_801FE4D0_ovl_01);
#endif

/* Nothing calls this */
void func_802004A0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 stripIndex;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  image = D_8010526C[D_8022F8C4_ovl_01];
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
    gSPScisTextureRectangle(gfx++, MODE_ICON_LEFT << 2,
        (MODE_ICON_TOP + (stripIndex * MODE_ICON_STRIP_HEIGHT)) << 2,
        (MODE_ICON_LEFT + MODE_ICON_WIDTH) << 2,
        (MODE_ICON_TOP + MODE_ICON_STRIP_HEIGHT +
            (stripIndex * MODE_ICON_STRIP_HEIGHT))
            << 2,
        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  }

  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
    iconIndex =
        D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
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
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
  }

  *displayListHead = gfx;
}

void func_80200A40_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 stripIndex;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  image = D_8010526C[D_8022F8C4_ovl_01];
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
    gSPScisTextureRectangle(gfx++, left << 2,
        (top + (stripIndex * halfSize)) << 2, (left + halfSize + halfSize) << 2,
        (top + halfSize + (stripIndex * halfSize)) << 2, G_TX_RENDERTILE, 0, 0,
        scale, scale);
    gDPPipeSync(gfx++);
  }

  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
    iconIndex =
        D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
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
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
  }

  *displayListHead = gfx;
}

/*
 * Doesn't match: the original fills the delay slot of the rival test before
 * SetCharacterLighting with its first argument, and this leaves it empty.
 */
#ifdef NON_MATCHING
void func_80200EF0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 rival = NO_RIVAL;
  MenuMemory* memory;
  s32 characterCount;
  s32 row;
  s32 column;
  s32 layout;
  s32 character;
  s32 palette;
  s32 raise;
  u16 slot;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  s32 unused[12];
  Mat4 transform;

  memory = func_8005AF74();
  characterCount = g_ChosenPlayerCount;
  if (D_8022BEBC_ovl_01[characterCount - 1][memory->modeChoice] ==
      MODE_ICON_GET_CHARACTER) {
    if (characterCount == 1) {
      for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
          if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
            rival = D_8022FB58_ovl_01[g_CharacterGrid[row][column] +
                MARKED_CHARACTER_OFFSET];
          }
        }
      }
    }
  }

  characterCount += (rival >= 0);
  switch (characterCount) {
    case 1:
      D_80231FA0_ovl_01[0] = 5.0f;
      break;
    case 2:
      D_80231FA0_ovl_01[0] = (rival >= 0) ? 5.0f : 4.0f;
      D_80231FA0_ovl_01[1] = 4.0f;
      break;
    case 3:
    case 4:
      D_80231FA0_ovl_01[0] = 3.5f;
      D_80231FA0_ovl_01[1] = 3.4f;
      D_80231FA0_ovl_01[2] = 3.4f;
      D_80231FA0_ovl_01[3] = 3.5f;
      break;
    default:
      D_80231FA0_ovl_01[0] = 4.0f;
      D_80231FA0_ovl_01[1] = 4.0f;
      D_80231FA0_ovl_01[2] = 4.0f;
      D_80231FA0_ovl_01[3] = 4.0f;
      break;
  }

  guOrtho(&D_80231F60_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CHARACTERS_NEAR,
      MENU_CHARACTERS_FAR, MENU_CHARACTERS_ORTHO_SCALE);
  for (slot = 0; slot != characterCount; slot++) {
    if (D_8023BFC0_ovl_01[slot] > 0) {
      D_8023BFC0_ovl_01[slot]--;
    }

    if (D_8023BFC0_ovl_01[slot] == 0) {
      D_8023BFC0_ovl_01[slot] = -1;
      SetAnimationLooping(slot, FALSE);
      SetNextAnimation(slot, MENU_ANIMATION_FIDGET);
    }

    if (D_8023BFC0_ovl_01[slot] == -1) {
      if (GetAnimationEndFrame(slot, MENU_ANIMATION_FIDGET) ==
          GetAnimationFrame(slot)) {
        SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
        SetAnimationLooping(slot, TRUE);
        D_8023BFC0_ovl_01[slot] = guRandom() % FIDGET_DELAY_RANGE;
      }
    }

    if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
      character = rival;
      palette = 1;
    } else {
      character = (s8)g_Players[slot].characterIndex;
      palette = (s8)g_Players[slot].paletteIndex;
    }

    if (!D_801B70F0[slot]) {
      if (IsCharacterSlotBusy(slot)) {
        FreeCharacterSlot(slot);
      } else {
        LoadCharacter(slot, character + CHARACTER_COUNT, palette);
        D_801B70F0[slot] = TRUE;
        SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
        SetAnimationSpeed(slot, 1.0f);
        if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
          SetCharacterLighting(RIVAL_SLOT, CHARACTER_LIGHTING_BLACK);
        }
      }
    }

    layout = 0;
    if (rival < 0) {
      layout = characterCount - 1;
    }

    if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
      D_8022FA5C_ovl_01 -= RIVAL_SPIN_STEP;
      D_8022FA58_ovl_01 = D_8022FA5C_ovl_01;
    } else {
      D_8022FA58_ovl_01 = 0.0f;
    }

    rotation[0] = DEGREES_TO_RADIANS(D_8022FA54_ovl_01);
    rotation[1] = DEGREES_TO_RADIANS(D_8022FA58_ovl_01);
    rotation[2] = 0.0f;
    translation[0] = D_8022FA68_ovl_01[layout][slot][0];
    raise = (character == CHARACTER_BABY_MARIO) ? BABY_MARIO_RAISE : 0;
    translation[1] = D_8022FA68_ovl_01[layout][slot][1] + raise;
    translation[2] = D_8022FA68_ovl_01[layout][slot][2];
    scale[0] = D_80231FA0_ovl_01[slot];
    scale[1] = D_80231FA0_ovl_01[slot];
    scale[2] = D_80231FA0_ovl_01[slot];
    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(
        transform, &D_80231D60_ovl_01[slot][g_DrawBufferIndex]);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231F60_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&D_80231D60_ovl_01[slot][g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    g_DisplayListHead = gfx;
    DrawCharacter(slot);
    *displayListHead = g_DisplayListHead;
    gfx = *displayListHead;
    AnimateCharacter(slot);
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_80200EF0_ovl_01);
#endif

/*
 * A debug character viewer. START hides the board and turns on the controls:
 * the stick turns the character, the C buttons move it, A and B scale it, left
 * and right pick an animation, R plays it, Z with the C buttons steps through
 * it and L changes the character's kind.
 */
#define VIEWER_BOARD_KIND 11
#define VIEWER_NAME_X 32
#define VIEWER_NAME_Y 192
#define VIEWER_PLAYER_NUMBER_X 144
#define VIEWER_PLAYER_NAME_X 176
#define VIEWER_PLAYER_TOP 64
#define VIEWER_PLAYER_SPACING 18
#define VIEWER_ANIMATION_X 64
#define VIEWER_ANIMATION_Y 56
#define VIEWER_CLUB_X 144
#define VIEWER_POWER_Y 184
#define VIEWER_SPIN_Y 192
#define VIEWER_SIDE_SPIN_Y 200
#define VIEWER_STICK_DEAD_ZONE 20
#define VIEWER_STICK_DIVISOR 4
#define VIEWER_SCALE_STEP 0.05f
#define VIEWER_LAST_ANIMATION 34
/* From this animation on, the character's second model plays it */
#define VIEWER_FIRST_SECOND_MODEL_ANIMATION 12
#define VIEWER_SECOND_MODEL_OFFSET 18
#define VIEWER_KIND_COUNT 5
#define VIEWER_HALF_WIDTH 160.0f
#define VIEWER_HALF_HEIGHT 120.0f
#define VIEWER_NEAR 1.0f
#define VIEWER_FAR 3000.0f
#define VIEWER_PERSPECTIVE_SCALE 10.0f
#define VIEWER_BASE_SCALE 5.0f
#define VIEWER_BASE_OFFSET -64.0f
#define VIEWER_DEPTH -1000.0f

extern s32 D_8022BE24_ovl_01;
extern s8 D_8022BE81_ovl_01;
extern u8 D_8022BE94_ovl_01;
extern s32 D_8022FBA0_ovl_01;
extern s32 D_8022FBA4_ovl_01;
extern f32 D_8022FBA8_ovl_01;
extern f32 D_8022FBAC_ovl_01;
extern f32 D_8022FBB0_ovl_01;
extern u32 D_8022FBB4_ovl_01;
extern f32 D_8022FBB8_ovl_01;
extern f32 D_8022FBBC_ovl_01;
extern char D_802312E0_ovl_01[];
extern char D_802312E8_ovl_01[];
extern char D_802312F0_ovl_01[];
extern Mtx D_80231FB0_ovl_01[2];
extern Mtx D_80232030_ovl_01;

const char D_80231660_ovl_01[] = "%d:%s";
const char D_80231668_ovl_01[] = "%d:";
const char D_8023166C_ovl_01[] = "%d";
const char D_80231670_ovl_01[] = "POWER:%3d";
const char D_8023167C_ovl_01[] = "SPIN :%3d";

void func_80201574_ovl_01(Gfx** displayListHead) {
  Gfx* gfx;
  s32 index;
  s32 stickX;
  s32 stickY;
  s32 character;

  gfx = *displayListHead;

  if (g_ControllerStates[0].pressedButtons & START_BUTTON) {
    D_8022BE28_ovl_01 ^= 1;
  }

  if (D_8022BE28_ovl_01 == 0) {
    func_801FB54C_ovl_01(&gfx, VIEWER_BOARD_KIND);
  }

  SetUpTextDrawing(&gfx);
  gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0x7F, 0xFF, 0xFF);
  gDPSetEnvColor(gfx++, 0x00, 0x10, 0x40, 0xFF);
  sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.unk2 + 1,
      D_8022BF70_ovl_01[D_801323A0.unk2]);
  DrawLargeText(&gfx, VIEWER_NAME_X, VIEWER_NAME_Y, g_PrintBuffer);
  for (index = 0; index != g_ChosenPlayerCount; index++) {
    sprintf(g_PrintBuffer, D_80231668_ovl_01, index + 1);
    DrawTextStub(&gfx, VIEWER_PLAYER_NUMBER_X,
        VIEWER_PLAYER_TOP + (index * VIEWER_PLAYER_SPACING), g_PrintBuffer);
  }

  for (index = 0; index != D_801B7070; index++) {
    sprintf(
        g_PrintBuffer, D_80231514_ovl_01, D_8022BF70_ovl_01[D_8012F480[index]]);
    DrawTextStub(&gfx, VIEWER_PLAYER_NAME_X,
        VIEWER_PLAYER_TOP + (index * VIEWER_PLAYER_SPACING), g_PrintBuffer);
  }

  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  sprintf(g_PrintBuffer, D_8023166C_ovl_01, D_8022BE24_ovl_01);
  DrawShadowedText(&gfx, VIEWER_ANIMATION_X, VIEWER_ANIMATION_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, D_80231670_ovl_01,
      g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].distance);
  DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_POWER_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, D_8023167C_ovl_01,
      g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].spin);
  DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SPIN_Y, g_PrintBuffer);
  if (g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].sideSpin > 0) {
    DrawShadowedText(
        &gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, D_802312E8_ovl_01);
  } else if (g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].sideSpin < 0) {
    DrawShadowedText(
        &gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, D_802312E0_ovl_01);
  } else {
    DrawShadowedText(
        &gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, D_802312F0_ovl_01);
  }

  if (D_8022BE28_ovl_01 == 0) {
    ClearGameFlag(GAME_FLAG_SHOW_ANIMATION_FRAMES);
  } else {
    stickX = g_ControllerStates[0].stickX;
    stickY = g_ControllerStates[0].stickY;
    SetGameFlag(GAME_FLAG_SHOW_ANIMATION_FRAMES);
    if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
      if (stickX > 0) {
        stickX -= VIEWER_STICK_DEAD_ZONE;
        if (stickX < 0) {
          stickX = 0;
        }

        stickX /= VIEWER_STICK_DIVISOR;
      } else if (stickX < 0) {
        stickX += VIEWER_STICK_DEAD_ZONE;
        if (stickX > 0) {
          stickX = 0;
        }

        stickX /= VIEWER_STICK_DIVISOR;
      }

      if (stickY > 0) {
        stickY -= VIEWER_STICK_DEAD_ZONE;
        if (stickY < 0) {
          stickY = 0;
        }

        stickY /= VIEWER_STICK_DIVISOR;
      } else if (stickY < 0) {
        stickY += VIEWER_STICK_DEAD_ZONE;
        if (stickY > 0) {
          stickY = 0;
        }

        stickY /= VIEWER_STICK_DIVISOR;
      }

      D_8022FBA8_ovl_01 += stickY;
      D_8022FBAC_ovl_01 += stickX;
      if (g_ControllerStates[0].buttons & L_CBUTTONS) {
        D_8022FBB8_ovl_01 -= 1.0f;
      }

      if (g_ControllerStates[0].buttons & R_CBUTTONS) {
        D_8022FBB8_ovl_01 += 1.0f;
      }

      if (g_ControllerStates[0].buttons & U_CBUTTONS) {
        D_8022FBBC_ovl_01 += 1.0f;
      }

      if (g_ControllerStates[0].buttons & D_CBUTTONS) {
        D_8022FBBC_ovl_01 -= 1.0f;
      }

      if (g_ControllerStates[0].buttons & B_BUTTON) {
        D_8022FBB0_ovl_01 -= VIEWER_SCALE_STEP;
      }

      if (g_ControllerStates[0].buttons & A_BUTTON) {
        D_8022FBB0_ovl_01 += VIEWER_SCALE_STEP;
      }
    }

    if ((g_MenuPressedButtons & L_JPAD) && D_8022BE24_ovl_01 > 0) {
      D_8022BE24_ovl_01--;
      D_8022FBA4_ovl_01 = TRUE;
    }

    if ((g_MenuPressedButtons & R_JPAD) &&
        D_8022BE24_ovl_01 < VIEWER_LAST_ANIMATION) {
      D_8022BE24_ovl_01++;
      D_8022FBA4_ovl_01 = TRUE;
    }

    if (g_ControllerStates[0].buttons & Z_TRIG) {
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
      if (g_ControllerStates[0].repeatedButtons & L_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, -1.0f);
      }

      if (g_ControllerStates[0].repeatedButtons & R_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, 1.0f);
      }

      if (g_ControllerStates[0].buttons & U_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, 4.0f);
      }

      if (g_ControllerStates[0].buttons & D_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, -4.0f);
      }

      AnimateAllCharacters();
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
    }

    if (g_ControllerStates[0].buttons & R_TRIG) {
      if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
        SetCharacterAnimation(D_8022FBB4_ovl_01, D_8022BE24_ovl_01);
      }

      SetAnimationSpeed(D_8022FBB4_ovl_01, 1.0f);
    }

    if ((g_MenuRepeatButtons & L_TRIG) && D_8022BE94_ovl_01 == FALSE) {
      D_8022BE81_ovl_01++;
      if (D_8022BE81_ovl_01 >= VIEWER_KIND_COUNT) {
        D_8022BE81_ovl_01 = 0;
      }

      if (D_8022BE81_ovl_01 < 0) {
        D_8022BE81_ovl_01 = VIEWER_KIND_COUNT - 1;
      }

      D_8022BE94_ovl_01 = TRUE;
    }
  }

  if (D_8022FBA0_ovl_01 != D_801323A0.unk2 || D_8022FBA4_ovl_01 == TRUE ||
      D_8022BE94_ovl_01 == TRUE) {
    if (IsCharacterSlotBusy(D_8022FBB4_ovl_01)) {
      FreeCharacterSlot(D_8022FBB4_ovl_01);
    } else {
      character = D_801323A0.unk2;
      D_8022BE94_ovl_01 = FALSE;
      D_8022FBA4_ovl_01 = FALSE;
      D_8022FBA0_ovl_01 = character;
      if (D_8022BE24_ovl_01 >= VIEWER_FIRST_SECOND_MODEL_ANIMATION) {
        LoadCharacter(D_8022FBB4_ovl_01, character + VIEWER_SECOND_MODEL_OFFSET,
            D_8022BE81_ovl_01);
      } else {
        LoadCharacter(D_8022FBB4_ovl_01, character, D_8022BE81_ovl_01);
      }

      SetCharacterAnimation(D_8022FBB4_ovl_01, D_8022BE24_ovl_01);
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
    }
  }

  AnimateAllCharacters();
  guOrtho(&D_80232030_ovl_01, -VIEWER_HALF_WIDTH, VIEWER_HALF_WIDTH,
      -VIEWER_HALF_HEIGHT, VIEWER_HALF_HEIGHT, VIEWER_NEAR, VIEWER_FAR,
      VIEWER_PERSPECTIVE_SCALE);
  {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;

    rotation[2] = 0;
    rotation[0] = DEGREES_TO_RADIANS(D_8022FBA8_ovl_01);
    rotation[1] = DEGREES_TO_RADIANS(D_8022FBAC_ovl_01);
    translation[0] = D_8022FBB8_ovl_01 + VIEWER_BASE_OFFSET;
    translation[1] = D_8022FBBC_ovl_01 + VIEWER_BASE_OFFSET;
    translation[2] = VIEWER_DEPTH;
    scale[0] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    scale[1] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    scale[2] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    ConvertMatrixToFixed(matrix, &D_80231FB0_ovl_01[g_DrawBufferIndex]);
  }

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80232030_ovl_01),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231FB0_ovl_01[g_DrawBufferIndex]),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  g_DisplayListHead = gfx;
  DrawCharacter(D_8022FBB4_ovl_01);
  *displayListHead = g_DisplayListHead;
  gfx = *displayListHead;
}

/*
 * Draws screen 6, the course select, or the bet settings and screen 20's
 * columns as screens 20 and 22 slide between them. The course select draws
 * the water plane, then the wheel of the open courses and the next one, or
 * of the mini-golf disc's images, the front one bobbing. While the holes are
 * chosen (MAP_CAMERA_MODE_TOP_BYTE is 1) it draws the holes badges;
 * otherwise the course's birdie badge count and tournament trophies. Then
 * func_8021B618 slides the stats panel's rows the way D_8022BE89 picks; when
 * that is 3 or 4, the chosen hole starts loading once they stop.
 */
/*
 * Doesn't match: the original keeps the chosen course on the stack instead
 * of in a register, and orders the D_8022BE89 switch's branches and the
 * trophies' loads differently.
 */
#ifdef NON_MATCHING
void func_80202000_ovl_01(Gfx** displayListHead) {
  Gfx* gfx;
  u16 perspNorm;
  u16 wheelPerspNorm;
  u8 unusedStack[0x10];
  f32 iconX[COURSE_COUNT];
  f32 iconY[COURSE_COUNT];
  f32 iconScale[COURSE_COUNT];
  u8 unusedStack2[0x18];
  s32 drawOrder[COURSE_COUNT];
  s16 places[TOURNAMENT_PLACE_COUNT];
  SaveData* save;
  s32 courseCount;
  s32 choice;
  s32 spin;
  s32 texelsOffset;
  s32 angleStep;
  s32 index;
  /* Counts the sort's passes, the strips, the holes (then holds the badge
   * count's shift), the badges, the places and the trophies */
  s32 otherIndex;
  s32 key;
  s32 slot;
  s32 halfSize;
  s32 step;
  s32 stripTop;
  s32 stripOffset;
  f32 angle;
  f32 wheelX;
  f32 bob;
  void* palette;
  u8* texels;
  s16 badgeCount;
  s32 placeCount;
  s32 red;
  s32 green;
  s32 blue;
  s16 left;
  s16 top;

  save = GetSaveData();
  gfx = *displayListHead;
  if (g_MenuScreen == MENU_SCREEN_UNK6) {
    func_801FB54C_ovl_01(&gfx, COURSE_SELECT_BACK_BOARD_KIND);
    guTranslate(&D_802321F0_ovl_01, 0.0f, 0.0f, -WATER_PLANE_DISTANCE);
    guPerspective(&D_80232230_ovl_01, &perspNorm, WATER_FIELD_OF_VIEW,
        WHEEL_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE,
        WATER_PERSPECTIVE_SCALE);
    gSPPerspNormalize(gfx++, perspNorm);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gSPFogPosition(gfx++, WATER_FOG_START, WATER_FOG_END);
    gDPSetFogColor(gfx++, 255, 255, 255, 255);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0,
        1, SHADE, 0, COMBINED, 0, 0, 0, 0, COMBINED);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, WATER_BLEND_ALPHA);
    gDPLoadTextureBlock(gfx++,
        ((u32)D_801F4A20 + COURSE_SELECT_WATER_OFFSET + IMAGE_HEADER_SIZE) & ~7,
        G_IM_FMT_RGBA, G_IM_SIZ_16b, WATER_TEXTURE_SIZE, WATER_TEXTURE_SIZE, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
        WATER_TEXTURE_MASK, WATER_TEXTURE_MASK, WATER_TEXTURE_SHIFT,
        WATER_TEXTURE_SHIFT);
    gDPLoadMultiBlock(gfx++,
        ((u32)D_801F4A20 + COURSE_SELECT_LAYER_OFFSET + IMAGE_HEADER_SIZE) & ~7,
        WATER_LAYER_TMEM, WATER_LAYER_TILE, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        WATER_TEXTURE_SIZE, WATER_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, WATER_TEXTURE_MASK, WATER_TEXTURE_MASK,
        WATER_TEXTURE_SHIFT, WATER_TEXTURE_SHIFT);
    gDPPipeSync(gfx++);
    D_8022FD98_ovl_01++;
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, D_8022FD98_ovl_01, 0,
        D_8022FD98_ovl_01 + WATER_TILE_SPAN, WATER_TILE_SPAN);
    gDPSetTileSize(gfx++, WATER_LAYER_TILE, -D_8022FD98_ovl_01,
        D_8022FD98_ovl_01, -D_8022FD98_ovl_01 + WATER_TILE_SPAN,
        D_8022FD98_ovl_01 + WATER_TILE_SPAN);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80232230_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_802321F0_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, D_8022FD48_ovl_01, WATER_VERTEX_COUNT, 0);
    gDPPipeSync(gfx++);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gSP1Triangle(gfx++, 4, 2, 3, 0);
    gSP1Triangle(gfx++, 0, 2, 4, 0);
    gDPPipeSync(gfx++);
    guPerspectiveF(D_80232270_ovl_01, &wheelPerspNorm, WHEEL_FIELD_OF_VIEW,
        WHEEL_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE, 1.0f);
    guLookAtF(D_802322B0_ovl_01, 0.0f, COURSE_SELECT_EYE_HEIGHT,
        COURSE_SELECT_EYE_DISTANCE, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    SetCpuProjectionF(D_80232270_ovl_01, D_802322B0_ovl_01, g_Viewports);
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
    if (g_GameState.isMiniGolf) {
      choice = g_MenuChoices.unk13;
      courseCount = MINI_GOLF_DISC_IMAGE_COUNT;
      angleStep = DEGREES_PER_TURN / MINI_GOLF_DISC_IMAGE_COUNT;
      texelsOffset = 0;
      spin = g_MenuChoices.unk15;
    } else {
      courseCount = GetOpenCourseCount();
      courseCount =
          (courseCount < COURSE_COUNT) ? (courseCount + 1) : COURSE_COUNT;

      choice = D_801323A0.courseChoice;
      angleStep = DEGREES_PER_TURN / courseCount;
      spin = D_801323A0.courseSpin;
      texelsOffset = LARGE_PALETTED_TEXELS_OFFSET;
    }

    for (index = 0; index != courseCount; index++) {
      ScreenPoint point;

      angle =
          -DEGREES_TO_RADIANS((((choice - index) % courseCount) * angleStep) +
              (spin * DEGREES_PER_SPIN_STEP));
      wheelX = sinf(angle) * WHEEL_RADIUS_X;
      if (ProjectPoint(wheelX, 0.0f, cosf(angle) * WHEEL_RADIUS_Z, &point) <
          0) {
        iconX[index] = point.x;
        iconY[index] = point.y - WHEEL_RAISE;
        iconScale[index] = point.scaleX;
      }
    }

    for (otherIndex = 0; otherIndex != courseCount; otherIndex++) {
      drawOrder[otherIndex] = otherIndex;
    }

    for (otherIndex = 1; otherIndex != courseCount; otherIndex++) {
      key = drawOrder[otherIndex];
      for (index = otherIndex - 1;
          (index >= 0) && (iconY[drawOrder[index]] > iconY[key]); index--) {
        drawOrder[index + 1] = drawOrder[index];
      }

      drawOrder[index + 1] = key;
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    D_8022FBC0_ovl_01 += FRONT_COURSE_BOB_PHASE_STEP;
    for (index = 0; index != courseCount; index++) {
      slot = drawOrder[index];
      halfSize = iconScale[slot] * COURSE_ICON_HALF_SIZE;
      step = (f32)(COURSE_ICON_STRIP_HEIGHT << 10) / halfSize;
      if (index == (courseCount - 1)) {
        bob = sinf(D_8022FBC0_ovl_01) * FRONT_COURSE_BOB_HEIGHT;
      } else {
        bob = 0.0f;
      }

      if (g_GameState.isMiniGolf == TRUE) {
        palette = D_801052C4[slot];
        /* Indexed as one row, as that matches the original better */
        texels =
            D_801049A0[0]
                      [(D_8022F8DD_ovl_01 * MINI_GOLF_DISC_IMAGE_COUNT) + slot];
      } else {
        palette = (u8*)D_801049A0[0][slot] + IMAGE_HEADER_SIZE;
        texels = D_801049A0[0][slot];
      }

      gDPLoadTLUT_pal256(gfx++, (u32)palette & ~7);
      if (choice == slot) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(
            gfx++, G_RM_AA_TEX_EDGE | Z_UPD, G_RM_AA_TEX_EDGE2 | Z_UPD);
        gDPSetPrimDepth(gfx++, CHOSEN_COURSE_DEPTH, 0);
      } else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
      }

      if (!g_GameState.isMiniGolf) {
        if ((slot == (courseCount - 1)) &&
            (GetOpenCourseCount() < COURSE_COUNT)) {
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPSetPrimColor(gfx++, 0, 0, LOCKED_COURSE_SHADE, LOCKED_COURSE_SHADE,
              LOCKED_COURSE_SHADE, 255);
        } else {
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        }
      }

      stripTop = 0;
      stripOffset = texelsOffset;
      for (otherIndex = 0; otherIndex != COURSE_ICON_STRIP_COUNT;
          otherIndex++) {
        gDPLoadTextureBlock(gfx++, ((u32)texels + stripOffset) & ~7,
            G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_ICON_WIDTH,
            COURSE_ICON_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (s32)(iconX[slot] - halfSize) << 2,
            (s32)((iconY[slot] - halfSize) + stripTop + bob) << 2,
            (s32)(iconX[slot] + halfSize) << 2,
            (s32)(iconY[slot] + stripTop + bob) << 2, G_TX_RENDERTILE, 0, 0,
            step, step);
        gDPPipeSync(gfx++);
        stripTop += halfSize;
        stripOffset += COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT;
      }

      if ((g_GameState.isMiniGolf == TRUE) && (g_MenuChoices.unk13 == slot)) {
        if ((spin == 0) || (D_8022FD44_ovl_01 == 0.0f)) {
          D_8022FD44_ovl_01 = iconY[slot];
        }

        func_8021CBDC_ovl_01(&gfx, D_8022FD44_ovl_01 + bob);
      }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    if (MAP_CAMERA_MODE_TOP_BYTE == 0) {
      badgeCount = 0;
      for (otherIndex = 0; otherIndex != COURSE_HOLE_COUNT; otherIndex++) {
        if (save->unk990
                .birdieBadgeCounts[D_801323A0.courseChoice][otherIndex]) {
          badgeCount++;
        }
      }

      /* Centers the count by its number of digits */
      if (badgeCount < 10) {
        otherIndex = WON_COUNT_DIGIT_WIDTH;
      }

      if ((badgeCount >= 10) && (badgeCount < 100)) {
        otherIndex = 0;
      }

      if (badgeCount >= 100) {
        otherIndex = -WON_COUNT_DIGIT_WIDTH;
      }

      func_80207E28_ovl_01(&gfx, D_800FC898, otherIndex + COURSE_BADGE_ICON_X,
          COURSE_BADGE_ICON_Y);
    }

    if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_BILERP);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
      gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_80106100 + IMAGE_HEADER_SIZE) & ~7);
      for (otherIndex = 0; otherIndex != HOLES_BADGE_COUNT; otherIndex++) {
        if ((otherIndex == HOLES_BADGE_BET) && (D_800C2BD5 != 1)) {
          gDPPipeSync(gfx++);
          gDPLoadTextureTile_4b(gfx++,
              ((u32)D_80106100 + PALETTED_TEXELS_OFFSET +
                  (otherIndex * HOLES_BADGE_IMAGE_SIZE)) &
                  ~7,
              G_IM_FMT_CI, HOLES_BADGE_SIZE, HOLES_BADGE_SIZE, 0, 0,
              HOLES_BADGE_SIZE - 1, HOLES_BADGE_SIZE - 1, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          gSPScisTextureRectangle(gfx++, D_8022FC20_ovl_01[otherIndex].x << 2,
              D_8022FC20_ovl_01[otherIndex].y << 2,
              (D_8022FC20_ovl_01[otherIndex].x + HOLES_BADGE_SIZE) << 2,
              (D_8022FC20_ovl_01[otherIndex].y + HOLES_BADGE_SIZE) << 2,
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
          gDPPipeSync(gfx++);
          D_8022FD40_ovl_01 += BET_BADGE_COLOR_STEP;
          red = (s32)(sinf(D_8022FD40_ovl_01) * BET_BADGE_COLOR_SWING) +
              BET_BADGE_COLOR_MIDPOINT;
          blue = (s32)(sinf(D_8022FD40_ovl_01 + (M_PI / 2)) *
                     BET_BADGE_COLOR_SWING) +
              BET_BADGE_COLOR_MIDPOINT;
          green =
              (s32)(sinf(D_8022FD40_ovl_01 + M_PI) * BET_BADGE_COLOR_SWING) +
              BET_BADGE_COLOR_MIDPOINT;
          gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 128);
          gDPSetEnvColor(gfx++, red / BET_BADGE_ENV_DIVISOR,
              green / BET_BADGE_ENV_DIVISOR, blue / BET_BADGE_ENV_DIVISOR, 255);
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        } else if (g_MenuChoices.unk0 == otherIndex) {
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        } else {
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPSetPrimColor(gfx++, 0, 0, UNCHOSEN_HOLES_BADGE_SHADE,
              UNCHOSEN_HOLES_BADGE_SHADE, UNCHOSEN_HOLES_BADGE_SHADE, 255);
        }

        gDPPipeSync(gfx++);
        gDPLoadTextureTile_4b(gfx++,
            ((u32)D_80106100 + PALETTED_TEXELS_OFFSET +
                (otherIndex * HOLES_BADGE_IMAGE_SIZE)) &
                ~7,
            G_IM_FMT_CI, HOLES_BADGE_SIZE, HOLES_BADGE_SIZE, 0, 0,
            HOLES_BADGE_SIZE - 1, HOLES_BADGE_SIZE - 1, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, D_8022FC20_ovl_01[otherIndex].x << 2,
            D_8022FC20_ovl_01[otherIndex].y << 2,
            (D_8022FC20_ovl_01[otherIndex].x + HOLES_BADGE_SIZE) << 2,
            (D_8022FC20_ovl_01[otherIndex].y + HOLES_BADGE_SIZE) << 2,
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
      }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    if ((MAP_CAMERA_MODE_TOP_BYTE == 0) && !g_GameState.isMiniGolf) {
      placeCount = 0;
      for (otherIndex = 0; otherIndex != TOURNAMENT_PLACE_COUNT; otherIndex++) {
        if (D_80105C38[D_801323A0.courseChoice].hasPlace[otherIndex]) {
          places[placeCount] = otherIndex;
          placeCount++;
        }
      }

      if (D_801323A0.courseSpin == 0) {
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
        for (otherIndex = 0; otherIndex != placeCount; otherIndex++) {
          gDPLoadTLUT_pal256(gfx++, (u32)D_800FBE38[places[otherIndex]] & ~7);
          gDPLoadTextureTile(gfx++, (u32)D_800FBDFC[D_8022BE80_ovl_01] & ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b,
              D_800C20E0[D_801323A0.courseChoice].width,
              D_800C20E0[D_801323A0.courseChoice].height, 0, 0,
              D_800C20E0[D_801323A0.courseChoice].width - 1,
              D_800C20E0[D_801323A0.courseChoice].height - 1, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          left = D_8022FC48_ovl_01[D_801323A0.courseChoice][placeCount - 1]
                                  [otherIndex]
                                      .x;
          top = D_8022FC48_ovl_01[D_801323A0.courseChoice][placeCount - 1]
                                 [otherIndex]
                                     .y;
          gSPScisTextureRectangle(gfx++, left << 2, top << 2,
              (left + D_800C20E0[D_801323A0.courseChoice].width) << 2,
              (top + D_800C20E0[D_801323A0.courseChoice].height) << 2,
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
          gDPPipeSync(gfx++);
        }
      }

      badgeCount = 0;
      for (otherIndex = 0; otherIndex != COURSE_HOLE_COUNT; otherIndex++) {
        if (save->unk990
                .birdieBadgeCounts[D_801323A0.courseChoice][otherIndex]) {
          badgeCount++;
        }
      }

      /* Centers the count by its number of digits */
      if (badgeCount < 10) {
        otherIndex = WON_COUNT_DIGIT_WIDTH;
      }

      if ((badgeCount >= 10) && (badgeCount < 100)) {
        otherIndex = 0;
      }

      if (badgeCount >= 100) {
        otherIndex = -WON_COUNT_DIGIT_WIDTH;
      }

      gDPPipeSync(gfx++);
      func_80210DF4_ovl_01(&gfx, otherIndex + COURSE_BADGE_COUNT_X,
          COURSE_BADGE_COUNT_Y, badgeCount, MENU_NUMBER_TIMES);
      gDPPipeSync(gfx++);
    }

    func_8021BE88_ovl_01(&gfx);
    func_802047F0_ovl_01(&gfx);
    if (!g_GameState.isMiniGolf &&
        (D_801323A0.courseChoice == (courseCount - 1)) &&
        (GetOpenCourseCount() < COURSE_COUNT)) {
      gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++, LOCKED_PANEL_LEFT << 2, LOCKED_PANEL_TOP << 2,
          LOCKED_PANEL_RIGHT << 2, LOCKED_PANEL_BOTTOM << 2, G_TX_RENDERTILE, 0,
          0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
      SetUpGradientTextDrawing(&gfx, TRUE);
      gDPSetEnvColor(gfx++, 191, 191, 255, 255);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
      SetTextSlotValue(
          GetCoursePointsNeeded(D_801323A0.courseChoice) - save->coursePoints,
          0, 0);
      SetTextSlot(LOCKED_COURSE_TEXT_X, LOCKED_COURSE_TEXT_Y,
          MESSAGE_POINTS_TO_UNLOCK, 1, 0, TEXT_FLAG_UNK20, 255, 255, 255, 255);
      DrawLargeTextSlot(&gfx, 0);
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
    }

    switch (D_8022BE89_ovl_01) {
      case 0:
        break;

      default:
        if (func_8021B618_ovl_01(&gfx, STATS_SLIDE_OUT_RIGHT) == 0) {
          D_8022BE88_ovl_01 = BET_PHASE_CHOOSING;
        }

        break;

      case MENU_UNK89_UNK3:
        if (func_8021B618_ovl_01(&gfx, STATS_SLIDE_IN_FROM_RIGHT) == 0) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE89_ovl_01 = 0;
          StartLoadingChosenHole();
        }

        break;

      case MENU_UNK89_UNK4:
        if (func_8021B618_ovl_01(&gfx, STATS_SLIDE_IN_FROM_LEFT) == 0) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE89_ovl_01 = 0;
          StartLoadingChosenHole();
        }

        break;
    }

    func_801FB54C_ovl_01(&gfx, COURSE_SELECT_FRONT_BOARD_KIND);
  }

  if ((g_MenuScreen == MENU_SCREEN_UNK22) ||
      (g_MenuScreen == MENU_SCREEN_UNK20)) {
    switch (D_8022BE88_ovl_01) {
      case BET_PHASE_OPENING:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_8023BFD0_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = BET_PHASE_CHOOSING;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
        }

        break;

      case BET_PHASE_CANCELLED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 += BET_SLIDE_STEP;
        D_8023BFD0_ovl_01 -= BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
          SetTextSlot(
              0, 0, NO_MESSAGE, 1, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
        }

        break;

      case BET_PHASE_CONFIRMED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 -= BET_SLIDE_STEP;
        D_802322F4_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = MENU_PHASE_CHOOSING;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
          D_802322F4_ovl_01 = 0;
        }

        break;

      case MENU_PHASE_CANCELLED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 += BET_SLIDE_STEP;
        D_802322F4_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = BET_PHASE_CHOOSING;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
          D_802322F4_ovl_01 = 0;
        }

        break;

      case MENU_PHASE_CONFIRMED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F4_ovl_01 -= BET_SLIDE_STEP;
        D_8023BFD0_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE84_ovl_01 = 0;
          D_802322F4_ovl_01 = 0;
          D_8023BFD0_ovl_01 = 0;
        }

        break;
    }

    func_8021B618_ovl_01(&gfx, STATS_SLIDE_OUT_RIGHT);
    if ((D_8022BE88_ovl_01 >= BET_PHASE_CHOOSING) &&
        ((D_8022BE88_ovl_01 != BET_PHASE_CANCELLED) &&
            (D_8022BE88_ovl_01 != MENU_PHASE_CONFIRMED))) {
      func_802047F0_ovl_01(&gfx);
    }

    func_801FB54C_ovl_01(&gfx, COURSE_SELECT_FRONT_BOARD_KIND);
    switch (D_8022BE88_ovl_01) {
      case BET_PHASE_OPENING:
        func_8021826C_ovl_01(
            &gfx, BET_SLIDE_DISTANCE - D_802322F0_ovl_01, 0, 0);
        break;

      case BET_PHASE_CHOOSING:
        func_8021826C_ovl_01(&gfx, 0, 0, 1);
        break;

      case BET_PHASE_CANCELLED:
        func_8021826C_ovl_01(&gfx, D_802322F0_ovl_01, 0, 0);
        break;

      case BET_PHASE_CONFIRMED:
        func_8021826C_ovl_01(&gfx, D_802322F0_ovl_01, 1, 0);
        func_80219BD4_ovl_01(
            &gfx, BET_SLIDE_DISTANCE - D_802322F4_ovl_01, 0, 0);
        break;

      case MENU_PHASE_CHOOSING:
        func_80219BD4_ovl_01(&gfx, 0, 0, 1);
        break;

      case MENU_PHASE_CANCELLED:
        func_8021826C_ovl_01(
            &gfx, D_802322F0_ovl_01 - BET_SLIDE_DISTANCE, 1, 0);
        func_80219BD4_ovl_01(&gfx, D_802322F4_ovl_01, 0, 0);
        break;

      case MENU_PHASE_CONFIRMED:
        func_80219BD4_ovl_01(&gfx, D_802322F4_ovl_01, 1, 0);
        break;
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_80202000_ovl_01);
#endif

/*
 * func_802047F0 draws the course select's banners of D_801B60D0: one behind
 * the course's name, and while the holes can be chosen, a wider one below
 * it behind the choice made, which squeezes the first.
 */
#define COURSE_NAME_BANNER_LEFT 67
#define COURSE_NAME_BANNER_TOP 168
#define COURSE_NAME_BANNER_WIDTH 190
#define COURSE_NAME_BANNER_SQUEEZE 48
#define HOLES_BANNER_LEFT 47
#define HOLES_BANNER_TOP 190
#define HOLES_BANNER_WIDTH 230
#define HOLES_BANNER_STRIP_HEIGHT 11
#define COURSE_NAME_Y 176
#define HOLES_CHOICE_Y 202
/* A HOLES_CHOICE_* value plus this is the message that explains it */
#define HOLES_CHOICE_MESSAGE_BASE 0xC3
#define HOLES_CHOICE_TEXT_SLOT 1

extern char* D_8022FDEC_ovl_01[MINI_GOLF_RECORD_COUNT];
extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];

/*
 * Doesn't match: the original sets the text drawing's argument before the
 * mini-golf test that picks the name's colours, and keeps the course name
 * banner's strip top in one register instead of copying it.
 */
#ifdef NON_MATCHING
void func_802047F0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 slide = D_8023BFD0_ovl_01;

  /* Also the course's index into the name colours, to match the original's
   * registers */
  s32 stripIndex;
  s32 stripTop;
  s32 squeeze;
  s32 cameraModeTopByte;

  if (D_8022BE88_ovl_01 == MENU_PHASE_UNK7) {
    slide = -slide;
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
  if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
              stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      stripTop = HOLES_BANNER_TOP + (stripIndex * HOLES_BANNER_STRIP_HEIGHT);
      gSPScisTextureRectangle(gfx++, HOLES_BANNER_LEFT << 2,
          (slide + stripTop) << 2,
          (HOLES_BANNER_LEFT + HOLES_BANNER_WIDTH) << 2,
          (slide + stripTop + HOLES_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE,
          0, 0, (MENU_BANNER_WIDTH << 10) / HOLES_BANNER_WIDTH,
          (MENU_BANNER_STRIP_HEIGHT << 10) / HOLES_BANNER_STRIP_HEIGHT);
      gDPPipeSync(gfx++);
    }
  }

  squeeze = MAP_CAMERA_MODE_TOP_BYTE != 0 ? COURSE_NAME_BANNER_SQUEEZE : 0;
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
            stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = COURSE_NAME_BANNER_TOP + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++,
        (COURSE_NAME_BANNER_LEFT + (squeeze / 2)) << 2, (slide + stripTop) << 2,
        (COURSE_NAME_BANNER_LEFT + COURSE_NAME_BANNER_WIDTH - (squeeze / 2))
            << 2,
        (slide + stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0,
        0, (MENU_BANNER_WIDTH << 10) / (COURSE_NAME_BANNER_WIDTH - squeeze),
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  if (!g_GameState.isMiniGolf) {
    if (D_800FF4C0 == 1) {
      sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
          g_CourseRecords[D_801323A0.courseChoice].name);
    } else {
      sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
          g_TournamentNames[D_801323A0.courseChoice]);
    }
  } else {
    sprintf(g_PrintBuffer, D_80231660_ovl_01, g_MenuChoices.unk13 + 1,
        D_8022FDEC_ovl_01[g_MenuChoices.unk13]);
  }

  SetUpGradientTextDrawing(&gfx, 0);
  if (g_GameState.isMiniGolf == TRUE) {
    stripIndex = g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX;
  } else {
    stripIndex = D_801323A0.courseChoice;
  }

  DrawLargeTextWithShadow(&gfx, SCREEN_WIDTH / 2, slide + COURSE_NAME_Y,
      g_PrintBuffer, g_CourseNameEnvColors[stripIndex],
      g_CourseNamePrimColors[stripIndex], TEXT_FLAG_CENTERED);
  cameraModeTopByte = MAP_CAMERA_MODE_TOP_BYTE;
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  if (cameraModeTopByte == 1) {
    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    SetTextSlot(SCREEN_WIDTH / 2, slide + HOLES_CHOICE_Y,
        g_MenuChoices.unk0 + HOLES_CHOICE_MESSAGE_BASE, 1,
        HOLES_CHOICE_TEXT_SLOT, TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 255, 255,
        255, 255);
    DrawLargeTextSlot(&gfx, HOLES_CHOICE_TEXT_SLOT);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_802047F0_ovl_01);
#endif

void DrawContinueLabel(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  DrawShadowedText(&gfx, 96, 64, g_ContinueLabelText);
  *displayListHead = gfx;
}

extern s32 D_8022FDF4_ovl_01[MAIN_CHOICE_COUNT][2];

void func_80205038_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;

  func_801FB54C_ovl_01(&gfx, 61);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_COUNT; choiceIndex++) {
    if (g_MenuChoices.mainChoice != choiceIndex) {
      gDPLoadTextureBlock_4b(gfx++,
          ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
              (choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE)) &
              ~7,
          G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, D_8022FDF4_ovl_01[choiceIndex][0] << 2,
          D_8022FDF4_ovl_01[choiceIndex][1] << 2,
          (D_8022FDF4_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH) << 2,
          (D_8022FDF4_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  choiceIndex = g_MenuChoices.mainChoice;
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
          (choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE)) &
          ~7,
      G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      (D_8022FDF4_ovl_01[choiceIndex][0] - (D_800FE33C / 2)) << 2,
      (D_8022FDF4_ovl_01[choiceIndex][1] - (D_800FE33C / 2)) << 2,
      (D_8022FDF4_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH +
          (D_800FE33C / 2))
          << 2,
      (D_8022FDF4_ovl_01[choiceIndex][1] +
          (MAIN_CHOICE_LABEL_HEIGHT + (D_800FE33C / 2)))
          << 2,
      G_TX_RENDERTILE, 0, 0,
      (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
      (MAIN_CHOICE_LABEL_HEIGHT << 10) /
          (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  *displayListHead = gfx;
}

/* The menu without Transmit skips the CODE ENTRY label */
extern s32 D_8022FE14_ovl_01[MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT][2];

void func_802058B8_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;
  s32 labelOffset;

  func_801FB54C_ovl_01(&gfx, 61);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT;
      choiceIndex++) {
    if (g_MenuChoices.mainChoiceWithoutTransmit != choiceIndex) {
      labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
      if (choiceIndex < MAIN_CHOICE_OPTIONS) {
        labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
      }

      gDPLoadTextureBlock_4b(gfx++,
          ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
              labelOffset) &
              ~7,
          G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, D_8022FE14_ovl_01[choiceIndex][0] << 2,
          D_8022FE14_ovl_01[choiceIndex][1] << 2,
          (D_8022FE14_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH) << 2,
          (D_8022FE14_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  choiceIndex = g_MenuChoices.mainChoiceWithoutTransmit;
  if (choiceIndex == MAIN_CHOICE_OPTIONS) {
    labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
  } else {
    labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
  }

  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
          labelOffset) &
          ~7,
      G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      (D_8022FE14_ovl_01[choiceIndex][0] - (D_800FE33C / 2)) << 2,
      (D_8022FE14_ovl_01[choiceIndex][1] - (D_800FE33C / 2)) << 2,
      (D_8022FE14_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH +
          (D_800FE33C / 2))
          << 2,
      (D_8022FE14_ovl_01[choiceIndex][1] +
          (MAIN_CHOICE_LABEL_HEIGHT + (D_800FE33C / 2)))
          << 2,
      G_TX_RENDERTILE, 0, 0,
      (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
      (MAIN_CHOICE_LABEL_HEIGHT << 10) /
          (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  *displayListHead = gfx;
}

/* The clubhouse's room labels, 192 by 20 each, plain in D_801B5CD0 and lit in
 * D_8012D420 */
#define ROOM_LABEL_WIDTH 192
#define ROOM_LABEL_HEIGHT 20
#define ROOM_LABEL_IMAGE_SIZE (ROOM_LABEL_WIDTH * ROOM_LABEL_HEIGHT / 2)

/* Places for the clubhouse's rows, three more than it shows */
#define CLUBHOUSE_ROW_PLACE_COUNT 8

extern s32 D_8022FE2C_ovl_01[CLUBHOUSE_ROW_PLACE_COUNT][2];

void func_80206180_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;
  s32 labelOffset;

  func_801FB54C_ovl_01(&gfx, 61);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (choiceIndex = 0; choiceIndex != CLUBHOUSE_CHOICE_COUNT; choiceIndex++) {
    if (choiceIndex != g_MenuChoices.clubhouseChoice) {
      labelOffset = D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE;
      gDPLoadTextureBlock_4b(gfx++,
          ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
              labelOffset) &
              ~7,
          G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, D_8022FE2C_ovl_01[choiceIndex][0] << 2,
          D_8022FE2C_ovl_01[choiceIndex][1] << 2,
          (D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH) << 2,
          (D_8022FE2C_ovl_01[choiceIndex][1] + ROOM_LABEL_HEIGHT) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  choiceIndex = g_MenuChoices.clubhouseChoice;
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
          (D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE)) &
          ~7,
      G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      (D_8022FE2C_ovl_01[choiceIndex][0] - (D_800FE33C / 2)) << 2,
      (D_8022FE2C_ovl_01[choiceIndex][1] - (D_800FE33C / 2)) << 2,
      (D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH + (D_800FE33C / 2))
          << 2,
      (D_8022FE2C_ovl_01[choiceIndex][1] +
          (ROOM_LABEL_HEIGHT + (D_800FE33C / 2)))
          << 2,
      G_TX_RENDERTILE, 0, 0,
      (ROOM_LABEL_WIDTH << 10) / (D_800FE33C + ROOM_LABEL_WIDTH),
      (ROOM_LABEL_HEIGHT << 10) / (D_800FE33C + ROOM_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  *displayListHead = gfx;
}

void func_80206A2C_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  func_801FB54C_ovl_01(&gfx, 61);
  *displayListHead = gfx;
}

const char D_802316EC_ovl_01[] = "Peach's Castle";
const char D_802316FC_ovl_01[] = "Luigi's Garden";

#define OPTIONS_BOARD_KIND 0xB9

/* The options screen's Mario and his shadow, in the menu image buffer */
#define MARIO_WIDTH 100
#define MARIO_HEIGHT 128
#define MARIO_STRIP_HEIGHT 16
#define MARIO_STRIP_COUNT 8
#define MARIO_LEFT 208
#define MARIO_TOP 88
#define MARIO_SHADOW_WIDTH 104
#define MARIO_SHADOW_HEIGHT 64
#define MARIO_SHADOW_LEFT 214
#define MARIO_SHADOW_TOP 168
#define MARIO_SHADOW_ALPHA 159

/* Each option's row is this far below the last */
#define OPTION_ROW_SPACING 30

/*
 * The toggled options' values sit on banners of D_801B60D0, squeezed to 120
 * columns and 8 rows a strip. Their strips are placed from row 62 and drawn 20
 * rows lower. The chosen one's moves 2 columns left above a shadow 3 rows
 * lower.
 */
#define VALUE_BANNER_LEFT 136
#define VALUE_BANNER_WIDTH 120
#define VALUE_BANNER_ROW_TOP 62
#define VALUE_BANNER_OFFSET_Y 20
#define VALUE_BANNER_STRIP_HEIGHT 8
#define CHOSEN_BANNER_SHIFT 2
#define BANNER_SHADOW_DROP 3
#define BANNER_SHADOW_COLOR 31
#define BANNER_SHADOW_ALPHA 159
#define DIMMED_COLOR 191

/*
 * The options' labels, plain in D_801B5CD0 and lit in D_8012D420. Each label
 * is followed in its image by the option's two values, so the labels lie 60
 * rows apart; the lit values come from their own image
 */
#define OPTION_LABEL_WIDTH 128
#define OPTION_LABEL_HEIGHT 20
#define OPTION_LABEL_IMAGE_SIZE (OPTION_LABEL_WIDTH * OPTION_LABEL_HEIGHT / 2)
#define OPTION_LABEL_SPACING (OPTION_LABEL_IMAGE_SIZE * 3)
#define OPTION_LABEL_LEFT 32
#define OPTION_LABEL_TOP 84
#define OPTION_VALUE_LEFT 142
#define OPTION_TOGGLE_COUNT OPTIONS_CHOICE_ERASE_SAVE_DATA

/*
 * The arrows either side of the chosen toggle switch frames every 5 updates.
 * The left arrow is the fourth of a frame, the right one the third
 */
#define ARROW_SIZE 16
#define ARROW_FRAME_HEIGHT (ARROW_SIZE * 4)
#define ARROW_FRAME_SIZE (ARROW_SIZE * ARROW_FRAME_HEIGHT / 2)
#define ARROW_TIMER_PERIOD 10
#define ARROW_FRAME_DURATION 5
#define LEFT_ARROW_ROW (ARROW_SIZE * 3)
#define RIGHT_ARROW_ROW (ARROW_SIZE * 2)
#define LEFT_ARROW_LEFT 125
#define RIGHT_ARROW_LEFT 247
#define ARROW_TOP 86
#define ARROW_SHADOW_OFFSET_X 1
#define ARROW_SHADOW_OFFSET_Y 3
#define ARROW_PASS_COUNT 2

/* The sound test's lines, a song column and a sound column */
#define SONG_TEST_X 32
#define SOUND_TEST_X 168
#define TEST_NUMBER_Y 152
#define TEST_NAME_Y 162
#define SONG_NAME_X 94
#define SOUND_NAME_X 230
#define TEST_REVERB_Y 176
#define TEST_PITCH_Y 184
#define TEST_TEMPO_Y 192
/* The chosen line is yellow */
#define CHOSEN_TEXT_BLUE 0

/* The erase window, and its question in text slot 10 */
#define ERASE_WINDOW_WIDTH 240
#define ERASE_WINDOW_HEIGHT 160
#define ERASE_WINDOW_CENTER_X 160
#define ERASE_WINDOW_CENTER_Y 120
#define NO_OK_TEXT -1
#define ERASE_MESSAGE 0xC9
#define ERASE_MESSAGE_X 160
#define ERASE_MESSAGE_Y 106
#define ERASE_MESSAGE_SLOT 10
#define ERASE_MESSAGE_RED 95
#define ERASE_MESSAGE_GREEN 63
#define ERASE_MESSAGE_BLUE 255
#define ERASE_MESSAGE_PALETTE_RED 255
#define ERASE_MESSAGE_PALETTE_GREEN 127
#define ERASE_MESSAGE_PALETTE_BLUE 127

extern u8 g_OptionBits;
extern s32 D_8022FEFC_ovl_01;

void func_80226A58_ovl_01(Gfx** displayListHead, s32 width, s32 height,
    s32 centerX, s32 centerY, s32 okTextShown, s32 chosenButton);

/*
 * Draws the options screen: Mario and his shadow, each option's label, the
 * toggles' values on their banners with arrows beside the chosen one, the
 * sound test's lines when game flag 0x55 is set, and the erase window while it
 * is up
 */
/*
 * Doesn't match: the original adds each value banner strip's row in both
 * branches without masking it to a byte
 */
#ifdef NON_MATCHING
void func_80206A68_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = func_8005AF74();
  u8* mario = GetMenuImageBuffer();
  u8* arrows = (u8*)GetMenuImageBuffer() + OPTIONS_ARROWS_OFFSET;
  u8* shadow = GetMenuImageBuffer();
  /* Counts the options' rows, then the arrows' passes */
  s32 loopIndex;
  s32 stripIndex;
  u8 stripTop;
  s32 valueOffset;
  s32 shiftX;
  s32 shiftY;
  s32 rowTop;
  s32 frameIndex;

  func_801FB54C_ovl_01(&gfx, OPTIONS_BOARD_KIND);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, MARIO_SHADOW_ALPHA);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
      PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  gDPLoadTextureTile_4b(gfx++,
      ((u32)shadow + OPTIONS_SHADOW_OFFSET + IMAGE_HEADER_SIZE) & ~7,
      G_IM_FMT_I, MARIO_SHADOW_WIDTH, MARIO_SHADOW_HEIGHT, 0, 0,
      MARIO_SHADOW_WIDTH - 1, MARIO_SHADOW_HEIGHT - 1, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  DrawTextureRect(&gfx, MARIO_SHADOW_LEFT << 2, MARIO_SHADOW_TOP << 2,
      (MARIO_SHADOW_LEFT + MARIO_SHADOW_WIDTH) << 2,
      (MARIO_SHADOW_TOP + MARIO_SHADOW_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  LoadLargePalette(&gfx, mario + IMAGE_HEADER_SIZE);
  for (stripIndex = 0; stripIndex != MARIO_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureTile(gfx++, ((u32)mario + LARGE_PALETTED_TEXELS_OFFSET) & ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MARIO_WIDTH, MARIO_HEIGHT, 0,
        stripIndex * MARIO_STRIP_HEIGHT, MARIO_WIDTH - 1,
        (stripIndex * MARIO_STRIP_HEIGHT) + (MARIO_STRIP_HEIGHT - 1), 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(&gfx, MARIO_LEFT << 2,
        (MARIO_TOP + (stripIndex * MARIO_STRIP_HEIGHT)) << 2,
        (MARIO_LEFT + MARIO_WIDTH) << 2,
        (MARIO_TOP + MARIO_STRIP_HEIGHT + (stripIndex * MARIO_STRIP_HEIGHT))
            << 2,
        G_TX_RENDERTILE, 0, (stripIndex * MARIO_STRIP_HEIGHT) << 5,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
  gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, DIMMED_COLOR);
  for (loopIndex = 0; loopIndex != OPTION_TOGGLE_COUNT; loopIndex++) {
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
      LoadTextureBlock(&gfx,
          (u8*)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
              (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, 0, 0, 0, 0, 0, 0);
      if (g_MenuChoices.optionsChoice == loopIndex) {
        stripTop = (loopIndex * OPTION_ROW_SPACING) +
            ((stripIndex * VALUE_BANNER_STRIP_HEIGHT) + VALUE_BANNER_ROW_TOP);
        gDPSetPrimColor(gfx++, 0, 0, BANNER_SHADOW_COLOR, BANNER_SHADOW_COLOR,
            BANNER_SHADOW_COLOR, BANNER_SHADOW_ALPHA);
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0,
            0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
        DrawTextureRect(&gfx, VALUE_BANNER_LEFT << 2,
            (stripTop + VALUE_BANNER_OFFSET_Y + BANNER_SHADOW_DROP) << 2,
            (VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH) << 2,
            (stripTop + VALUE_BANNER_OFFSET_Y + BANNER_SHADOW_DROP +
                VALUE_BANNER_STRIP_HEIGHT)
                << 2,
            G_TX_RENDERTILE, 0, 0,
            (MENU_BANNER_WIDTH << 10) / VALUE_BANNER_WIDTH,
            (MENU_BANNER_STRIP_HEIGHT << 10) / VALUE_BANNER_STRIP_HEIGHT);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        DrawTextureRect(&gfx, (VALUE_BANNER_LEFT - CHOSEN_BANNER_SHIFT) << 2,
            (stripTop + VALUE_BANNER_OFFSET_Y) << 2,
            (VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH - CHOSEN_BANNER_SHIFT) << 2,
            (stripTop + VALUE_BANNER_OFFSET_Y + VALUE_BANNER_STRIP_HEIGHT) << 2,
            G_TX_RENDERTILE, 0, 0,
            (MENU_BANNER_WIDTH << 10) / VALUE_BANNER_WIDTH,
            (MENU_BANNER_STRIP_HEIGHT << 10) / VALUE_BANNER_STRIP_HEIGHT);
      } else {
        stripTop = (loopIndex * OPTION_ROW_SPACING) +
            ((stripIndex * VALUE_BANNER_STRIP_HEIGHT) + VALUE_BANNER_ROW_TOP);
        gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0,
            PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0);
        gDPSetPrimColor(
            gfx++, 0, 0, DIMMED_COLOR, DIMMED_COLOR, DIMMED_COLOR, 255);
        DrawTextureRect(&gfx, VALUE_BANNER_LEFT << 2,
            (stripTop + VALUE_BANNER_OFFSET_Y) << 2,
            (VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH) << 2,
            (stripTop + VALUE_BANNER_OFFSET_Y + VALUE_BANNER_STRIP_HEIGHT) << 2,
            G_TX_RENDERTILE, 0, 0,
            (MENU_BANNER_WIDTH << 10) / VALUE_BANNER_WIDTH,
            (MENU_BANNER_STRIP_HEIGHT << 10) / VALUE_BANNER_STRIP_HEIGHT);
      }
    }
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (loopIndex = 0; loopIndex != OPTIONS_CHOICE_COUNT; loopIndex++) {
    if (g_MenuChoices.optionsChoice != loopIndex) {
      LoadNibbleTextureBlock(&gfx,
          (u8*)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
              (loopIndex * OPTION_LABEL_SPACING),
          G_IM_FMT_CI, OPTION_LABEL_WIDTH, OPTION_LABEL_HEIGHT, 0, 0, 0, 0, 0,
          0, 0);
      DrawTextureRect(&gfx, OPTION_LABEL_LEFT << 2,
          (OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)) << 2,
          (OPTION_LABEL_LEFT + OPTION_LABEL_WIDTH) << 2,
          (OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT +
              (loopIndex * OPTION_ROW_SPACING))
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  LoadSmallPalette(
      &gfx, 0, (u8*)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE);
  for (loopIndex = 0; loopIndex != OPTIONS_CHOICE_COUNT; loopIndex++) {
    if (g_MenuChoices.optionsChoice == loopIndex) {
      LoadNibbleTextureBlock(&gfx,
          (u8*)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
              (loopIndex * OPTION_LABEL_SPACING),
          G_IM_FMT_CI, OPTION_LABEL_WIDTH, OPTION_LABEL_HEIGHT, 0, 0, 0, 0, 0,
          0, 0);
      gDPPipeSync(gfx++);
      DrawTextureRect(&gfx, OPTION_LABEL_LEFT << 2,
          (OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)) << 2,
          (OPTION_LABEL_LEFT + OPTION_LABEL_WIDTH) << 2,
          (OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT +
              (loopIndex * OPTION_ROW_SPACING))
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0, PRIMITIVE,
      0, TEXEL0, 0, 0, 0, 0, TEXEL0);
  gDPLoadTLUT_pal16(gfx++, 0,
      ((u32)D_8012D420[D_8022BE8E_ovl_01] + OPTION_VALUES_OFFSET +
          IMAGE_HEADER_SIZE) &
          ~7);
  for (loopIndex = 0; loopIndex != OPTION_TOGGLE_COUNT; loopIndex++) {
    /* Each toggle has two values, the second shown while its bit is set */
    valueOffset = loopIndex * 2 * OPTION_LABEL_IMAGE_SIZE;
    switch (loopIndex) {
      case OPTIONS_CHOICE_MUSIC:
        if (g_OptionBits & OPTION_BIT_MUSIC_OFF) {
          valueOffset += OPTION_LABEL_IMAGE_SIZE;
        }

        break;
      case OPTIONS_CHOICE_MONO:
        if (g_OptionBits & OPTION_BIT_MONO) {
          valueOffset += OPTION_LABEL_IMAGE_SIZE;
        }

        break;
      case OPTIONS_CHOICE_CAMERA:
        if (g_OptionBits & OPTION_BIT_SIMPLE_CAMERA) {
          valueOffset += OPTION_LABEL_IMAGE_SIZE;
        }

        break;
    }

    LoadNibbleTextureBlock(&gfx,
        (u8*)D_8012D420[D_8022BE8E_ovl_01] + OPTION_VALUES_OFFSET +
            PALETTED_TEXELS_OFFSET + valueOffset,
        G_IM_FMT_CI, OPTION_LABEL_WIDTH, OPTION_LABEL_HEIGHT, 0, 0, 0, 0, 0, 0,
        0);
    if (g_MenuChoices.optionsChoice == loopIndex) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(
          gfx++, 0, 0, DIMMED_COLOR, DIMMED_COLOR, DIMMED_COLOR, 255);
    }

    DrawTextureRect(&gfx, OPTION_VALUE_LEFT << 2,
        (OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)) << 2,
        (OPTION_VALUE_LEFT + OPTION_LABEL_WIDTH) << 2,
        (OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT +
            (loopIndex * OPTION_ROW_SPACING))
            << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPLoadTLUT_pal16(gfx++, 0, ((u32)arrows + IMAGE_HEADER_SIZE) & ~7);
  if (g_MenuChoices.optionsChoice != OPTIONS_CHOICE_ERASE_SAVE_DATA) {
    D_8022FEFC_ovl_01 = (D_8022FEFC_ovl_01 + 1) % ARROW_TIMER_PERIOD;
    frameIndex = D_8022FEFC_ovl_01 < ARROW_FRAME_DURATION;
    LoadNibbleTextureBlock(&gfx,
        arrows + PALETTED_TEXELS_OFFSET + (frameIndex * ARROW_FRAME_SIZE),
        G_IM_FMT_CI, ARROW_SIZE, ARROW_FRAME_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    /* The first pass draws the arrows' shadows */
    for (loopIndex = 0; loopIndex != ARROW_PASS_COUNT; loopIndex++) {
      shiftX = 0;
      if (loopIndex == 0) {
        shiftX = ARROW_SHADOW_OFFSET_X;
        shiftY = ARROW_SHADOW_OFFSET_Y;
        gDPSetPrimColor(gfx++, 0, 0, BANNER_SHADOW_COLOR, BANNER_SHADOW_COLOR,
            BANNER_SHADOW_COLOR, BANNER_SHADOW_ALPHA);
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0,
            0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
      } else {
        shiftY = 0;
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      }

      rowTop = g_MenuChoices.optionsChoice * OPTION_ROW_SPACING;
      DrawTextureRect(&gfx, (LEFT_ARROW_LEFT + shiftX) << 2,
          (rowTop + ARROW_TOP + shiftY) << 2,
          (LEFT_ARROW_LEFT + ARROW_SIZE + shiftX) << 2,
          (rowTop + ARROW_TOP + ARROW_SIZE + shiftY) << 2, G_TX_RENDERTILE, 0,
          LEFT_ARROW_ROW << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      rowTop = g_MenuChoices.optionsChoice * OPTION_ROW_SPACING;
      DrawTextureRect(&gfx, (RIGHT_ARROW_LEFT + shiftX) << 2,
          (rowTop + ARROW_TOP + shiftY) << 2,
          (RIGHT_ARROW_LEFT + ARROW_SIZE + shiftX) << 2,
          (rowTop + ARROW_TOP + ARROW_SIZE + shiftY) << 2, G_TX_RENDERTILE, 0,
          RIGHT_ARROW_ROW << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  if (IsGameFlagSet(GAME_FLAG_UNK55)) {
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    if (g_MenuChoices.optionsChoice == OPTIONS_CHOICE_SONG) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    if (g_SoundTestSong == NO_SONG) {
      /* The original passes the song, though STOP prints no number */
      sprintf(g_PrintBuffer, "BGM TEST : STOP", g_SoundTestSong);
      DrawShadowedText(&gfx, SONG_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
    } else {
      sprintf(g_PrintBuffer, "BGM TEST : %2d", g_SoundTestSong);
      DrawShadowedText(&gfx, SONG_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
      sprintf(g_PrintBuffer, D_80231514_ovl_01,
          g_SoundTestSongNames[g_SoundTestSong]);
      DrawShadowedText(&gfx, SONG_NAME_X, TEST_NAME_Y, g_PrintBuffer);
    }

    if (g_MenuChoices.optionsChoice == OPTIONS_CHOICE_SONG_REVERB) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " REVERB: %3d", g_SoundTestSongReverb);
    DrawShadowedText(&gfx, SONG_TEST_X, TEST_REVERB_Y, g_PrintBuffer);
    if (g_MenuChoices.optionsChoice == OPTIONS_CHOICE_SONG_FREQUENCY) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " PICH  : %3.1f", g_SoundTestSongFrequency);
    DrawShadowedText(&gfx, SONG_TEST_X, TEST_PITCH_Y, g_PrintBuffer);
    if (g_MenuChoices.optionsChoice == OPTIONS_CHOICE_SONG_TEMPO) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " TEMPO : %3d", g_SoundTestSongTempo);
    DrawShadowedText(&gfx, SONG_TEST_X, TEST_TEMPO_Y, g_PrintBuffer);
    if (g_MenuChoices.optionsChoice == OPTIONS_CHOICE_SOUND) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, "SE TEST : %3d", g_SoundTestSound);
    DrawShadowedText(&gfx, SOUND_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
    sprintf(g_PrintBuffer, D_80231514_ovl_01,
        g_SoundTestSoundNames[g_SoundTestSound]);
    DrawShadowedText(&gfx, SOUND_NAME_X, TEST_NAME_Y, g_PrintBuffer);
    if (g_MenuChoices.optionsChoice == OPTIONS_CHOICE_SOUND_REVERB) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " REVERB: %3d", g_SoundTestSoundReverb);
    DrawShadowedText(&gfx, SOUND_TEST_X, TEST_REVERB_Y, g_PrintBuffer);
    if (g_MenuChoices.optionsChoice == OPTIONS_CHOICE_SOUND_FREQUENCY) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " PICH  : %3.1f", g_SoundTestSoundFrequency);
    DrawShadowedText(&gfx, SOUND_TEST_X, TEST_PITCH_Y, g_PrintBuffer);
  }

  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  if (g_IsEraseWindowUp == TRUE) {
    func_80226A58_ovl_01(&gfx, ERASE_WINDOW_WIDTH, ERASE_WINDOW_HEIGHT,
        ERASE_WINDOW_CENTER_X, ERASE_WINDOW_CENTER_Y, NO_OK_TEXT,
        memory->eraseChoice);
    if (IsMenuWindowOpen()) {
      SetTextPaletteColor(ERASE_MESSAGE_PALETTE_RED,
          ERASE_MESSAGE_PALETTE_GREEN, ERASE_MESSAGE_PALETTE_BLUE, 0);
      SetTextSlot(ERASE_MESSAGE_X, ERASE_MESSAGE_Y, ERASE_MESSAGE, 1,
          ERASE_MESSAGE_SLOT, TEXT_FLAG_CENTERED, ERASE_MESSAGE_RED,
          ERASE_MESSAGE_GREEN, ERASE_MESSAGE_BLUE, 255);
    } else {
      SetTextSlot(0, 0, NO_MESSAGE, 1, ERASE_MESSAGE_SLOT, TEXT_FLAG_TYPED, 255,
          255, 255, 255);
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_80206A68_ovl_01);
#endif

void func_80207D08_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 caddyIndex;

  func_801FB54C_ovl_01(&gfx, 61);
  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  DrawShadowedText(
      &gfx, CADDY_SELECT_TITLE_X, CADDY_SELECT_TITLE_Y, D_8023176C_ovl_01);
  for (caddyIndex = 0; caddyIndex != CADDY_COUNT; caddyIndex++) {
    if (g_MenuChoices.unk1 == caddyIndex) {
      SetTextColor(&gfx, 255, 255, 0);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    DrawShadowedText(&gfx, CADDY_SELECT_NAME_X,
        (caddyIndex * CADDY_SELECT_LINE_HEIGHT) + CADDY_SELECT_FIRST_Y,
        D_8022C0D0_ovl_01[caddyIndex]);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Defined after the function, as the INCLUDE_ASM before it leaves the assembler
 * in .text */
const char D_8023176C_ovl_01[] = "CADDY SELECT";

void func_80207E18_ovl_01(Gfx** displayListHead) {}

void func_80207E20_ovl_01(void) {}

void func_80207E28_ovl_01(Gfx** displayListHead, void* image, s32 x, s32 y) {
  Gfx* gfx = *displayListHead;
  s32 unused[8];

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPPipeSync(gfx++);
  gDPLoadTextureTile_4b(gfx++,
      ((u32)image + PALETTED_TEXELS_OFFSET +
          (D_8022F8C8_ovl_01 * ANIMATED_ICON_IMAGE_SIZE)) &
          ~7,
      G_IM_FMT_CI, ANIMATED_ICON_SIZE, ANIMATED_ICON_SIZE, 0, 0,
      ANIMATED_ICON_SIZE - 1, ANIMATED_ICON_SIZE - 1, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, x << 2, y << 2, (x + ANIMATED_ICON_SIZE) << 2,
      (y + ANIMATED_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  D_8022F8CD_ovl_01++;
  if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
    D_8022F8CD_ovl_01 = 0;
    D_8022F8C8_ovl_01++;
    if (D_8022F8C8_ovl_01 >= ANIMATED_ICON_FRAME_COUNT) {
      D_8022F8C8_ovl_01 = 0;
    }
  }

  *displayListHead = gfx;
}

/*
 * Doesn't match: the two growths swap registers ($t1 and $t8), and the
 * right and bottom edges add their constants last.
 */
#ifdef NON_MATCHING
void func_802081E8_ovl_01(Gfx** displayListHead, s32 left, s32 top,
    u8 starCount, u8 enlargesChosenHole) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  void* image = D_801B93E0[D_8022F8CC_ovl_01];
  u8 starIndex = 0;
  s32 growWidth = 0;
  s32 growHeight;

  gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
      G_IM_FMT_CI, STAR_WIDTH, STAR_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  if (starCount > MAX_STAR_COUNT) {
    starCount = MAX_STAR_COUNT;
  }

  if (starCount == 0) {
    starCount = 1;
  }

  for (; starIndex < starCount; starIndex++) {
    growWidth = 0;
    if (enlargesChosenHole == TRUE) {
      growHeight = 0;
      if (starIndex == g_MenuChoices.holeIndex) {
        growWidth = CHOSEN_STAR_GROWTH;
        growHeight = CHOSEN_STAR_GROWTH;
      }
    } else {
      growHeight = 0;
    }

    gSPScisTextureRectangle(gfx++,
        (left + starIndex * STAR_WIDTH - (growWidth / 2)) << 2,
        (top - (growHeight / 2)) << 2,
        (left + starIndex * STAR_WIDTH + (growWidth / 2) + STAR_WIDTH) << 2,
        (top + (growHeight / 2) + STAR_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        (STAR_WIDTH << 10) / (growWidth + STAR_WIDTH),
        (STAR_HEIGHT << 10) / (growHeight + STAR_HEIGHT));
    gDPPipeSync(gfx++);
  }

  D_8022F8CD_ovl_01++;
  if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
    D_8022F8CD_ovl_01 = 0;
    D_8022F8C8_ovl_01++;
    if (D_8022F8C8_ovl_01 >= STAR_FRAME_COUNT) {
      D_8022F8C8_ovl_01 = 0;
    }

    D_8022F8CC_ovl_01 = D_8022F8CC_ovl_01 == 0;
    OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C8_ovl_01], &asset);
    ReadEncodedAsset(D_801B93E0[D_8022F8CC_ovl_01], &asset);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_802081E8_ovl_01);
#endif

/*
 * func_8020879C draws the course's Ring Shot holes as stars on a hexagon
 * around a centre, gold where the hole's star was won and silver where not,
 * with the chosen hole larger. Nothing calls it.
 */
#define RING_SHOT_STAR_RADIUS 56
#define RING_SHOT_STAR_WIDTH 32
#define RING_SHOT_STAR_HEIGHT 24
#define RING_SHOT_STAR_GROWTH 16
#define RING_SHOT_STAR_FRAME_DURATION 2
#define GOLD_STAR_PALETTE 0
#define SILVER_STAR_PALETTE 1
/* The silver star's texels follow the gold star's in texture memory */
#define SILVER_STAR_TMEM 128
#define SILVER_STAR_TILE 1

extern s32 D_8022F884_ovl_01[STAR_FRAME_COUNT]; /* The silver star's frames */

void func_8020879C_ovl_01(Gfx** displayListHead, s32 centerX, s32 centerY) {
  EncodedAsset asset;
  SaveData* save = GetSaveData();
  Gfx* gfx = *displayListHead;
  f32 starPositions[RING_SHOT_HOLE_COUNT][2] = {
      {0.0f, -1.0f},
      {0.89f, -0.45f},
      {0.89f, 0.45f},
      {0.0f, 1.0f},
      {-0.89f, 0.45f},
      {-0.89f, -0.45f},
  };
  void* image;
  s32 tile;
  u8 holeIndex;
  s32 growWidth;
  s32 growHeight;
  s32 left;
  s32 top;

  image = D_8010526C[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal16(
      gfx++, GOLD_STAR_PALETTE, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
      G_IM_FMT_CI, RING_SHOT_STAR_WIDTH, RING_SHOT_STAR_HEIGHT,
      GOLD_STAR_PALETTE, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  image = D_800FF180[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal16(
      gfx++, SILVER_STAR_PALETTE, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadMultiBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
      SILVER_STAR_TMEM, SILVER_STAR_TILE, G_IM_FMT_CI, RING_SHOT_STAR_WIDTH,
      RING_SHOT_STAR_HEIGHT, SILVER_STAR_PALETTE, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  for (holeIndex = 0; holeIndex < RING_SHOT_HOLE_COUNT; holeIndex++) {
    growWidth = 0;
    if (holeIndex == g_MenuChoices.holeIndex) {
      growWidth = RING_SHOT_STAR_GROWTH;
      growHeight = RING_SHOT_STAR_GROWTH;
    } else {
      growHeight = 0;
    }

    left = starPositions[holeIndex][0] * RING_SHOT_STAR_RADIUS;
    top = starPositions[holeIndex][1] * RING_SHOT_STAR_RADIUS;
    left += centerX - (RING_SHOT_STAR_WIDTH / 2);
    top += centerY - (RING_SHOT_STAR_HEIGHT / 2);

    /* A hole without its star draws from the silver star's tile */
    tile = save->hasRingShotStar[g_MenuChoices.courseIndex][holeIndex] == FALSE;
    gSPScisTextureRectangle(gfx++, (left - (growWidth / 2)) << 2,
        (top - (growHeight / 2)) << 2,
        (left + RING_SHOT_STAR_WIDTH + (growWidth / 2)) << 2,
        (top + RING_SHOT_STAR_HEIGHT + (growHeight / 2)) << 2, tile, 0, 0,
        (RING_SHOT_STAR_WIDTH << 10) / (growWidth + RING_SHOT_STAR_WIDTH),
        (RING_SHOT_STAR_HEIGHT << 10) / (growHeight + RING_SHOT_STAR_HEIGHT));
    gDPPipeSync(gfx++);
  }

  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == RING_SHOT_STAR_FRAME_DURATION) {
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= STAR_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }

    D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
    OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
    OpenEncodedAsset(D_8022F884_ovl_01[D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_800FF180[D_8022F8C4_ovl_01], &asset);
  }

  *displayListHead = gfx;
}

void func_80208F8C_ovl_01(Gfx** displayListHead, s32 left, s32 top) {
  Gfx* gfx = *displayListHead;
  s16 stripIndex;
  s32 rowCount;
  s32 unused[8];

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPLoadTLUT_pal256(
      gfx++, ((u32)D_800FBE30[D_8022F8CE_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != TOAD_STRIP_COUNT; stripIndex++) {
    rowCount = TOAD_LAST_STRIP_HEIGHT;
    if (stripIndex == 0) {
      rowCount = TOAD_STRIP_HEIGHT;
    }

    gDPLoadTextureBlock(gfx++,
        ((u32)D_800FBE30[D_8022F8CE_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * TOAD_WIDTH * TOAD_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, TOAD_WIDTH, rowCount, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2,
        (top + (stripIndex * TOAD_STRIP_HEIGHT)) << 2, (left + TOAD_WIDTH) << 2,
        (top + (stripIndex * TOAD_STRIP_HEIGHT) + rowCount) << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

void DrawBirdieBadgeLabel(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  DrawShadowedText(&gfx, 64, 32, g_BirdieBadgeLabelText);
  *displayListHead = gfx;
}

s32 GetOpenCourseCount(void) {
  s32 coursePoints = GetSaveData()->coursePoints;

  if (IsGameFlagSet(GAME_FLAG_OPEN_SIX_COURSES)) {
    return 6;
  }

  if (IsGameFlagSet(GAME_FLAG_OPEN_FIVE_COURSES)) {
    return 5;
  }

  if (IsGameFlagSet(GAME_FLAG_OPEN_FOUR_COURSES)) {
    return 4;
  }

  if (IsGameFlagSet(GAME_FLAG_OPEN_THREE_COURSES)) {
    return 3;
  }

  if (IsGameFlagSet(GAME_FLAG_OPEN_TWO_COURSES)) {
    return 2;
  }

  return CountOpenCourses(coursePoints);
}
