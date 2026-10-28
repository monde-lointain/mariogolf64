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
#include "game_flags.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "sound.h"
#include "terrain.h"
#include "variables.h"

#define HOLE_CREATURE_COUNT 10

/* HoleCreature.state values */
#define HOLE_CREATURE_NONE 0
#define HOLE_CREATURE_CHEEP_CHEEP 1
#define HOLE_CREATURE_MONKEY 2

#define CHEEP_CHEEP_ASSET 0x6E0
#define GOLD_CHEEP_CHEEP_ASSET 0x6E5
#define MONKEY_ASSET 0x6F0 /* The first of its frames */
#define MONKEY_FRAME_COUNT 4

/* Course heights grow downwards, so these lift a creature by subtracting */
/* Below the ball where a Cheep Cheep starts and ends */
#define CHEEP_CHEEP_DEPTH 0x7800
#define CHEEP_CHEEP_LEAP_SPEED 9216.0f /* Upwards */
#define CHEEP_CHEEP_GRAVITY 524.0f
/* And the range of random speeds above it */
#define CHEEP_CHEEP_MIN_SPEED 0x600
#define CHEEP_CHEEP_SPEED_RANGE 0x600
#define CHEEP_CHEEP_PITCH_EASING 0.7f
/* Below the ball, over which a Cheep Cheep fades out */
#define CHEEP_CHEEP_FADE_DEPTH 46080
#define MONKEY_DROP_HEIGHT 0x9600 /* Above the cup */
#define MONKEY_GRAVITY 262.0f
/* The frames at which the monkey shows each of its later images */
#define MONKEY_SECOND_FRAME 16
#define MONKEY_THIRD_FRAME 20
#define MONKEY_LAST_FRAME 24
#define MONKEY_LIFETIME 46

#define SPLASH_SOUND 8
#define MONKEY_SOUND 0x2C
#define SOUND_CHANNEL 15
#define SPLASH_SOUND_VOLUME 0x5A
#define MONKEY_SOUND_VOLUME 0x7F

/* The monkey is drawn as two halves, each loaded as one texture */
#define MONKEY_HALF_IMAGE_SIZE 0x1000

typedef struct HoleCreature {
  /* 0x00 */ Mtx matrices[DRAW_BUFFER_COUNT];
  /* 0x80 */ CoursePoint position;
  /* 0x8C */ f32 velocity[AXIS_COUNT];
  /* 0x98 */ s32 groundHeight;
  /* 0x9C */ f32 yaw;
  /* 0xA0 */ f32 pitch;
  /* 0xA4 */ u8 frameCount;
  /* 0xA5 */ u8 isFacingBack;
  /* 0xA6 */ u8 isGold;
  /* 0xA7 */ u8 state;
} HoleCreature; /* size = 0xA8 */

extern u16 f_MonkeyFrameAssets[MONKEY_FRAME_COUNT];
extern s32 f_AreCheepCheepsLeaping;
extern s32 f_CheepCheepLeapUnk38;
extern s32 f_CheepCheepLeapUnk3C;
extern s32 f_CheepCheepLeapUnk40;
extern Vtx f_CheepCheepVertices[4];
extern Vtx f_MonkeyVertices[8];
static u8* f_CheepCheepImages[2];
static u8* f_MonkeyImage;
extern HoleCreature f_HoleCreatures[HOLE_CREATURE_COUNT];
extern Gfx D_800C3FC0[];

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
f32 GetCameraYaw(void) {
  f32 x = g_Cameras->view.lookAt[0] - g_Cameras->view.eye[0];
  f32 yaw = 0.0f;
  f32 z = g_Cameras->view.lookAt[2] - g_Cameras->view.eye[2];

  if ((s32)x != 0 || (s32)z != 0) {
    yaw = atan2f(x, z);
  }

  return yaw;
}

