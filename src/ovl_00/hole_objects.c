/*
 * The creatures that appear on a hole: Cheep Cheeps that leap out of the
 * water where the ball lands, and a monkey that drops onto the cup.
 */
#include "common.h"

#include "hole_objects.h"

#include "asset.h"
#include "camera.h"
#include "cpu_projection.h"
#include "effects.h"
#include "game.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_display.h"
#include "libc.h"
#include "math_util.h"
#include "round_hud.h"
#include "save.h"
#include "screen_layers.h"
#include "sound.h"
#include "terrain.h"
#include "turn.h"

#define HOLE_CREATURE_COUNT 10

/* HoleCreature.state values */
enum HoleCreatureKind {
  HOLE_CREATURE_NONE,
  HOLE_CREATURE_CHEEP_CHEEP,
  HOLE_CREATURE_MONKEY
};

#define CHEEP_CHEEP_ASSET 0x6E0
/* A Cheep Cheep's images as it turns; CHEEP_CHEEP_ASSET is the middle one */
#define CHEEP_CHEEP_FIRST_VIEW_ASSET 0x6DE
#define CHEEP_CHEEP_VIEW_COUNT 5
#define GOLD_CHEEP_CHEEP_ASSET 0x6E5
/* The first of its frames */
#define MONKEY_ASSET 0x6F0
#define MONKEY_FRAME_COUNT 4

/* Course heights grow downwards, so these lift a creature by subtracting */
/* Below the ball where a Cheep Cheep starts and ends */
#define CHEEP_CHEEP_DEPTH 0x7800
/* Upwards */
#define CHEEP_CHEEP_LEAP_SPEED 9216.0f
/* Twice the course's gravity */
#define CHEEP_CHEEP_GRAVITY (GRAVITY * 2)
/* And the range of random speeds above it */
#define CHEEP_CHEEP_MIN_SPEED 0x600
#define CHEEP_CHEEP_SPEED_RANGE 0x600
#define CHEEP_CHEEP_PITCH_EASING 0.7f
/* Below the ball, over which a Cheep Cheep fades out */
#define CHEEP_CHEEP_FADE_DEPTH 46080
/* Above the cup */
#define MONKEY_DROP_HEIGHT 0x9600
/* The frames at which the monkey shows each of its later images */
#define MONKEY_SECOND_FRAME 16
#define MONKEY_THIRD_FRAME 20
#define MONKEY_LAST_FRAME 24
#define MONKEY_LIFETIME 46

#define HOLE_OBJECT_SOUND_CHANNEL 15
#define SPLASH_SOUND_VOLUME 0x5A

/* The monkey is drawn as two halves, each loaded as one texture */
#define MONKEY_HALF_IMAGE_SIZE 0x1000

typedef struct HoleCreature {
  /* 0x00 */ Mtx matrices[DRAW_BUFFER_COUNT];
  /* 0x80 */ CoursePoint position;
  /* 0x8C */ f32 velocity[AXIS_COUNT];
  /* 0x98 */ s32 groundHeight;
  /* 0x9C */ Radians yaw;
  /* 0xA0 */ Radians pitch;
  /* 0xA4 */ u8 frameCount;
  /* 0xA5 */ u8 isFacingBack;
  /* 0xA6 */ u8 isGold;
  /* 0xA7 */ u8 state;
} HoleCreature; /* size = 0xA8 */

/* Read by no routine */
static AssetId f_CheepCheepViewAssets[CHEEP_CHEEP_VIEW_COUNT] UNUSED = {
    CHEEP_CHEEP_FIRST_VIEW_ASSET, CHEEP_CHEEP_FIRST_VIEW_ASSET + 1,
    CHEEP_CHEEP_FIRST_VIEW_ASSET + 2, CHEEP_CHEEP_FIRST_VIEW_ASSET + 3,
    CHEEP_CHEEP_FIRST_VIEW_ASSET + 4};
