/*
 * The golf balls on the course, one for each player, and the fading
 * afterimages that the ball in play leaves behind it.
 */
#include "common.h"

#include "golf_balls.h"

#include "asset.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "cpu_projection.h"
#include "debug_text.h"
#include "functions.h"
#include "game.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_loader.h"
#include "main.h"
#include "overlay_manager.h"
#include "pin.h"
#include "save.h"
#include "surface.h"
#include "terrain.h"
#include "variables.h"

#define GOLF_BALL_COUNT 32
#define AFTERIMAGE_MAX_LENGTH 60
#define AFTERIMAGE_FAST_LENGTH 10

/* A ball's alpha while it is hidden */
#define BALL_HIDDEN -1
#define BALL_SHOWN 0xFF
/* A ball's markerPlayerIndex while no number is shown over it */
#define BALL_NO_MARKER -1
/* An afterimage's alpha while it is unused */
#define AFTERIMAGE_UNUSED -1
#define AFTERIMAGE_ALPHA 0x80
#define AFTERIMAGE_FAST_ALPHA 0xC0
#define AFTERIMAGE_FAST_FADE 0x10
/* Slow afterimages fade by one on every this many frames */
#define AFTERIMAGE_SLOW_FADE_PERIOD 3

/* The heap that the ball images are loaded into, and its size */
#define HEAP_BALL_IMAGES 2
#define BALL_IMAGE_HEAP_SIZE 0x1800

/* The texture that "PIKA" is written into for Pikachu's ball */
#define PIKA_TEXTURE_WIDTH 32
#define PIKA_TEXTURE_HEIGHT 8

/* The ball turns this many degrees as it rolls one course unit */
#define BALL_ROLL_DEGREES_PER_UNIT 0.09325484f
#define BALL_MODEL_SCALE 0.05f
/* The ball's radius in course units, at a pin scale of one */
#define BALL_RADIUS 0x600

/* The game states that play a single ball, rather than one for each player */
#define GAME_STATE_UNK6 6
#define GAME_STATE_UNK11 11

typedef struct GolfBall {
  /* 0x00 */ Mtx matrices[DRAW_BUFFER_COUNT];
  /* 0x80 */ SurfaceInfo* surface;
  /* 0x84 */ f32 position[3];
  /* 0x90 */ s32 alpha; /* Or BALL_HIDDEN */
  /* 0x94 */ u8 isInCup;
  /* 0x98 */ s32 state; /* A BALL_STATE_* value */
  /* 0x9C */ s8 markerPlayerIndex; /* Or BALL_NO_MARKER */
  /* 0x9D */ s8 purpose; /* A LAUNCH_PURPOSE_* value */
  /* 0x9E */ s8 unk9E;
} GolfBall; /* size = 0xA0 */

typedef struct BallAfterimage {
  /* 0x00 */ f32 position[3];
  /* 0x0C */ s32 alpha; /* Or AFTERIMAGE_UNUSED */
  /* 0x10 */ s32 isInCup;
} BallAfterimage; /* size = 0x14 */

extern GolfBall f_GolfBalls[GOLF_BALL_COUNT];
extern BallAfterimage f_BallAfterimages[AFTERIMAGE_MAX_LENGTH];
extern f32 f_BallAfterimageOrigin[3];
/* The point that the next one is written over */
extern s32 f_BallAfterimageHead;
extern s32 f_IsBallAfterimageShown;
extern s32 f_BallAfterimageFrameCount;
/* Whether the points are scaled for the close view of the cup */
extern s32 f_IsBallAfterimageZoomed;
/* Set for the few afterimages that fade quickly */
extern s32 f_IsBallAfterimageFast;
extern s32 f_BallAfterimageLength;

void func_801FD5F0_ovl_00(void) {}

/* Where the ball of the player whose turn it is was last drawn */
static s32 f_CurrentBallScreenX;
static s32 f_CurrentBallScreenY;
static s32 f_CurrentBallDepth;
static u8* f_BallShadowImage;
static u8* f_RollingBallImage;
static u8* f_BallSparkleImage;
static u8* f_BallMarkerImage;
static u8* f_BallAfterimageImage;

/* Read by no routine */
static u8 D_80225830_ovl_00[16];

static u8 f_BallImageHeap[BALL_IMAGE_HEAP_SIZE];
/* "PIKA" in 4-bit texels, four to a u16 */
static u16 f_PikaBallTexture[PIKA_TEXTURE_WIDTH * PIKA_TEXTURE_HEIGHT / 4];
/* How the ball has turned as it rolled */
static Mat4 f_BallRotation;
static s32 f_GolfBallUnk100;
/* The ball's position when last rolled */
static s32 f_LastRolledBallPosition[3];

void DrawPikaBallTexture(void) {
  DrawTextIntoTexture("PIKA", 4, f_PikaBallTexture);
}

void LoadGolfBallImages(void) {
  EncodedAsset asset;

  ResetHeap(HEAP_BALL_IMAGES);
  AddHeapRegion(HEAP_BALL_IMAGES, f_BallImageHeap,
      f_BallImageHeap + BALL_IMAGE_HEAP_SIZE);
  f_BallShadowImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C7, &asset));
  ReadEncodedAsset(f_BallShadowImage, &asset);
  f_RollingBallImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C8, &asset));
  ReadEncodedAsset(f_RollingBallImage, &asset);
  f_BallSparkleImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4DA, &asset));
  ReadEncodedAsset(f_BallSparkleImage, &asset);
  f_BallMarkerImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C6, &asset));
  ReadEncodedAsset(f_BallMarkerImage, &asset);
  f_BallAfterimageImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x6BB, &asset));
  ReadEncodedAsset(f_BallAfterimageImage, &asset);
  osSyncPrintf("HEAP_BALLTEX LEFT%d\n", GetHeapFreeSize(HEAP_BALL_IMAGES));
  DrawPikaBallTexture();
  ResetGolfBalls();
  ResetBallRotation();
}

