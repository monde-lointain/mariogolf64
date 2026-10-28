/*
 * Draws what helps aim the shot: the players' tags, the sign over the target,
 * the flag over the pin and the shot preview. Each player's tag, "1P" or "1C"
 * and so on, is drawn beside their ball with a line joining the two. It sits
 * off one of the ball's corners, and moves to another corner when it would
 * cover a tag drawn before it.
 */
#include "common.h"

#include "asset.h"
#include "camera.h"
#include "club.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "math_util.h"
#include "overlay_manager.h"
#include "play.h"
#include "round_hud.h"
#include "save.h"
#include "screen.h"
#include "shot_camera.h"
#include "surface.h"
#include "variables.h"

#define LETTERBOX_STATE_NONE 0
#define LETTERBOX_STATE_UNK3 3
#define LETTERBOX_STATE_UNK4 4
#define LETTERBOX_STATE_UNK5 5

/* The players whose balls are tagged */
enum ShotLabels {
  SHOT_LABELS_NONE = -1,
  /* From the overhead camera, every player's ball */
  SHOT_LABELS_OVERHEAD,
  /* While a player takes their shot, the other balls in play */
  SHOT_LABELS_OTHER_BALLS
};

/* The corners of the ball that a tag sits off, clockwise from the lower right
 */
#define NO_CORNER -1
#define CORNER_LOWER_RIGHT 0
#define CORNER_LOWER_LEFT 1
#define CORNER_UPPER_LEFT 2
#define CORNER_UPPER_RIGHT 3
#define CORNER_COUNT 4
/* Only the upper corners are used while a shot is taken, keeping the tags clear
 * of the golfer */
#define UPPER_CORNER_COUNT 2

#define TAG_WIDTH 16
#define TAG_HEIGHT 8
/* In bytes, at four bits a texel */
#define TAG_IMAGE_SIZE (TAG_WIDTH * TAG_HEIGHT / 2)
/* Balls this far off the screen are not tagged */
#define TAG_SCREEN_MARGIN 64
/* The tags of the computer's players follow the four "1P" to "4P" */
#define COMPUTER_TAG_OFFSET 4
#define TAG_ALPHA 255
#define TAG_SHADOW_ALPHA 79
/* The line from a tag spans this far beside it */
#define LEADER_LINE_NEAR_X 6
#define LEADER_LINE_FAR_X 22
#define LEADER_LINE_NEAR_Y 4
#define LEADER_LINE_FAR_Y 12

typedef struct ShotLabel {
  /* 0x0 */ s32 x;
  /* 0x4 */ s32 y;
  /* 0x8 */ s32 depth;
  /* 0xC */ s32 corner; /* NO_CORNER when the tag is hidden */
} ShotLabel; /* size = 0x10 */

/* D_800C3084's bit for a shot from the green */
#define SHOT_UNK3084_ON_GREEN 1

/* The sign shows the shot's distance, with the target's height below it */
#define SIGN_WIDTH 40
#define SIGN_HEIGHT 24
#define SIGN_BOARD_HEIGHT 18 /* Above the post */
#define SIGN_DISTANCE_TEXT_Y 15
/* The distance is printed after the height, in the same buffer */
#define DISTANCE_TEXT_OFFSET 10
#define SIGN_HEIGHT_TEXT_Y 23
#define OUT_OF_BOUNDS_TEXT_Y 31
/* The sign fades from where ProjectPoint returns these */
#define SIGN_FADE_START_DEPTH -2000
#define SIGN_FADE_END_DEPTH -149
#define SIGN_MIN_ALPHA 64
#define SIGN_FADE_ALPHA_RANGE (255 - SIGN_MIN_ALPHA)
#define SIGN_FADE_DEPTH_RANGE 1850
/* The height difference shown, in yards, and the most of it that tints the text
 */
#define MAX_HEIGHT_YARDS 99
#define MAX_TINT_YARDS 25
#define TINT_PER_YARD 10
#define TEXT_SHADOW_ALPHA 127
/* The surfaces under the target that warn that it is out of bounds */
#define SURFACE_KIND_UNK13 13
#define SURFACE_KIND_UNK21 21
#define OUT_OF_BOUNDS_GREEN 95
#define OUT_OF_BOUNDS_GREEN_SWING 45
#define OUT_OF_BOUNDS_PULSE_STEP (M_PI / 6)