static AssetId f_MonkeyFrameAssets[MONKEY_FRAME_COUNT] = {
    MONKEY_ASSET, MONKEY_ASSET + 1, MONKEY_ASSET + 2, MONKEY_ASSET + 3};
static Boolean f_AreCheepCheepsLeaping = FALSE;
static s32 f_CheepCheepLeapUnk38 = 0;
static s32 f_CheepCheepLeapUnk3C = 0;
static s32 f_CheepCheepLeapUnk40 = 0;
/* Read by no routine */
static s32 f_UnusedWordBeforeCheepCheepVertices UNUSED = 0;
static Vtx f_CheepCheepVertices[4] = {
    {{{-7, -7, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-7, 7, 0}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, 7, 0}, 0, {0x7C0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, -7, 0}, 0, {0x7C0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}}};
static Vtx f_MonkeyVertices[8] = {
    {{{-10, -12, 0}, 0, {0xFC0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-10, -6, 0}, 0, {0xFC0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, -6, 0}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, -12, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-10, -6, 0}, 0, {0xFC0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-10, 0, 0}, 0, {0xFC0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, 0}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, -6, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}}};

static u8* f_CheepCheepImages[2];
static u8* f_MonkeyImage;

/* Not static: the original left it to the linker's common block */
HoleCreature f_HoleCreatures[HOLE_CREATURE_COUNT];

void SpawnRipple(CoursePoint position);

void LoadHoleCreatureImages(void) {
  EncodedAsset asset;

  f_CheepCheepImages[0] =
      AllocMemory(OpenEncodedAsset(CHEEP_CHEEP_ASSET, &asset));
  ReadEncodedAsset(f_CheepCheepImages[0], &asset);
  f_CheepCheepImages[1] =
      AllocMemory(OpenEncodedAsset(GOLD_CHEEP_CHEEP_ASSET, &asset));
  ReadEncodedAsset(f_CheepCheepImages[1], &asset);
  f_MonkeyImage = AllocMemory(OpenEncodedAsset(MONKEY_ASSET, &asset));
  ReadEncodedAsset(f_MonkeyImage, &asset);
  f_AreCheepCheepsLeaping = FALSE;
}

void FreeHoleCreatureImages(void) {
  FreeMemory(&f_CheepCheepImages[0]);
  FreeMemory(&f_CheepCheepImages[1]);
  FreeMemory(&f_MonkeyImage);
}

/* The yaw the camera looks along, or 0 when it looks straight up or down */
Radians GetCameraYaw(void) {
  f32 x = g_Cameras->view.lookAt[0] - g_Cameras->view.eye[0];
  Radians yaw = 0.0f;
  f32 z = g_Cameras->view.lookAt[2] - g_Cameras->view.eye[2];

  if ((s32)x != 0 || (s32)z != 0) {
    yaw = atan2f(x, z);
  }

  return yaw;
}

Boolean IsMonkeyShown(void) {
  s32 i;
  HoleCreature* creature;

  for (i = 0; i != HOLE_CREATURE_COUNT; i++) {
    creature = &f_HoleCreatures[i];
    if (creature->state == HOLE_CREATURE_MONKEY) {
      return TRUE;
    }
  }

  return FALSE;
}