/* Turns the ball about the horizontal axis across the way that it moved */
void RollGolfBall(GolfBall* ball, BallSimulation* simulation) {
  Mat4 rotation;
  s32 movement[3];
  f32 distance;

  f32 GetLength(f32 x, f32 y, f32 z) {
    return sqrtf(x * x + y * y + z * z);
  }

  if (simulation->isRunning == BALL_STATE_FLYING ||
      simulation->isRunning == BALL_STATE_ROLLING) {
    movement[0] = simulation->position[0] - f_LastRolledBallPosition[0];
    movement[1] = simulation->position[1] - f_LastRolledBallPosition[1];
    movement[2] = simulation->position[2] - f_LastRolledBallPosition[2];
    f_LastRolledBallPosition[0] = simulation->position[0];
    f_LastRolledBallPosition[1] = simulation->position[1];
    f_LastRolledBallPosition[2] = simulation->position[2];
  } else {
    movement[0] = simulation->velocity[0];
    movement[1] = simulation->velocity[1];
    movement[2] = simulation->velocity[2];
  }

  if (simulation->isRunning != BALL_STATE_STOPPED) {
    distance = GetLength(movement[0], movement[1], movement[2]);
    if (movement[2] != 0 || movement[0] != 0) {
      guRotateF(rotation, distance * BALL_ROLL_DEGREES_PER_UNIT, -movement[2],
          0.0f, movement[0]);
      guMtxCatF(f_BallRotation, rotation, f_BallRotation);
    }
  }
}

void ResetBallAfterimage(void) {
  s32 i;

  for (i = 0; i != AFTERIMAGE_MAX_LENGTH; i++) {
    f_BallAfterimages[i].alpha = AFTERIMAGE_UNUSED;
    f_BallAfterimages[i].isInCup = FALSE;
  }

  f_BallAfterimages[0].position[0] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[0];
  f_BallAfterimages[0].position[1] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[1];
  f_BallAfterimages[0].position[2] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[2];
  f_BallAfterimageOrigin[0] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[0];
  f_BallAfterimageOrigin[1] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[1];
  f_BallAfterimageHead = 0;
  f_IsBallAfterimageShown = FALSE;
  f_BallAfterimageFrameCount = 0;
  f_IsBallAfterimageZoomed = FALSE;
  f_BallAfterimageOrigin[2] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[2];
}

void StartFastBallAfterimage(void) {
  f_IsBallAfterimageShown = TRUE;
  f_IsBallAfterimageFast = TRUE;
  f_BallAfterimageLength = AFTERIMAGE_FAST_LENGTH;
}

void ResetBallRotation(void) {
  guMtxIdentF(f_BallRotation);
}

void ResetGolfBalls(void) {
  s32 i;

  for (i = 0; i < GOLF_BALL_COUNT; i++) {
    f_GolfBalls[i].alpha = BALL_HIDDEN;
    f_GolfBalls[i].state = BALL_STATE_STOPPED;
  }

  ResetBallAfterimage();
  ResetBallRotation();
  f_GolfBallUnk100 = 0;
}