s32 IsMonkeyShown(void) {
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
void StartMonkeyDrop(f32 cameraYaw) {
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
      creature->position[0] = D_800BA9D8.cupPosition[0];
      creature->position[1] = D_800BA9D8.cupPosition[1] - MONKEY_DROP_HEIGHT;
      creature->position[2] = D_800BA9D8.cupPosition[2];
      creature->yaw = -cameraYaw + M_PI;
      translation[0] = creature->position[0] * COURSE_TO_WORLD_SCALE;
      translation[1] = creature->position[1] * COURSE_TO_WORLD_SCALE;
      translation[2] = creature->position[2] * COURSE_TO_WORLD_SCALE;
      rotation[0] = 0.0f;
      rotation[1] = creature->yaw;
      rotation[2] = 0;
      scale[0] = 1.0f;
      scale[1] = 1.0f;
      scale[2] = 1.0f;
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

/* Sends a Cheep Cheep leaping across the wind from below the ball */
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
      creature->position[0] = g_BallPosition[0];
      creature->position[1] = g_BallPosition[1] + CHEEP_CHEEP_DEPTH;
      creature->position[2] = g_BallPosition[2];
      speed = (random % CHEEP_CHEEP_SPEED_RANGE) + CHEEP_CHEEP_MIN_SPEED;
      creature->groundHeight =
          GetGroundOrWaterHeight(g_BallPosition[0], g_BallPosition[2]);
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
      rotation[0] = 0;
      creature->velocity[1] = -CHEEP_CHEEP_LEAP_SPEED;
      creature->pitch = -M_PI / 2;
      translation[0] = creature->position[0] * COURSE_TO_WORLD_SCALE;
      translation[1] = creature->position[1] * COURSE_TO_WORLD_SCALE;
      translation[2] = creature->position[2] * COURSE_TO_WORLD_SCALE;
      rotation[1] = creature->yaw;
      rotation[2] = creature->pitch + (M_PI / 2);
      scale[0] = 1.0f;
      scale[1] = 1.0f;
      scale[2] = 1.0f;
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
  f32 unused[6];
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  Mat4 transform;
  Mat4 roll;
  CoursePoint splashPosition;
  EncodedAsset asset;
  f32 horizontalSpeed;
  f32 horizontalSpeedSquared;
  s32 wasUnderwater;

  IsGameFlagSet(GAME_FLAG_UNK5F);
  gSPDisplayList(gfx++, D_800C3FC0);
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
      wasUnderwater = g_BallPosition[1] < creature->position[1];
      creature->velocity[1] += CHEEP_CHEEP_GRAVITY;
      creature->position[0] += creature->velocity[0];
      creature->position[1] += creature->velocity[1];
      creature->position[2] += creature->velocity[2];
      horizontalSpeed = 0.0f;
      horizontalSpeedSquared = (creature->velocity[0] * creature->velocity[0]) +
          (creature->velocity[2] * creature->velocity[2]);
      if ((s32)horizontalSpeedSquared > 0) {
        horizontalSpeed = sqrtf(horizontalSpeedSquared);
      }

      creature->pitch = ((f32)atan2(creature->velocity[1], horizontalSpeed) -
                            creature->pitch) *
          CHEEP_CHEEP_PITCH_EASING;
      translation[0] = creature->position[0] * COURSE_TO_WORLD_SCALE;
      translation[1] = creature->position[1] * COURSE_TO_WORLD_SCALE;
      translation[2] = creature->position[2] * COURSE_TO_WORLD_SCALE;
      rotation[0] = 0.0f;
      rotation[1] = creature->yaw;
      rotation[2] = 0;
      if (!creature->isFacingBack) {
        scale[1] = 1.0f;
        scale[2] = 1.0f;
        scale[0] = -1.0f;
      } else {
        scale[0] = 1.0f;
        scale[1] = 1.0f;
        scale[2] = 1.0f;
      }

      BuildTransformMatrix(transform, rotation, translation, scale);
      guRotateRPYF(roll, 0.0f, 0.0f, -creature->pitch * 360.0f / (2 * M_PI));
      guMtxCatF(roll, transform, transform);
      ConvertMatrixToFixed(transform, &creature->matrices[g_MatrixBufferIndex]);
      if (wasUnderwater == TRUE) {
        if (creature->position[1] < g_BallPosition[1]) {
          splashPosition[0] = creature->position[0];
          splashPosition[2] = creature->position[2];
          splashPosition[1] = creature->groundHeight;
          SpawnRipple(splashPosition);
          PlaySound(SPLASH_SOUND, SOUND_CHANNEL, SPLASH_SOUND_VOLUME);
        }
      } else if (creature->position[1] > g_BallPosition[1]) {
        splashPosition[0] = creature->position[0];
        splashPosition[2] = creature->position[2];
        splashPosition[1] = creature->groundHeight;
        SpawnRipple(splashPosition);
        PlaySound(SPLASH_SOUND, SOUND_CHANNEL, SPLASH_SOUND_VOLUME);
      }

      if (creature->position[1] < g_BallPosition[1]) {
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      } else {
        s32 alpha = (g_BallPosition[1] - creature->position[1]) * 0xFF /
            CHEEP_CHEEP_FADE_DEPTH;

        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
      }

      if (g_BallPosition[1] + CHEEP_CHEEP_DEPTH < creature->position[1]) {
        creature->state = HOLE_CREATURE_NONE;
      }

      gDPLoadTextureBlock(gfx++,
          TEXTURE_ADDRESS(
              f_CheepCheepImages[creature->isGold] + IMAGE_HEADER_SIZE),
          G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPMatrix(gfx++,
          OS_K0_TO_PHYSICAL(&creature->matrices[g_MatrixBufferIndex]),
          G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_CheepCheepVertices, 4, 0);
      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
      gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    } else if (creature->state == HOLE_CREATURE_MONKEY) {
      creature->velocity[1] += MONKEY_GRAVITY;
      creature->position[1] += creature->velocity[1];
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
          PlaySound(MONKEY_SOUND, SOUND_CHANNEL, MONKEY_SOUND_VOLUME);
          break;
      }

      if (creature->position[1] > D_800BA9D8.cupPosition[AXIS_Y]) {
        creature->position[1] = D_800BA9D8.cupPosition[AXIS_Y];
      }

      if (creature->frameCount >= MONKEY_LIFETIME) {
        creature->state = HOLE_CREATURE_NONE;
      }

      translation[0] = creature->position[0] * COURSE_TO_WORLD_SCALE;
      translation[1] = creature->position[1] * COURSE_TO_WORLD_SCALE;
      translation[2] = creature->position[2] * COURSE_TO_WORLD_SCALE;
      rotation[0] = 0.0f;
      rotation[1] = GetCameraYaw();
      rotation[2] = 0;
      scale[0] = 1.0f;
      scale[1] = 1.0f;
      scale[2] = 1.0f;
      BuildTransformMatrix(transform, rotation, translation, scale);
      ConvertMatrixToFixed(transform, &creature->matrices[g_MatrixBufferIndex]);
      gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      gDPPipeSync(gfx++);
      gSPMatrix(gfx++,
          OS_K0_TO_PHYSICAL(&creature->matrices[g_MatrixBufferIndex]),
          G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_MonkeyVertices, 8, 0);
      gDPLoadTextureBlock(gfx++,
          TEXTURE_ADDRESS(f_MonkeyImage + IMAGE_HEADER_SIZE), G_IM_FMT_RGBA,
          G_IM_SIZ_16b, 64, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
      gDPPipeSync(gfx++);
      gDPLoadTextureBlock(gfx++,
          TEXTURE_ADDRESS(
              f_MonkeyImage + IMAGE_HEADER_SIZE + MONKEY_HALF_IMAGE_SIZE),
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