/* The pin flag's frames are 8 by 16, four to an image */
#define PIN_FLAG_IMAGE_WIDTH 32
#define PIN_FLAG_IMAGE_HEIGHT 16
#define PIN_FLAG_IMAGE_SIZE (PIN_FLAG_IMAGE_WIDTH * PIN_FLAG_IMAGE_HEIGHT / 2)
#define PIN_FLAG_FRAME_WIDTH 8
#define PIN_FLAG_LEFT 3
#define PIN_FLAG_RIGHT 5
#define PIN_FLAG_WAVE_MASK 15
#define PIN_FLAG_WAVE_TICKS 4 /* Per frame */
#define PIN_FLAG_WAVE_HALF 8
/* Off the green, the flag sits at this height over the cup */
#define PIN_FLAG_TOP_Y 40
#define PIN_FLAG_DROP 0xB400
/* The images that the flag uses */
#define PIN_FLAG_GREEN_IMAGE 0
#define PIN_FLAG_IMAGE 1
#define PIN_FLAG_HIDDEN 2
/* From the green the flag fades out, and is gone by PIN_FLAG_HIDE_FRAME */
#define PIN_FLAG_FADE_FRAME 114
#define PIN_FLAG_HIDE_FRAME 120
#define PIN_FLAG_FADE_STEP 32

/* Draws the flag opaque, keeping the coverage of its alpha */
#define PIN_FLAG_RENDER_MODE \
  AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
      GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
#define PIN_FLAG_RENDER_MODE2 \
  AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
      GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)

enum SpotState {
  SPOT_HIDDEN = -1,
  SPOT_SHOWN
};

/* Where a point is on the screen */
typedef struct ScreenSpot {
  /* 0x0 */ s32 x;
  /* 0x4 */ s32 y;
  /* 0x8 */ s32 isHidden; /* SPOT_HIDDEN behind the camera, else SPOT_SHOWN */
} ScreenSpot; /* size = 0xC */

extern u8 f_PlayerTagColors[MAX_PLAYER_COUNT][3];
extern ShotLabel f_ShotLabels[MAX_PLAYER_COUNT];
extern s32 f_ShotLabelPlayers;
/* Where a tag's centre sits from the ball at each corner */
extern s8 f_TagOffsetsX[CORNER_COUNT];
extern s8 f_TagOffsetsY[CORNER_COUNT];
extern f32 f_OutOfBoundsPulseAngle;
extern s32 f_PinFlagWaveFrame;
extern s32 D_800C3084;
extern s32 D_800C307C; /* Frames, up to PIN_FLAG_HIDE_FRAME, since the shot was
                          set up */

void DrawShotLabels(Gfx** displayListHead);
void DrawShotPreview(CameraMatrices* matrices);
/*
 * Without a prototype, as the calls pass x and y as they are rather than as
 * s16s. Including text.h would bring in its prototype.
 */
void DrawCenteredShadowedText();
extern void* g_ShadowedFontImage;
#define SHADOWED_FONT_IMAGE_WIDTH 256
#define SHADOWED_FONT_IMAGE_HEIGHT 32

/*
 * DrawLeaderLine and DrawPlayerTag are nested functions, a GNU C extension.
 * The tag loop keeps its corner pointer and label offsets in other registers.
 */
#ifdef NON_MATCHING
/* Draws the tags of the players' balls, moving each to a corner that is clear
 * of the others */