#ifdef NON_MATCHING
void UpdateGolfBalls(void) {
  s32 currentIndex;
  s32 i;
  UnkB8* player;
  GolfBall* ball;
  BallSimulation* simulation;
  BallAfterimage* head;
  BallAfterimage* afterimage;
  BallAfterimage* previous;
  Mat4 scaling;
  Mat4 rotated;
  Mat4 translation;
  f32 scale;
  f32 distance;
  s8 markerPlayerIndex;

  currentIndex = 0;
  if (g_GameState.unk0 == GAME_STATE_UNK6) {
    currentIndex = g_GameState.currentPlayerIndex;
  }

  for (i = 0, player = g_Players, ball = f_GolfBalls;
      i < g_GameState.playerCount; i++, player++, ball++) {
    if (player->unk26 != 0 || player->strokeCount == 0) {
      ball->alpha = BALL_HIDDEN;
    }

    if (currentIndex != i) {
      ball->state = BALL_STATE_STOPPED;
      if (GetSurfaceInfo((s8)player->ballSurface)->kind == SURFACE_KIND_GREEN) {
        ball->markerPlayerIndex = i;
      } else if (g_GameState.isMiniGolf) {
        ball->markerPlayerIndex = i;
      } else {
        ball->markerPlayerIndex = BALL_NO_MARKER;
      }
    }
  }

  if (g_GameState.unk0 == GAME_STATE_UNK6 ||
      g_GameState.unk0 == GAME_STATE_UNK11) {
    ball = &f_GolfBalls[currentIndex];
    for (i = 0; i < BALL_SIMULATION_COUNT; i++) {
      simulation = &g_BallSimulations[i];
      ball->isInCup = FALSE;
      if (simulation->playerIndex >= 0 && g_Players[currentIndex].unk26 == 0) {
        if (g_IsCupViewShown == TRUE) {
          ball->position[0] =
              (simulation->position[0] -
                  (f32)(D_800BA9D8.cupPosition[0] - simulation->position[0])) *
              COURSE_TO_WORLD_SCALE;
          ball->position[1] =
              (simulation->position[1] -
                  (f32)(D_800BA9D8.cupPosition[1] - simulation->position[1])) *
              COURSE_TO_WORLD_SCALE;
          ball->position[2] =
              (simulation->position[2] -
                  (f32)(D_800BA9D8.cupPosition[2] - simulation->position[2])) *
              COURSE_TO_WORLD_SCALE;
        } else {
          ball->position[0] = simulation->position[0] * COURSE_TO_WORLD_SCALE;
          ball->position[1] = simulation->position[1] * COURSE_TO_WORLD_SCALE;
          ball->position[2] = simulation->position[2] * COURSE_TO_WORLD_SCALE;
        }

        ball->alpha = BALL_SHOWN;
        ball->surface = GetSurfaceInfo(simulation->surface);
        if (simulation->isRunning != BALL_STATE_IN_CUP) {
          ball->isInCup = FALSE;
        } else {
          ball->isInCup = TRUE;
        }

        if (GetCupDistance(simulation->position[0] - D_800BA9D8.cupPosition[0],
                simulation->position[2] - D_800BA9D8.cupPosition[2]) == 0 ||
            simulation->isRunning == BALL_STATE_IN_CUP ||
            simulation->unkA0 != 0) {
          ball->isInCup = TRUE;
        } else {
          ball->isInCup = FALSE;
        }

        RollGolfBall(ball, simulation);
        ball->state = simulation->isRunning;
        ball->unk9E = simulation->unkA1;
        ball->purpose = simulation->purpose;
        markerPlayerIndex = BALL_NO_MARKER;
        if (currentIndex != simulation->playerIndex &&
            (GetSurfaceInfo(simulation->surface)->kind == SURFACE_KIND_GREEN ||
                g_GameState.isMiniGolf)) {
          markerPlayerIndex = simulation->playerIndex;
        }

        ball->markerPlayerIndex = markerPlayerIndex;
        scale = g_PinVertexScale * BALL_MODEL_SCALE;
        guScaleF(scaling, scale, scale, scale);
        guMtxCatF(f_BallRotation, scaling, rotated);
        guTranslateF(translation, ball->position[0],
            ball->position[1] - ((g_PinVertexScale * BALL_RADIUS) / 5120.0),
            ball->position[2]);
        MultiplyMatrices(rotated, translation, scaling);
        ConvertMatrixToFixed(scaling, &ball->matrices[g_DrawBufferIndex]);
      }
    }
  }

  if (f_IsBallAfterimageShown == TRUE) {
    if (g_IsCupViewShown == TRUE) {
      if (!f_IsBallAfterimageZoomed) {
        f_IsBallAfterimageZoomed = TRUE;
        for (i = 0, afterimage = f_BallAfterimages; i != f_BallAfterimageLength;
            i++, afterimage++) {
          if (afterimage->alpha != AFTERIMAGE_UNUSED) {
            s32 coursePosition[3];

            coursePosition[0] = afterimage->position[0] * 1024.0f;
            coursePosition[1] = afterimage->position[1] * 1024.0f;
            coursePosition[2] = afterimage->position[2] * 1024.0f;
            afterimage->position[0] += afterimage->position[0] -
                (D_800BA9D8.cupPosition[0] * COURSE_TO_WORLD_SCALE);
            afterimage->position[1] += afterimage->position[1] -
                (D_800BA9D8.cupPosition[1] * COURSE_TO_WORLD_SCALE);
            afterimage->position[2] += afterimage->position[2] -
                (D_800BA9D8.cupPosition[2] * COURSE_TO_WORLD_SCALE);
          }
        }
      }
    } else if (f_IsBallAfterimageZoomed == TRUE) {
      f_IsBallAfterimageZoomed = FALSE;
      for (i = 0, afterimage = f_BallAfterimages; i != f_BallAfterimageLength;
          i++, afterimage++) {
        if (afterimage->alpha != AFTERIMAGE_UNUSED) {
          afterimage->position[0] -=
              (afterimage->position[0] -
                  (D_800BA9D8.cupPosition[0] * COURSE_TO_WORLD_SCALE)) *
              0.5f;
          afterimage->position[1] -=
              (afterimage->position[1] -
                  (D_800BA9D8.cupPosition[1] * COURSE_TO_WORLD_SCALE)) *
              0.5f;
          afterimage->position[2] -=
              (afterimage->position[2] -
                  (D_800BA9D8.cupPosition[2] * COURSE_TO_WORLD_SCALE)) *
              0.5f;
        }
      }
    }

    head = &f_BallAfterimages[f_BallAfterimageHead];
    ball = &f_GolfBalls[currentIndex];
    if (f_BallAfterimageHead == 0) {
      previous = &f_BallAfterimages[f_BallAfterimageLength - 1];
    } else {
      previous = &f_BallAfterimages[f_BallAfterimageHead - 1];
    }
    {
      f32 offset[3];

      offset[0] = ball->position[0] - previous->position[0];
      offset[1] = ball->position[1] - previous->position[1];
      offset[2] = ball->position[2] - previous->position[2];
      distance = sqrtf((offset[0] * offset[0]) + (offset[1] * offset[1]) +
          (offset[2] * offset[2]));
    }

    if (f_IsBallAfterimageFast == TRUE) {
      if ((f32)(g_PinVertexScale * BALL_RADIUS) * 0.00009765625f < distance &&
          g_BallSimulations[0].isRunning == BALL_STATE_ROLLING) {
        head->alpha = AFTERIMAGE_FAST_ALPHA;
        head->position[0] = ball->position[0];
        head->position[1] = ball->position[1];
        head->position[2] = ball->position[2];
        if (ball->isInCup != TRUE) {
          head->isInCup = FALSE;
        } else {
          head->isInCup = TRUE;
        }

        f_BallAfterimageHead++;
        if (f_BallAfterimageHead == f_BallAfterimageLength) {
          f_BallAfterimageHead = 0;
        }
      }

      for (i = 0, afterimage = f_BallAfterimages; i != f_BallAfterimageLength;
          i++, afterimage++) {
        if (afterimage->alpha > 0) {
          afterimage->alpha -= AFTERIMAGE_FAST_FADE;
        }
      }
    } else {
      if ((f32)(g_PinVertexScale * BALL_RADIUS) * 0.0005859375f < distance) {
        head->alpha = AFTERIMAGE_ALPHA;
        head->position[0] = ball->position[0];
        head->position[1] = ball->position[1];
        head->position[2] = ball->position[2];
        if (ball->isInCup != TRUE) {
          head->isInCup = FALSE;
        } else {
          head->isInCup = ball->isInCup;
        }

        f_BallAfterimageHead++;
        if (f_BallAfterimageHead == f_BallAfterimageLength) {
          f_BallAfterimageHead = 0;
        }
      }

      if (f_BallAfterimageFrameCount % AFTERIMAGE_SLOW_FADE_PERIOD == 0) {
        for (i = 0, afterimage = f_BallAfterimages; i != f_BallAfterimageLength;
            i++, afterimage++) {
          if (afterimage->alpha > 0) {
            afterimage->alpha--;
          }
        }
      }
    }

    f_BallAfterimageFrameCount++;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/golf_balls", UpdateGolfBalls);
#endif

s32 IsAnyBallInCup(void) {
  s32 isInCup = FALSE;
  s32 i;

  for (i = 0; i != GOLF_BALL_COUNT; i++) {
    if (f_GolfBalls[i].isInCup == TRUE && f_GolfBalls[i].alpha != BALL_HIDDEN) {
      isInCup = TRUE;
    }
  }

  return isInCup;
}

/* The rolling ball turns through this many frames, each shown for four draws */
#define BALL_ROLL_FRAME_COUNT 16
#define BALL_ROLL_FRAME_SIZE 0x400
/* The tiles that the ball images are loaded into */
#define BALL_TILE_ROLLING G_TX_RENDERTILE
#define BALL_TILE_STILL 1
#define BALL_TILE_SHADOW 2
#define BALL_TILE_MARKER 3
#define BALL_TILE_SMALL 4
#define BALL_TILE_AFTERIMAGE 5
#define BALL_TEXTURE_SIZE 32
#define SMALL_BALL_TEXTURE_SIZE 8

/* How a ball sits in its surface, from SurfaceInfo.unk14 */
#define BALL_BURIED -1
#define BALL_HALF_SUNK 0
#define BALL_EXPOSED 1

/* How each ball was drawn, which decides whether it gets a shadow */
#define BALL_SPRITE_FULL 0
#define BALL_SPRITE_SMALL 1
#define BALL_SPRITE_DOT 2
#define BALL_SPRITE_FAR_DOT 3
#define BALL_SPRITE_DOT_SHADOWED 4
#define BALL_SPRITE_FAR_DOT_SHADOWED 5

/* Balls nearer than this are not drawn */
#define BALL_NEAREST_DEPTH (-5)
#define BALL_FULL_SPRITE_DEPTH (-86)
#define BALL_NEAR_DEPTH (-200)
#define BALL_DOT_DEPTH (-40000)
#define BALL_DEPTH_BIAS 60.0f
/* The pika balls are drawn as models only while the current ball is nearer than
 * this */
#define PIKA_BALL_DEPTH 20000

extern Vtx f_PikaBallVertices[];
/* f_BallRollFrame cycles through BALL_ROLL_FRAME_COUNT frames, this through
 * nine unused ones */
extern s32 f_UnusedBallFrame;
extern s32 f_BallRollFrame;

/* Like G_RM_ZB_XLU_SURF and G_RM_AA_ZB_XLU_SURF, but also updating the depth
 * buffer */
#define RM_ZB_XLU_SURF_UPDATE(clk) \
  (Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL | \
      GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))
#define RM_AA_ZB_XLU_SURF_UPDATE(clk) \
  (AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | \
      FORCE_BL | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))