/* Drops the monkey onto the cup, facing away from cameraYaw */
void StartMonkeyDrop(Radians cameraYaw) {
  EncodedAsset asset;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  Mat4 transform;
  s32 i;
  HoleCreature* creature;

  if (IsMonkeyShown()) {
    return;
  }

  OpenEncodedAsset(f_MonkeyFrameAssets[0], &asset);
  ReadEncodedAsset(f_MonkeyImage, &asset);
  for (i = 0; i != HOLE_CREATURE_COUNT; i++) {
    creature = &f_HoleCreatures[i];
    if (creature->state == HOLE_CREATURE_NONE) {
      creature->state = HOLE_CREATURE_MONKEY;
      creature->isGold = FALSE;
      creature->isFacingBack = FALSE;
      creature->frameCount = 0;
      creature->velocity[1] = 0;
      creature->position[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
      creature->position[AXIS_Y] =
          g_HoleSetup.cupPosition[AXIS_Y] - MONKEY_DROP_HEIGHT;
      creature->position[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
      creature->yaw = -cameraYaw + M_PI;
      translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      rotation[AXIS_X] = 0.0f;
      rotation[AXIS_Y] = creature->yaw;
      rotation[AXIS_Z] = 0;
      scale[AXIS_X] = 1.0f;
      scale[AXIS_Y] = 1.0f;
      scale[AXIS_Z] = 1.0f;
      BuildTransformMatrix(transform, rotation, translation, scale);
      ConvertMatrixToFixed(transform, &creature->matrices[0]);
      ConvertMatrixToFixed(transform, &creature->matrices[1]);
      return;
    }
  }
}

void ClearHoleCreatures(void) {
  s32 i;
  HoleCreature* creature;

  for (i = 0; i != HOLE_CREATURE_COUNT; i++) {
    creature = &f_HoleCreatures[i];
    creature->state = HOLE_CREATURE_NONE;
  }
}

/*
 * Sends a Cheep Cheep leaping across the wind from below the ball.
 *
 * Doesn't match: GCC schedules the store of the rotation's zero X angle
 * right after the Cheep Cheep's velocity, where the original stores it after
 * converting the translation. sched can't tell the array store from the loads
 * of the creature's position, so they wait for it. Clearing it later, or
 * through a pointer, which frees it, lets the velocity's Z store slide past
 * the first load instead: 14 to 26 words. No order of the eight statements
 * does better, nor velocity stores through a pointer or the array. 2 words
 * differ.
 */
#ifdef NON_MATCHING
void StartCheepCheepLeap(s32 arg0) {
  EncodedAsset asset;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  Mat4 transform;
  s32 i;
  HoleCreature* creature;
  f32 speed;

  f_CheepCheepLeapUnk38 = arg0;
  f_AreCheepCheepsLeaping = TRUE;
  f_CheepCheepLeapUnk3C = 0;
  f_CheepCheepLeapUnk40 = 0;
  for (i = 0; i != HOLE_CREATURE_COUNT; i++) {
    creature = &f_HoleCreatures[i];
    if (creature->state == HOLE_CREATURE_NONE) {
      s32 random = guRandom();

      creature->state = HOLE_CREATURE_CHEEP_CHEEP;
      creature->position[AXIS_X] = g_TurnBallSimulation.position[AXIS_X];
      creature->position[AXIS_Y] =
          g_TurnBallSimulation.position[AXIS_Y] + CHEEP_CHEEP_DEPTH;
      creature->position[AXIS_Z] = g_TurnBallSimulation.position[AXIS_Z];
      speed = (random % CHEEP_CHEEP_SPEED_RANGE) + CHEEP_CHEEP_MIN_SPEED;
      creature->groundHeight =
          GetGroundOrWaterHeight(g_TurnBallSimulation.position[AXIS_X],
              g_TurnBallSimulation.position[AXIS_Z]);
      creature->isFacingBack = guRandom() % 2;
      creature->isGold = guRandom() % 2;
      if (!creature->isGold) {
        OpenEncodedAsset(CHEEP_CHEEP_ASSET, &asset);
        ReadEncodedAsset(f_CheepCheepImages[0], &asset);
      } else {
        OpenEncodedAsset(GOLD_CHEEP_CHEEP_ASSET, &asset);
        ReadEncodedAsset(f_CheepCheepImages[1], &asset);
      }

      creature->yaw = g_SkyState.yaws[0];
      if (creature->isFacingBack) {
        speed = -speed;
      }

      creature->velocity[0] = cosf(creature->yaw) * speed;
      creature->velocity[2] = -sinf(creature->yaw) * speed;
      rotation[AXIS_X] = 0;
      creature->velocity[1] = -CHEEP_CHEEP_LEAP_SPEED;
      creature->pitch = -M_PI / 2;
      translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      rotation[AXIS_Y] = creature->yaw;
      rotation[AXIS_Z] = creature->pitch + (M_PI / 2);
      scale[AXIS_X] = 1.0f;
      scale[AXIS_Y] = 1.0f;
      scale[AXIS_Z] = 1.0f;
      BuildTransformMatrix(transform, rotation, translation, scale);
      ConvertMatrixToFixed(transform, &creature->matrices[0]);
      ConvertMatrixToFixed(transform, &creature->matrices[1]);
      return;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", StartCheepCheepLeap);
#endif

void DrawHoleCreatures(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 i;
  HoleCreature* creature;
  f32 unused[6] UNUSED;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  Mat4 transform;
  Mat4 roll;
  CoursePoint splashPosition;
  EncodedAsset asset;
  f32 horizontalSpeed;
  f32 horizontalSpeedSquared;
  Boolean wasUnderwater;

  IsGameFlagSet(GAME_FLAG_UNK5F);
  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(
      gfx++, G_RM_AA_ZB_XLU_SURF | Z_UPD, G_RM_AA_ZB_XLU_SURF2 | Z_UPD);
  gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
      TEXEL0, PRIMITIVE, 0, TEXEL0, 0);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x01);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  for (i = 0; i != HOLE_CREATURE_COUNT; i++) {
    creature = &f_HoleCreatures[i];
    if (creature->state == HOLE_CREATURE_NONE) {
    } else if (creature->state == HOLE_CREATURE_CHEEP_CHEEP) {
      wasUnderwater =
          g_TurnBallSimulation.position[AXIS_Y] < creature->position[AXIS_Y];
      creature->velocity[1] += CHEEP_CHEEP_GRAVITY;
      creature->position[AXIS_X] += creature->velocity[0];
      creature->position[AXIS_Y] += creature->velocity[1];
      creature->position[AXIS_Z] += creature->velocity[2];
      horizontalSpeed = 0.0f;
      horizontalSpeedSquared = (creature->velocity[0] * creature->velocity[0]) +
          (creature->velocity[2] * creature->velocity[2]);
      if ((s32)horizontalSpeedSquared > 0) {
        horizontalSpeed = sqrtf(horizontalSpeedSquared);
      }

      creature->pitch = ((f32)atan2(creature->velocity[1], horizontalSpeed) -
                            creature->pitch) *
          CHEEP_CHEEP_PITCH_EASING;
      translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      rotation[AXIS_X] = 0.0f;
      rotation[AXIS_Y] = creature->yaw;
      rotation[AXIS_Z] = 0;
      if (!creature->isFacingBack) {
        scale[AXIS_Y] = 1.0f;
        scale[AXIS_Z] = 1.0f;
        scale[AXIS_X] = -1.0f;
      } else {
        scale[AXIS_X] = 1.0f;
        scale[AXIS_Y] = 1.0f;
        scale[AXIS_Z] = 1.0f;
      }

      BuildTransformMatrix(transform, rotation, translation, scale);
      guRotateRPYF(roll, 0.0f, 0.0f, -creature->pitch * 360.0f / (2 * M_PI));
      guMtxCatF(roll, transform, transform);
      ConvertMatrixToFixed(transform, &creature->matrices[g_MatrixBufferIndex]);
      if (wasUnderwater == TRUE) {
        if (creature->position[AXIS_Y] <
            g_TurnBallSimulation.position[AXIS_Y]) {
          splashPosition[AXIS_X] = creature->position[AXIS_X];
          splashPosition[AXIS_Z] = creature->position[AXIS_Z];
          splashPosition[AXIS_Y] = creature->groundHeight;
          SpawnRipple(splashPosition);
          PlaySound(
              SOUND_SPLASH, HOLE_OBJECT_SOUND_CHANNEL, SPLASH_SOUND_VOLUME);
        }
      } else if (creature->position[AXIS_Y] >
          g_TurnBallSimulation.position[AXIS_Y]) {
        splashPosition[AXIS_X] = creature->position[AXIS_X];
        splashPosition[AXIS_Z] = creature->position[AXIS_Z];
        splashPosition[AXIS_Y] = creature->groundHeight;
        SpawnRipple(splashPosition);
        PlaySound(SOUND_SPLASH, HOLE_OBJECT_SOUND_CHANNEL, SPLASH_SOUND_VOLUME);
      }

      if (creature->position[AXIS_Y] < g_TurnBallSimulation.position[AXIS_Y]) {
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      } else {
        s32 alpha = (g_TurnBallSimulation.position[AXIS_Y] -
                        creature->position[AXIS_Y]) *
            0xFF / CHEEP_CHEEP_FADE_DEPTH;

        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
      }

      if (g_TurnBallSimulation.position[AXIS_Y] + CHEEP_CHEEP_DEPTH <
          creature->position[AXIS_Y]) {
        creature->state = HOLE_CREATURE_NONE;
      }

      gDPLoadTextureBlock(gfx++,
          TextureAddress(ImageBody(f_CheepCheepImages[creature->isGold])),
          G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPMatrix(gfx++,
          PhysicalAddress(&creature->matrices[g_MatrixBufferIndex]),
          G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_CheepCheepVertices, 4, 0);
      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
      gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    } else if (creature->state == HOLE_CREATURE_MONKEY) {
      creature->velocity[1] += GRAVITY;
      creature->position[AXIS_Y] += creature->velocity[1];
      switch (++creature->frameCount) {
        case MONKEY_SECOND_FRAME:
          OpenEncodedAsset(f_MonkeyFrameAssets[1], &asset);
          ReadEncodedAsset(f_MonkeyImage, &asset);
          break;
        case MONKEY_THIRD_FRAME:
          OpenEncodedAsset(f_MonkeyFrameAssets[2], &asset);
          ReadEncodedAsset(f_MonkeyImage, &asset);
          break;
        case MONKEY_LAST_FRAME:
          OpenEncodedAsset(f_MonkeyFrameAssets[3], &asset);
          ReadEncodedAsset(f_MonkeyImage, &asset);
          PlaySound(SOUND_MONKEY, HOLE_OBJECT_SOUND_CHANNEL, SOUND_FULL_SCALE);
          break;
      }

      if (creature->position[AXIS_Y] > g_HoleSetup.cupPosition[AXIS_Y]) {
        creature->position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
      }

      if (creature->frameCount >= MONKEY_LIFETIME) {
        creature->state = HOLE_CREATURE_NONE;
      }

      translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      rotation[AXIS_X] = 0.0f;
      rotation[AXIS_Y] = GetCameraYaw();
      rotation[AXIS_Z] = 0;
      scale[AXIS_X] = 1.0f;
      scale[AXIS_Y] = 1.0f;
      scale[AXIS_Z] = 1.0f;
      BuildTransformMatrix(transform, rotation, translation, scale);
      ConvertMatrixToFixed(transform, &creature->matrices[g_MatrixBufferIndex]);
      gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      gDPPipeSync(gfx++);
      gSPMatrix(gfx++,
          PhysicalAddress(&creature->matrices[g_MatrixBufferIndex]),
          G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_MonkeyVertices, 8, 0);
      gDPLoadTextureBlock(gfx++, TextureAddress(ImageBody(f_MonkeyImage)),
          G_IM_FMT_RGBA, G_IM_SIZ_16b, 64, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
      gDPPipeSync(gfx++);
      gDPLoadTextureBlock(gfx++,
          TextureAddress(ImageBody(f_MonkeyImage) + MONKEY_HALF_IMAGE_SIZE),
          G_IM_FMT_RGBA, G_IM_SIZ_16b, 64, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gSP2Triangles(gfx++, 4, 5, 6, 0, 4, 6, 7, 0);
      gDPPipeSync(gfx++);
      gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    }
  }

  *displayListHead = gfx;
}