void DrawShotLabels(Gfx** displayListHead) {
  ScreenPoint point;
  ScreenSpot cup;
  ShotLabel* label;
  Gfx* gfx = *displayListHead;
  s32 players;
  s32 corner;
  s32 tries;
  s32 overlaps;
  s32 i;
  s32 j;

  /* Draws the line from a tag at x and y to its ball, which lies off corner */
  void DrawLeaderLine(s32 x, s32 y, s32 corner) {
    gDPPipeSync(gfx++);
    switch (corner) {
      case CORNER_LOWER_RIGHT:
        gSPScisTextureRectangle(gfx++, (x - LEADER_LINE_FAR_X) << 2,
            (y - LEADER_LINE_FAR_Y) << 2, (x - LEADER_LINE_NEAR_X) << 2,
            (y - LEADER_LINE_NEAR_Y) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
            1 << 10);
        break;
      case CORNER_LOWER_LEFT:
        gSPScisTextureRectangle(gfx++, (x + LEADER_LINE_NEAR_X) << 2,
            (y - LEADER_LINE_FAR_Y) << 2, (x + LEADER_LINE_FAR_X) << 2,
            (y - LEADER_LINE_NEAR_Y) << 2, G_TX_RENDERTILE, TAG_WIDTH << 5, 0,
            1 << 10, 1 << 10);
        break;
      case CORNER_UPPER_LEFT:
        gSPScisTextureRectangle(gfx++, (x + LEADER_LINE_NEAR_X) << 2,
            (y + LEADER_LINE_NEAR_Y) << 2, (x + LEADER_LINE_FAR_X) << 2,
            (y + LEADER_LINE_FAR_Y) << 2, G_TX_RENDERTILE, TAG_WIDTH << 5,
            TAG_HEIGHT << 5, 1 << 10, 1 << 10);
        break;
      case CORNER_UPPER_RIGHT:
        gSPScisTextureRectangle(gfx++, (x - LEADER_LINE_FAR_X) << 2,
            (y + LEADER_LINE_NEAR_Y) << 2, (x - LEADER_LINE_NEAR_X) << 2,
            (y + LEADER_LINE_FAR_Y) << 2, G_TX_RENDERTILE, 0, TAG_HEIGHT << 5,
            1 << 10, 1 << 10);
        break;
    }

    gDPPipeSync(gfx++);
  }

  /* Draws tag number tag centred on x and y */
  void DrawPlayerTag(s32 x, s32 y, s32 corner, s32 tag) {
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++,
        ((u32)g_PlayerTagImages + PALETTED_TEXELS_OFFSET +
            tag * TAG_IMAGE_SIZE) &
            ~7,
        G_IM_FMT_CI, TAG_WIDTH, TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (x - TAG_WIDTH / 2) << 2,
        (y - TAG_HEIGHT / 2) << 2, (x + TAG_WIDTH / 2) << 2,
        (y + TAG_HEIGHT / 2) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  }

  players = SHOT_LABELS_NONE;
  if (g_Cameras[0].view.mode == CAMERA_MODE_UNK2) {
    players = SHOT_LABELS_OVERHEAD;
  } else if (g_GameState.unk0 == GAME_STATE_UNK6 &&
      g_CurrentShot.playerIndex >= 0 &&
      g_CurrentShot.playerIndex < MAX_PLAYER_COUNT &&
      g_Players[g_CurrentShot.playerIndex].unkC == PLAYER_STATE_UNK2 &&
      (g_LetterboxState != LETTERBOX_STATE_NONE &
          g_LetterboxState != LETTERBOX_STATE_UNK5)) {
    players = SHOT_LABELS_OTHER_BALLS;
  }

  if (f_ShotLabelPlayers != players) {
    for (i = 0; i != MAX_PLAYER_COUNT; i++) {
      f_ShotLabels[i].corner = NO_CORNER;
    }
  }

  f_ShotLabelPlayers = players;
  if (players != SHOT_LABELS_NONE) {
    /* This projects the cup with its x for all three coordinates */
    if (ProjectPoint(g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
            g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
            g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
            &point) < 0) {
      cup.isHidden = SPOT_SHOWN;
      cup.x = point.x;
      cup.y = point.y;
    } else {
      cup.isHidden = SPOT_HIDDEN;
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
      label = &f_ShotLabels[i];
      if (players == SHOT_LABELS_OTHER_BALLS &&
          (g_CurrentShot.playerIndex == i || g_Players[i].strokeCount == 0)) {
        label->corner = NO_CORNER;
        continue;
      }

      if (g_Players[i].unk26 != 0) {
        label->corner = NO_CORNER;
        continue;
      }

      if (ProjectPoint(
              g_Players[i].ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
              g_Players[i].ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
              g_Players[i].ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE,
              &point) >= 0) {
        label->corner = NO_CORNER;
        continue;
      }

      if (point.x < -TAG_SCREEN_MARGIN ||
          point.x > SCREEN_WIDTH + TAG_SCREEN_MARGIN ||
          point.y < -TAG_SCREEN_MARGIN ||
          point.y > SCREEN_HEIGHT + TAG_SCREEN_MARGIN) {
        label->x = point.x;
        label->y = point.y;
        label->depth = point.depth;
        label->corner = NO_CORNER;
        continue;
      }

      if (label->corner == NO_CORNER) {
        if (players == SHOT_LABELS_OTHER_BALLS) {
          label->corner = (i & 1) + CORNER_UPPER_LEFT;
        } else {
          label->corner = i;
        }
      }

      label->x = point.x + f_TagOffsetsX[label->corner];
      label->y = point.y + f_TagOffsetsY[label->corner];
      label->depth = point.depth;
      tries = 0;
      if (players == SHOT_LABELS_OVERHEAD) {
        overlaps = FALSE;
        while (TRUE) {
          corner = (label->corner + tries) % CORNER_COUNT;
          for (j = 0; j != i; j++) {
            if (f_ShotLabels[j].corner != NO_CORNER &&
                f_ShotLabels[j].x - TAG_WIDTH <
                    point.x + f_TagOffsetsX[corner] &&
                point.x + f_TagOffsetsX[corner] <
                    f_ShotLabels[j].x + TAG_WIDTH &&
                f_ShotLabels[j].y - TAG_HEIGHT <
                    point.y + f_TagOffsetsY[corner] &&
                point.y + f_TagOffsetsY[corner] <
                    f_ShotLabels[j].y + TAG_HEIGHT) {
              overlaps = TRUE;
            }
          }

          tries++;
          if (!overlaps) {
            break;
          }

          overlaps = FALSE;
          if (tries == CORNER_COUNT) {
            goto NEXT;
          }
        }
      } else {
        while (TRUE) {
          overlaps = FALSE;
          corner = ((f_ShotLabels[i].corner + tries) % UPPER_CORNER_COUNT) +
              CORNER_UPPER_LEFT;
          if (cup.isHidden == SPOT_SHOWN &&
              cup.x - TAG_WIDTH < point.x + f_TagOffsetsX[corner] &&
              point.x + f_TagOffsetsX[corner] < cup.x + TAG_WIDTH &&
              cup.y - TAG_HEIGHT < point.y + f_TagOffsetsY[corner] &&
              point.y + f_TagOffsetsY[corner] < cup.y + TAG_HEIGHT) {
            /* The tag would cover the cup, so it goes to the other upper corner
             */
            if (corner == CORNER_UPPER_LEFT) {
              f_ShotLabels[i].corner = CORNER_UPPER_RIGHT;
            } else {
              f_ShotLabels[i].corner = CORNER_UPPER_LEFT;
            }

            f_ShotLabels[i].x = point.x + f_TagOffsetsX[f_ShotLabels[i].corner];
            f_ShotLabels[i].y = point.y + f_TagOffsetsY[f_ShotLabels[i].corner];
            goto NEXT;
          }

          for (j = 0; j != i; j++) {
            if (f_ShotLabels[j].corner != NO_CORNER &&
                f_ShotLabels[j].x - TAG_WIDTH <
                    point.x + f_TagOffsetsX[corner] &&
                point.x + f_TagOffsetsX[corner] <
                    f_ShotLabels[j].x + TAG_WIDTH &&
                f_ShotLabels[j].y - TAG_HEIGHT <
                    point.y + f_TagOffsetsY[corner] &&
                point.y + f_TagOffsetsY[corner] <
                    f_ShotLabels[j].y + TAG_HEIGHT) {
              overlaps = TRUE;
            }
          }

          tries++;
          if (!overlaps) {
            break;
          }

          if (tries == UPPER_CORNER_COUNT) {
            goto NEXT;
          }
        }
      }

      f_ShotLabels[i].x = point.x + f_TagOffsetsX[corner];
      f_ShotLabels[i].y = point.y + f_TagOffsetsY[corner];
      f_ShotLabels[i].corner = corner;

NEXT:;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    if (players == SHOT_LABELS_OTHER_BALLS) {
      gDPSetRenderMode(gfx++,
          AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA |
              CVG_X_ALPHA | ALPHA_CVG_SEL |
              GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
          AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA |
              CVG_X_ALPHA | ALPHA_CVG_SEL |
              GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM));
    } else {
      gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(
        gfx++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
    gDPLoadTextureBlock_4b(gfx++,
        ((u32)g_LeaderLineImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I,
        TAG_WIDTH, TAG_HEIGHT, 0, G_TX_MIRROR | G_TX_WRAP,
        G_TX_MIRROR | G_TX_WRAP, 4, 3, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (f_ShotLabels[i].corner != NO_CORNER) {
        gDPSetPrimDepth(gfx++, f_ShotLabels[i].depth, 0);
        gDPSetPrimColor(gfx++, 0, 0, f_PlayerTagColors[i][0],
            f_PlayerTagColors[i][1], f_PlayerTagColors[i][2], TAG_ALPHA);
        DrawLeaderLine(
            f_ShotLabels[i].x, f_ShotLabels[i].y, f_ShotLabels[i].corner);
      }
    }

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(
        gfx++, 0, ((u32)g_PlayerTagImages + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (f_ShotLabels[i].corner != NO_CORNER) {
        gDPSetPrimDepth(gfx++, f_ShotLabels[i].depth, 0);
        DrawPlayerTag(f_ShotLabels[i].x, f_ShotLabels[i].y,
            f_ShotLabels[i].corner,
            g_Players[i].unk1E[0] ? i + COMPUTER_TAG_OFFSET : i);
      }
    }

    /* Then translucent over what covers them, so that they show through */
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0,
        TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0);
    gDPLoadTextureBlock_4b(gfx++,
        ((u32)g_LeaderLineImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I,
        TAG_WIDTH, TAG_HEIGHT, 0, G_TX_MIRROR | G_TX_WRAP,
        G_TX_MIRROR | G_TX_WRAP, 4, 3, G_TX_NOLOD, G_TX_NOLOD);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (f_ShotLabels[i].corner != NO_CORNER) {
        gDPSetPrimDepth(gfx++, f_ShotLabels[i].depth, 0);
        gDPSetPrimColor(gfx++, 0, 0, f_PlayerTagColors[i][0],
            f_PlayerTagColors[i][1], f_PlayerTagColors[i][2], TAG_SHADOW_ALPHA);
        DrawLeaderLine(
            f_ShotLabels[i].x, f_ShotLabels[i].y, f_ShotLabels[i].corner);
      }
    }

    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
        TEXEL0, PRIMITIVE, 0, TEXEL0, 0);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, TAG_SHADOW_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(
        gfx++, 0, ((u32)g_PlayerTagImages + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (f_ShotLabels[i].corner != NO_CORNER) {
        gDPSetPrimDepth(gfx++, f_ShotLabels[i].depth, 0);
        DrawPlayerTag(f_ShotLabels[i].x, f_ShotLabels[i].y,
            f_ShotLabels[i].corner,
            g_Players[i].unk1E[0] ? i + COMPUTER_TAG_OFFSET : i);
      }
    }

    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_labels", DrawLeaderLine);

INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_labels", DrawPlayerTag);

INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_labels", DrawShotLabels);
#endif

/*
 * Draws what helps aim the shot: the players' tags, the sign over the target
 * with the shot's distance and the target's height, the flag over the pin,
 * then the shot preview.
 */
void DrawAimGuides(CameraMatrices* matrices) {
  ScreenPoint point;
  Vec3 position;
  s32 alpha;
  s32 depth;
  s32 heightYards;
  s32 tintYards;
  s32 distance;
  s32 flagImage;
  s32 surfaceKind;
  Club* club;
  s32 outOfBoundsGreen;

  club = GetSelectedClub();
  outOfBoundsGreen = sinf(f_OutOfBoundsPulseAngle) * OUT_OF_BOUNDS_GREEN_SWING;
  surfaceKind =
      GetSurfaceInfoAt(g_CurrentShot.unk44[AXIS_X], g_CurrentShot.unk44[AXIS_Z])
          ->kind;
  if (g_IsCupViewShown != 0) {
    return;
  }

  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
  DrawShotLabels(&g_DisplayListHead);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 255);
  gDPPipeSync(g_DisplayListHead++);
  if (!(club->surfaceMask & CLUB_SURFACE_GREEN) && g_RoundHud.unkF != 0) {
    position[AXIS_X] = g_CurrentShot.unk44[AXIS_X] * COURSE_TO_WORLD_SCALE;
    position[AXIS_Y] = g_CurrentShot.unk44[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    position[AXIS_Z] = g_CurrentShot.unk44[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    depth = ProjectPoint(
        position[AXIS_X], position[AXIS_Y], position[AXIS_Z], &point);
    if (depth < 0) {
      heightYards =
          (g_CurrentShot.ballPosition[AXIS_Y] - g_CurrentShot.unk44[AXIS_Y]) *
          YARDS_PER_UNIT;
      if (heightYards > MAX_HEIGHT_YARDS) {
        heightYards = MAX_HEIGHT_YARDS;
      }

      if (heightYards < -MAX_HEIGHT_YARDS) {
        heightYards = -MAX_HEIGHT_YARDS;
      }

      tintYards = heightYards;
      if (tintYards > MAX_TINT_YARDS) {
        tintYards = MAX_TINT_YARDS;
      }

      if (tintYards < -MAX_TINT_YARDS) {
        tintYards = -MAX_TINT_YARDS;
      }

      /* The sign fades with distance, except on PAUSE_SCREEN_UNK4 */
      if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
        goto OPAQUE;
      }

      alpha = SIGN_MIN_ALPHA;
      if (depth >= SIGN_FADE_START_DEPTH) {
        if (depth >= SIGN_FADE_END_DEPTH) {
OPAQUE:

          alpha = 255;
        } else {
          alpha = ((depth - SIGN_FADE_START_DEPTH) * SIGN_FADE_ALPHA_RANGE /
                      SIGN_FADE_DEPTH_RANGE) +
              SIGN_MIN_ALPHA;
        }
      }

      distance = GetSelectedShotDistance();
      gDPPipeSync(g_DisplayListHead++);
      gDPSetPrimDepth(g_DisplayListHead++, point.depth, 0);
      gDPPipeSync(g_DisplayListHead++);
      gDPSetDepthSource(g_DisplayListHead++, G_ZS_PRIM);
      gDPSetCombineMode(
          g_DisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
      gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, alpha);
      gDPPipeSync(g_DisplayListHead++);
      gDPSetRenderMode(
          g_DisplayListHead++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
      gDPPipeSync(g_DisplayListHead++);
      gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
      gDPPipeSync(g_DisplayListHead++);
      gDPSetAlphaCompare(g_DisplayListHead++, G_AC_THRESHOLD);
      gDPSetBlendColor(g_DisplayListHead++, 255, 255, 255, 1);
      gDPLoadTLUT_pal16(g_DisplayListHead++, 0,
          ((u32)g_DistanceSignImage + IMAGE_HEADER_SIZE) & ~7);
      gDPLoadTextureTile_4b(g_DisplayListHead++,
          ((u32)g_DistanceSignImage + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
          SIGN_WIDTH, SIGN_HEIGHT, 0, 0, SIGN_WIDTH - 1, SIGN_HEIGHT - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, alpha);
      gDPPipeSync(g_DisplayListHead++);
      gSPScisTextureRectangle(g_DisplayListHead++,
          (point.x - (SIGN_WIDTH / 2)) << 2, (point.y - SIGN_HEIGHT) << 2,
          (point.x + (SIGN_WIDTH / 2)) << 2,
          (point.y - SIGN_HEIGHT + SIGN_BOARD_HEIGHT) << 2, G_TX_RENDERTILE, 0,
          0, 1 << 10, 1 << 10);
      /* The post is drawn opaque */
      gDPPipeSync(g_DisplayListHead++);
      gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 255);
      gDPPipeSync(g_DisplayListHead++);
      gSPScisTextureRectangle(g_DisplayListHead++,
          (point.x - (SIGN_WIDTH / 2)) << 2,
          (point.y - SIGN_HEIGHT + SIGN_BOARD_HEIGHT) << 2,
          (point.x + (SIGN_WIDTH / 2)) << 2, point.y << 2, G_TX_RENDERTILE, 0,
          SIGN_BOARD_HEIGHT << 5, 1 << 10, 1 << 10);
      gDPPipeSync(g_DisplayListHead++);
      gDPPipeSync(g_DisplayListHead++);
      gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
      gDPSetCombineMode(
          g_DisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
      gDPLoadTextureBlock_4b(g_DisplayListHead++,
          ((u32)g_ShadowedFontImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_IA,
          SHADOWED_FONT_IMAGE_WIDTH, SHADOWED_FONT_IMAGE_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        sprintf(g_PrintBuffer + DISTANCE_TEXT_OFFSET, "%2dm", distance);
      } else {
        sprintf(g_PrintBuffer + DISTANCE_TEXT_OFFSET, "%2dy", distance);
      }

      if (g_LetterboxState != LETTERBOX_STATE_UNK3 &&
          g_LetterboxState != LETTERBOX_STATE_UNK4) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 255);
        gDPPipeSync(g_DisplayListHead++);
        DrawCenteredShadowedText(&g_DisplayListHead, point.x,
            point.y - SIGN_DISTANCE_TEXT_Y,
            g_PrintBuffer + DISTANCE_TEXT_OFFSET);
      }

      /* Uphill is tinted red and downhill blue */
      gDPPipeSync(g_DisplayListHead++);
      if (tintYards > 0) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255,
            255 - (tintYards * TINT_PER_YARD),
            255 - (tintYards * TINT_PER_YARD), 255);
        gDPPipeSync(g_DisplayListHead++);
        sprintf(g_PrintBuffer, "%dyUP", heightYards);
      } else if (tintYards < 0) {
        sprintf(g_PrintBuffer, "%dyDW", -heightYards);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0,
            255 + (tintYards * TINT_PER_YARD),
            255 + (tintYards * TINT_PER_YARD), 255, 255);
        gDPPipeSync(g_DisplayListHead++);
      } else {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 255);
        gDPPipeSync(g_DisplayListHead++);
        sprintf(g_PrintBuffer, "FLAT");
      }

      if (g_LetterboxState != LETTERBOX_STATE_UNK3 &&
          g_LetterboxState != LETTERBOX_STATE_UNK4) {
        DrawCenteredShadowedText(&g_DisplayListHead, point.x,
            point.y - SIGN_HEIGHT_TEXT_Y, g_PrintBuffer);
      }

      if (surfaceKind == SURFACE_KIND_UNK13 ||
          surfaceKind == SURFACE_KIND_UNK21) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255,
            outOfBoundsGreen + OUT_OF_BOUNDS_GREEN, 0, 255);
        gDPPipeSync(g_DisplayListHead++);
        f_OutOfBoundsPulseAngle += OUT_OF_BOUNDS_PULSE_STEP;
        DrawCenteredShadowedText(
            &g_DisplayListHead, point.x, point.y - OUT_OF_BOUNDS_TEXT_Y, "OB!");
      }

      /* Then translucent, so that the text shows through what covers it */
      gDPPipeSync(g_DisplayListHead++);
      gDPSetRenderMode(g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      gDPSetCombineMode(
          g_DisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
      if (tintYards > 0) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255,
            255 - (tintYards * TINT_PER_YARD),
            255 - (tintYards * TINT_PER_YARD), TEXT_SHADOW_ALPHA);
        gDPPipeSync(g_DisplayListHead++);
      } else if (tintYards < 0) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0,
            255 + (tintYards * TINT_PER_YARD),
            255 + (tintYards * TINT_PER_YARD), 255, TEXT_SHADOW_ALPHA);
        gDPPipeSync(g_DisplayListHead++);
      } else {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(
            g_DisplayListHead++, 0, 0, 255, 255, 255, TEXT_SHADOW_ALPHA);
        gDPPipeSync(g_DisplayListHead++);
      }

      DrawCenteredShadowedText(&g_DisplayListHead, point.x,
          point.y - SIGN_HEIGHT_TEXT_Y, g_PrintBuffer);
      if (surfaceKind == SURFACE_KIND_UNK13 ||
          surfaceKind == SURFACE_KIND_UNK21) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255,
            outOfBoundsGreen + OUT_OF_BOUNDS_GREEN, 0, TEXT_SHADOW_ALPHA);
        gDPPipeSync(g_DisplayListHead++);
        f_OutOfBoundsPulseAngle += OUT_OF_BOUNDS_PULSE_STEP;
        DrawCenteredShadowedText(
            &g_DisplayListHead, point.x, point.y - OUT_OF_BOUNDS_TEXT_Y, "OB!");
      }

      gDPPipeSync(g_DisplayListHead++);
      gDPSetPrimColor(
          g_DisplayListHead++, 0, 0, 255, 255, 255, TEXT_SHADOW_ALPHA);
      gDPPipeSync(g_DisplayListHead++);
      DrawCenteredShadowedText(&g_DisplayListHead, point.x,
          point.y - SIGN_DISTANCE_TEXT_Y, g_PrintBuffer + DISTANCE_TEXT_OFFSET);
    }
  }

  if (D_800C307C < PIN_FLAG_HIDE_FRAME) {
    D_800C307C++;
  }

  if (g_RoundHud.unk12 == 1) {
    position[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    position[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
      position[AXIS_Y] =
          g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    } else {
      position[AXIS_Y] = (g_HoleSetup.cupPosition[AXIS_Y] - PIN_FLAG_DROP) *
          COURSE_TO_WORLD_SCALE;
    }

    if (ProjectPoint(
            position[AXIS_X], position[AXIS_Y], position[AXIS_Z], &point) < 0) {
      flagImage = PIN_FLAG_IMAGE;
      if (point.y < SCREEN_HEIGHT) {
        if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
          gDPSetCombineMode(
              g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
          gDPPipeSync(g_DisplayListHead++);
          gDPSetRenderMode(
              g_DisplayListHead++, PIN_FLAG_RENDER_MODE, PIN_FLAG_RENDER_MODE2);
        } else if (D_800C3084 & SHOT_UNK3084_ON_GREEN) {
          flagImage = PIN_FLAG_GREEN_IMAGE;
          if (D_800C307C == PIN_FLAG_HIDE_FRAME) {
            flagImage = PIN_FLAG_HIDDEN;
          } else if (D_800C307C >= PIN_FLAG_FADE_FRAME) {
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255,
                255 -
                    ((D_800C307C - (PIN_FLAG_FADE_FRAME - 1)) *
                        PIN_FLAG_FADE_STEP));
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(
                g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIA_PRIM,
                G_CC_MODULATEIA_PRIM);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetAlphaCompare(g_DisplayListHead++, G_AC_THRESHOLD);
            gDPSetBlendColor(g_DisplayListHead++, 255, 255, 255, 1);
          } else {
            gDPSetCombineMode(
                g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, PIN_FLAG_RENDER_MODE,
                PIN_FLAG_RENDER_MODE2);
          }

          if (point.y < PIN_FLAG_TOP_Y) {
            point.y = PIN_FLAG_TOP_Y;
          }
        } else {
          gDPSetCombineMode(
              g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
          gDPPipeSync(g_DisplayListHead++);
          gDPSetRenderMode(
              g_DisplayListHead++, PIN_FLAG_RENDER_MODE, PIN_FLAG_RENDER_MODE2);
          point.y = PIN_FLAG_TOP_Y;
        }

        if (flagImage != PIN_FLAG_HIDDEN) {
          gDPPipeSync(g_DisplayListHead++);
          gDPLoadTLUT_pal16(g_DisplayListHead++, 0,
              ((u32)g_PinFlagImages + IMAGE_HEADER_SIZE) & ~7);
          gDPPipeSync(g_DisplayListHead++);
          gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
          gDPLoadTextureBlock_4b(g_DisplayListHead++,
              ((u32)g_PinFlagImages + PALETTED_TEXELS_OFFSET +
                  (flagImage * PIN_FLAG_IMAGE_SIZE)) &
                  ~7,
              G_IM_FMT_CI, PIN_FLAG_IMAGE_WIDTH, PIN_FLAG_IMAGE_HEIGHT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(g_DisplayListHead++);

          /* The flag bobs down and back up as it waves */
          f_PinFlagWaveFrame = (f_PinFlagWaveFrame + 1) & PIN_FLAG_WAVE_MASK;
          if (f_PinFlagWaveFrame > PIN_FLAG_WAVE_HALF) {
            point.y -=
                (f_PinFlagWaveFrame - PIN_FLAG_WAVE_HALF) / PIN_FLAG_WAVE_TICKS;
          } else {
            point.y += f_PinFlagWaveFrame / PIN_FLAG_WAVE_TICKS;
          }

          gSPScisTextureRectangle(g_DisplayListHead++,
              (point.x - PIN_FLAG_LEFT) << 2,
              (point.y - PIN_FLAG_IMAGE_HEIGHT) << 2,
              (point.x + PIN_FLAG_RIGHT) << 2, point.y << 2, G_TX_RENDERTILE,
              (f_PinFlagWaveFrame / PIN_FLAG_WAVE_TICKS * PIN_FLAG_FRAME_WIDTH)
                  << 5,
              0, 1 << 10, 1 << 10);
          gDPPipeSync(g_DisplayListHead++);
        }
      }
    }
  }

  gDPPipeSync(g_DisplayListHead++);
  gDPSetDepthSource(g_DisplayListHead++, G_ZS_PIXEL);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
  gDPPipeSync(g_DisplayListHead++);
  if (g_IsModuleAlive[MODULE_PLAY] || ReportModuleNotAlive(MODULE_PLAY)) {
    DrawShotPreview(matrices);
  }
}