/* The texture coordinate step that stretches texelCount texels from from to to,
 * in quarter pixels */
#define TEXEL_STEP(texelCount, from, to) \
  ((to) - (from) != 0 ? ((((texelCount) << 13) / ((to) - (from))) + 1) >> 1 : 0)
#define gSPStretchedTextureRectangle( \
    pkt, xl, yl, xh, yh, tile, s, t, width, height) \
  gSPScisTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, \
      TEXEL_STEP(width, xl, xh), TEXEL_STEP(height, yl, yh))

#define ABSF(value) \
  ({ \
    f32 absValue = (value); \
    (absValue < 0.0f) ? -absValue : absValue; \
  })

#ifdef NON_MATCHING
void DrawGolfBalls(CameraMatrices* matrices) {
  Gfx* gfx = g_DisplayListHead;
  ScreenPoint ballPoint;
  ScreenPoint shadowPoint;
  s32 i;
  s32 isCurrentBallAboveGround = FALSE;
  GolfBall* ball;
  BallAfterimage* afterimage;
  SurfaceInfo* surface;
  s32 groundHeight;
  s32 spriteKind;
  s32 isAboveGround;
  s32 exposure;
  s32 isAboveCup;
  s32 ballDepth;
  s32 shadowDepth;
  s32 primDepth;
  s32 shadowPrimDepth;
  s32 size;
  s32 shade;
  s32 halfWidth;
  s32 halfHeight;
  u32 afterimageDepth;

  if (g_GameState.unk0 != GAME_STATE_UNK6 &&
      g_GameState.unk0 != GAME_STATE_UNK11) {
    return;
  }
  {
    g_ModuleRdpRequestFrames[MODULE_UNK1] = g_FrameCount;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, RM_ZB_XLU_SURF_UPDATE(1), RM_ZB_XLU_SURF_UPDATE(2));
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x01);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    f_BallRollFrame = (f_BallRollFrame + 1) & (BALL_ROLL_FRAME_COUNT - 1);
    f_UnusedBallFrame = (f_UnusedBallFrame + 1) % 9;
    gDPLoadTextureBlock(gfx++,
        TEXTURE_ADDRESS(f_RollingBallImage +
            ((f_BallRollFrame / 4 * BALL_ROLL_FRAME_SIZE) + IMAGE_HEADER_SIZE)),
        G_IM_FMT_I, G_IM_SIZ_8b, BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE, 0,
        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 6, 6,
        G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(gfx++,
        TEXTURE_ADDRESS(f_RollingBallImage + IMAGE_HEADER_SIZE), 0x80,
        BALL_TILE_STILL, G_IM_FMT_I, G_IM_SIZ_8b, BALL_TEXTURE_SIZE,
        BALL_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock_4b(gfx++,
        TEXTURE_ADDRESS(f_BallSparkleImage + IMAGE_HEADER_SIZE), 0x100,
        BALL_TILE_SHADOW, G_IM_FMT_I, 16, 16, 0, G_TX_MIRROR | G_TX_WRAP,
        G_TX_MIRROR | G_TX_WRAP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock_4b(gfx++,
        TEXTURE_ADDRESS(f_BallMarkerImage + IMAGE_HEADER_SIZE), 0x140,
        BALL_TILE_MARKER, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock_4b(gfx++,
        TEXTURE_ADDRESS(f_BallShadowImage + IMAGE_HEADER_SIZE), 0x150,
        BALL_TILE_SMALL, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock_4b(gfx++,
        TEXTURE_ADDRESS(f_BallAfterimageImage + IMAGE_HEADER_SIZE), 0x190,
        BALL_TILE_AFTERIMAGE, G_IM_FMT_I, 16, 64, 0, G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
    gDPPipeSync(gfx++);
    for (i = 0; i < GOLF_BALL_COUNT; i++) {
      ball = &f_GolfBalls[i];
      if (ball->alpha > 0) {
        surface = ball->surface;
        groundHeight =
            GetGroundHeight(ball->position[0] * WORLD_TO_COURSE_SCALE,
                ball->position[2] * WORLD_TO_COURSE_SCALE);
        spriteKind = BALL_SPRITE_FULL;
        isAboveGround = TRUE;
        if (ball->position[1] * WORLD_TO_COURSE_SCALE < groundHeight - 153.6f) {
          isAboveGround = FALSE;
        }

        if (isAboveGround) {
          if (surface->unk14 == 2) {
            exposure = BALL_BURIED;
          } else if (surface->unk14 == 1) {
            exposure = BALL_HALF_SUNK;
          } else {
            exposure = BALL_EXPOSED;
          }
        } else {
          exposure = BALL_EXPOSED;
        }

        isAboveCup = FALSE;
        if (groundHeight + 768.0f < ball->position[1] * WORLD_TO_COURSE_SCALE) {
          isAboveCup = ball->isInCup == TRUE;
        }

        ballDepth = ProjectPoint(ball->position[0],
            ball->position[1] - (g_PinVertexScale * 0.3f), ball->position[2],
            &ballPoint);
        shadowDepth = ProjectPoint(ball->position[0],
            (groundHeight * COURSE_TO_WORLD_SCALE) - (g_PinVertexScale * 0.06f),
            ball->position[2], &shadowPoint);
        if (ball->markerPlayerIndex == BALL_NO_MARKER) {
          if (ballDepth < BALL_NEAREST_DEPTH && exposure >= BALL_HALF_SUNK) {
            gDPPipeSync(gfx++);
            if (ball->unk9E) {
              groundHeight = GetGroundOrWaterHeight(
                  ball->position[0] * WORLD_TO_COURSE_SCALE,
                  ball->position[2] * WORLD_TO_COURSE_SCALE);
              gDPPipeSync(gfx++);
              gDPSetRenderMode(gfx++, RM_AA_ZB_XLU_SURF_UPDATE(1),
                  RM_AA_ZB_XLU_SURF_UPDATE(2));
              shade = 100 -
                  (s32)(((ball->position[1] * WORLD_TO_COURSE_SCALE) -
                            groundHeight) *
                      0.0032552085f);
              if (shade < 0) {
                shade = 0;
              }

              gDPSetPrimColor(gfx++, 0, 0, 0xEF, 0xEF, 0xFF, shade);
              gDPPipeSync(gfx++);
              gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
            } else {
              gDPPipeSync(gfx++);
              gDPSetRenderMode(
                  gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
              gDPSetPrimColor(gfx++, 0, 0, 0xEF, 0xEF, 0xFF, 0xFF);
            }

            if (g_IsCupViewShown == TRUE) {
              if (ballDepth < g_CupViewDepth) {
                primDepth = 0x402F;
              } else {
                primDepth = isAboveCup ? 0x401F : 0x4007;
              }
            } else if (ball->purpose == LAUNCH_PURPOSE_UNK3) {
              primDepth = ballPoint.depth;
            } else if (ballDepth < g_CupViewDepth) {
              if (isAboveCup == TRUE) {
                continue;
              }

              if (ballDepth < BALL_NEAR_DEPTH) {
                primDepth = ballPoint.depth;
              } else {
                primDepth = ballPoint.depth -
                    (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 48.0f -
                    BALL_DEPTH_BIAS;
              }
            } else {
              if (isAboveCup == TRUE) {
                continue;
              }

              if (ballDepth < BALL_NEAR_DEPTH) {
                primDepth = ballPoint.depth;
              } else {
                primDepth = ballPoint.depth -
                    (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 8.0f -
                    BALL_DEPTH_BIAS;
              }
            }

            primDepth &= ~3;
            if (g_GameState.currentPlayerIndex == i) {
              isCurrentBallAboveGround = isAboveGround;
              f_CurrentBallDepth = primDepth;
              f_CurrentBallScreenX = ballPoint.x;
              f_CurrentBallScreenY = ballPoint.y;
            }

            if (primDepth < 0) {
              primDepth = 0;
            }

            gDPSetPrimDepth(gfx++, primDepth, 0);
            gDPPipeSync(gfx++);
            size = (s32)(ABSF(ballPoint.scaleX) *
                (0.6f * (2.0f * g_PinVertexScale)) * 0.25f);
            if (size <= 0) {
              size = 1;
            }

            if (g_IsCupViewShown == TRUE ||
                ballDepth >= BALL_FULL_SPRITE_DEPTH) {
              spriteKind = BALL_SPRITE_FULL;
              if (ball->state == BALL_STATE_STOPPED) {
                gSPStretchedTextureRectangle(gfx++, (ballPoint.x - size) << 2,
                    (ballPoint.y - size) << 2, (ballPoint.x + size) << 2,
                    (ballPoint.y + (exposure * size)) << 2, BALL_TILE_STILL, 0,
                    0, BALL_TEXTURE_SIZE,
                    (exposure + 1) * (BALL_TEXTURE_SIZE / 2));
              } else {
                gSPStretchedTextureRectangle(gfx++, (ballPoint.x - size) << 2,
                    (ballPoint.y - size) << 2, (ballPoint.x + size) << 2,
                    (ballPoint.y + (exposure * size)) << 2, BALL_TILE_ROLLING,
                    0, 0, BALL_TEXTURE_SIZE,
                    (exposure + 1) * (BALL_TEXTURE_SIZE / 2));
              }
            } else if (ballDepth >= BALL_FULL_SPRITE_DEPTH) {
              gSPStretchedTextureRectangle(gfx++, (ballPoint.x - size) << 2,
                  (ballPoint.y - size) << 2, (ballPoint.x + size) << 2,
                  (ballPoint.y + (exposure * size)) << 2, BALL_TILE_SMALL, 0, 0,
                  SMALL_BALL_TEXTURE_SIZE,
                  (exposure + 1) * (SMALL_BALL_TEXTURE_SIZE / 2));
              spriteKind = BALL_SPRITE_SMALL;
            } else if (ballDepth > BALL_DOT_DEPTH) {
              gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
              gDPPipeSync(gfx++);
              gSPStretchedTextureRectangle(gfx++, (ballPoint.x - 1) << 2,
                  (ballPoint.y - 1) << 2, (ballPoint.x + 1) << 2,
                  ballPoint.y << 2, BALL_TILE_SMALL, 0, 0,
                  SMALL_BALL_TEXTURE_SIZE, SMALL_BALL_TEXTURE_SIZE);
              gDPPipeSync(gfx++);
              gDPSetPrimColor(gfx++, 0, 0, 0xBF, 0xBF, 0xBF, 0xFF);
              gDPPipeSync(gfx++);
              gDPPipeSync(gfx++);
              gDPPipeSync(gfx++);
              gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
              gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0x3F);
              gDPPipeSync(gfx++);
              if (isAboveGround == TRUE) {
                spriteKind = BALL_SPRITE_DOT_SHADOWED;
                gSPStretchedTextureRectangle(gfx++, (ballPoint.x - 1) << 2,
                    (ballPoint.y + 1) << 2, (ballPoint.x + 1) << 2,
                    (ballPoint.y + 2) << 2, BALL_TILE_SMALL, 0, 0,
                    SMALL_BALL_TEXTURE_SIZE, SMALL_BALL_TEXTURE_SIZE);
              } else {
                spriteKind = BALL_SPRITE_DOT;
              }
            } else {
              gDPPipeSync(gfx++);
              gDPSetRenderMode(
                  gfx++, RM_ZB_XLU_SURF_UPDATE(1), RM_ZB_XLU_SURF_UPDATE(2));
              gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
              gDPPipeSync(gfx++);
              gSPScisTextureRectangle(gfx++, (ballPoint.x << 2) - 4,
                  (ballPoint.y << 2) - 8, ballPoint.x << 2, ballPoint.y << 2,
                  BALL_TILE_SMALL, 0, 0,
                  TEXEL_STEP(SMALL_BALL_TEXTURE_SIZE, (ballPoint.x << 2) - 4,
                      ballPoint.x << 2),
                  TEXEL_STEP(SMALL_BALL_TEXTURE_SIZE, (ballPoint.y - 1) << 2,
                      (ballPoint.y + 1) << 2));
              gDPPipeSync(gfx++);
              gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0xFF);
              gDPPipeSync(gfx++);
              spriteKind = BALL_SPRITE_FAR_DOT;
              if (isAboveGround == TRUE) {
                spriteKind = BALL_SPRITE_FAR_DOT_SHADOWED;
              }
            }

            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
            gDPSetCombineMode(
                gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          }

          if (exposure == BALL_EXPOSED && shadowDepth < BALL_NEAREST_DEPTH &&
              !isAboveCup && !ball->unk9E) {
            size = (s32)(ABSF(shadowPoint.scaleX) *
                (0.6f * (2.0f * g_PinVertexScale)) * 0.25f);
            if (g_IsCupViewShown == TRUE) {
              if (shadowDepth < g_CupViewDepth) {
                shadowPrimDepth = 0x4033;
              } else if (ball->isInCup == TRUE) {
                shadowPrimDepth = 0x4023;
              } else {
                shadowPrimDepth = 0x400B;
              }

              shade = 0x7F -
                  ((s32)(groundHeight -
                       (ball->position[1] * WORLD_TO_COURSE_SCALE)) /
                      128);
              if (shade < 0x3F) {
                shade = 0x3F;
              }
            } else {
              if (ball->purpose == LAUNCH_PURPOSE_UNK3) {
                shadowPrimDepth = ballPoint.depth;
              } else {
                f32 depth;

                if (shadowDepth < g_CupViewDepth) {
                  depth = shadowPoint.depth -
                      (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) +
                      56.0f;
                } else if (ball->isInCup == TRUE) {
                  depth = shadowPoint.depth -
                      (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) +
                      40.0f;
                } else {
                  depth = shadowPoint.depth -
                      (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) +
                      16.0f;
                }

                shadowPrimDepth = depth - BALL_DEPTH_BIAS;
              }

              shade = 0x7F -
                  ((s32)(groundHeight -
                       (ball->position[1] * WORLD_TO_COURSE_SCALE)) /
                      512);
              if (shade < 0x5F) {
                shade = 0x5F;
              }
            }

            if (shade > 0x7F) {
              shade = 0x7F;
            }

            size = size * (shade * 0.007874016f);
            if (size <= 0) {
              size = 1;
            }

            shadowPrimDepth &= ~3;
            if (shadowPrimDepth < 0) {
              shadowPrimDepth = 0;
            }

            if (shadowPrimDepth < primDepth + 8) {
              shadowPrimDepth = primDepth + 8;
            }

            gDPSetPrimDepth(gfx++, shadowPrimDepth, 0);
            gDPPipeSync(gfx++);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, shade);
            if (spriteKind < BALL_SPRITE_DOT_SHADOWED) {
              gDPPipeSync(gfx++);
              gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
              gDPPipeSync(gfx++);
              gSPStretchedTextureRectangle(gfx++, (shadowPoint.x - size) << 2,
                  (shadowPoint.y - size) << 2, (shadowPoint.x + size) << 2,
                  (shadowPoint.y + size) << 2, BALL_TILE_SHADOW, 0, 0,
                  BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE);
            }
          }
        } else if (shadowDepth < BALL_NEAREST_DEPTH) {
          f32 depth;

          size = ABSF(shadowPoint.scaleX) * (0.6f * (2.0f * g_PinVertexScale)) *
              0.25f;
          if (g_IsCupViewShown != TRUE) {
            if (shadowDepth < g_CupViewDepth) {
              depth = shadowPoint.depth -
                  (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 56.0f;
            } else if (ball->isInCup == TRUE) {
              depth = shadowPoint.depth -
                  (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 40.0f;
            } else {
              depth = shadowPoint.depth -
                  (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 16.0f;
            }

            shadowPrimDepth = depth - BALL_DEPTH_BIAS;
            if (size <= 0) {
              size = 1;
            }

            gDPSetPrimDepth(gfx++, shadowPrimDepth & ~3, 0);
            gDPPipeSync(gfx++);
            switch (ball->markerPlayerIndex) {
              case -1:
                gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0);
                break;
              case 0:
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, 0xFF);
                break;
              case 1:
                gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0xFF, 0xFF);
                break;
              case 2:
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0xFF, 0xFF);
                break;
              case 3:
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xBF, 0x7F, 0xFF);
                break;
            }

            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
            gDPPipeSync(gfx++);
            gSPStretchedTextureRectangle(gfx++, (ballPoint.x - size) << 2,
                (ballPoint.y - size) << 2, (ballPoint.x + size) << 2,
                (ballPoint.y + size) << 2, BALL_TILE_MARKER, 0, 0, 16, 16);
          }
        }
      }
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
    gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, ENVIRONMENT, PRIMITIVE, 0,
        TEXEL0, 0, PRIMITIVE, 0, TEXEL0, ENVIRONMENT, PRIMITIVE, 0, TEXEL0, 0);
    gDPSetEnvColor(gfx++, 0x00, 0x00, 0x80, 0x1E);
    for (i = 0; i != f_BallAfterimageLength; i++) {
      afterimage = &f_BallAfterimages[i];
      if (afterimage->alpha > 0) {
        ballDepth = ProjectPoint(afterimage->position[0],
            afterimage->position[1] - (g_PinVertexScale * 0.27f),
            afterimage->position[2], &ballPoint);
        if (ballDepth < BALL_NEAREST_DEPTH) {
          if (g_IsCupViewShown == TRUE) {
            afterimageDepth = 0x402F;
            if (ballDepth >= g_CupViewDepth) {
              afterimageDepth = afterimage->isInCup == TRUE ? 0x401F : 0x4007;
            }

            gDPSetPrimColor(
                gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
          } else {
            if (ballDepth < g_CupViewDepth) {
              afterimageDepth = ballPoint.depth -
                  (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 48.0f -
                  BALL_DEPTH_BIAS;
            } else if (afterimage->isInCup == FALSE) {
              afterimageDepth = ballPoint.depth -
                  (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 8.0f -
                  BALL_DEPTH_BIAS;
            } else {
              continue;
            }

            if (f_IsBallAfterimageFast == TRUE) {
              gDPSetPrimColor(
                  gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
            } else {
              gDPSetPrimColor(
                  gfx++, 0, 0, 0xFF, 0xFF, 0xFF, afterimage->alpha / 2);
            }
          }

          gDPSetPrimDepth(gfx++, afterimageDepth & ~3, 0);
          gDPPipeSync(gfx++);
          halfWidth =
              (ABSF(ballPoint.scaleX) *
                  ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
              1.0;
          halfHeight =
              (ABSF(ballPoint.scaleY) *
                  ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
              1.0;
          if (halfWidth < 4) {
            halfWidth = 4;
          }

          if (halfHeight < 4) {
            halfHeight = 4;
          }

          if (f_IsBallAfterimageFast == TRUE) {
            gSPStretchedTextureRectangle(gfx++,
                (ballPoint.x << 2) - halfWidth - 1,
                (ballPoint.y << 2) - halfHeight - 1,
                (ballPoint.x << 2) + halfWidth + 2,
                (ballPoint.y << 2) + halfHeight + 2, BALL_TILE_AFTERIMAGE, 0,
                (guRandom() % 3) << 9, 15, 15);
          } else {
            gSPStretchedTextureRectangle(gfx++,
                (ballPoint.x << 2) - halfWidth - 1,
                (ballPoint.y << 2) - halfHeight - 1,
                (ballPoint.x << 2) + halfWidth + 2,
                (ballPoint.y << 2) + halfHeight + 2, BALL_TILE_STILL, 0, 0,
                BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE);
          }

          gDPPipeSync(gfx++);
        }
      }
    }

    if (IsGameFlagSet(0x4C)) {
      for (i = 0; i != ROUTE_POINT_COUNT; i++) {
        afterimage = &f_BallAfterimages[i];
        shadowDepth =
            ProjectPoint(g_HolePoints.routePoints[i][0] * COURSE_TO_WORLD_SCALE,
                (g_HolePoints.routePoints[i][1] * COURSE_TO_WORLD_SCALE) -
                    (g_PinVertexScale * 0.27f),
                g_HolePoints.routePoints[i][2] * COURSE_TO_WORLD_SCALE,
                &shadowPoint);
        if (shadowDepth < BALL_NEAREST_DEPTH) {
          if (shadowDepth < g_CupViewDepth) {
            afterimageDepth = shadowPoint.depth -
                (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 48.0f -
                BALL_DEPTH_BIAS;
          } else if (afterimage->isInCup == FALSE) {
            afterimageDepth = shadowPoint.depth -
                (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 8.0f -
                BALL_DEPTH_BIAS;
          } else {
            continue;
          }

          if (f_IsBallAfterimageFast == TRUE) {
            gDPSetPrimColor(
                gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
          } else {
            gDPSetPrimColor(
                gfx++, 0, 0, 0xFF, 0xFF, 0xFF, afterimage->alpha / 2);
          }

          gDPSetPrimDepth(gfx++, afterimageDepth & ~3, 0);
          gDPPipeSync(gfx++);
          halfWidth =
              (ABSF(shadowPoint.scaleX) *
                  ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
              1.0;
          halfHeight =
              (ABSF(shadowPoint.scaleY) *
                  ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
              1.0;
          if (halfWidth < 4) {
            halfWidth = 4;
          }

          if (halfHeight < 4) {
            halfHeight = 4;
          }

          if (f_IsBallAfterimageFast == TRUE) {
            gSPStretchedTextureRectangle(gfx++,
                (shadowPoint.x << 2) - halfWidth - 1,
                (shadowPoint.y << 2) - halfHeight - 1,
                (shadowPoint.x << 2) + halfWidth + 2,
                (shadowPoint.y << 2) + halfHeight + 2, BALL_TILE_AFTERIMAGE, 0,
                (guRandom() % 3) << 9, 15, 15);
          } else {
            gSPStretchedTextureRectangle(gfx++,
                (shadowPoint.x << 2) - halfWidth - 1,
                (shadowPoint.y << 2) - halfHeight - 1,
                (shadowPoint.x << 2) + halfWidth + 2,
                (shadowPoint.y << 2) + halfHeight + 2, BALL_TILE_STILL, 0, 0,
                BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE);
          }

          gDPPipeSync(gfx++);
        }
      }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++, RM_AA_ZB_XLU_SURF_UPDATE(1), RM_AA_ZB_XLU_SURF_UPDATE(2));
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPSetCombineLERP(gfx++, 0, 0, 0, SHADE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
        SHADE, PRIMITIVE, 0, TEXEL0, 0);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x00, 0x60);
    gDPLoadTextureBlock_4b(gfx++, f_PikaBallTexture, G_IM_FMT_IA,
        PIKA_TEXTURE_WIDTH, PIKA_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPSetPrimDepth(gfx++, f_CurrentBallDepth - 4, 0);
    if (f_CurrentBallDepth < PIKA_BALL_DEPTH) {
      for (i = 0, ball = f_GolfBalls; i < GOLF_BALL_COUNT; i++, ball++) {
        if (ball->markerPlayerIndex == BALL_NO_MARKER &&
            f_GolfBalls[i].alpha == BALL_SHOWN &&
            (g_IsCupViewShown || ball->isInCup != TRUE) &&
            (!isCurrentBallAboveGround || ball->surface->unk14 == 0) &&
            ball->alpha > 0) {
          gDPPipeSync(gfx++);
          gSPMatrix(gfx++, &ball->matrices[g_DrawBufferIndex],
              G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
          gSPVertex(gfx++, f_PikaBallVertices, 16, 0);
          gSP2Triangles(gfx++, 0, 2, 1, 0, 1, 2, 3, 0);
          gSP2Triangles(gfx++, 4, 6, 5, 0, 5, 6, 7, 0);
          gSP2Triangles(gfx++, 8, 9, 10, 0, 9, 11, 10, 0);
          gSP2Triangles(gfx++, 12, 13, 14, 0, 13, 15, 14, 0);
          gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
          gDPPipeSync(gfx++);
        }
      }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/golf_balls", DrawGolfBalls);
#endif
