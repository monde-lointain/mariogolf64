/*
 * The fireworks that the turn starts in sudden death. Each one bursts from a
 * random point into the outline of a character, picked by the course.
 */
#include "common.h"

#include "fireworks.h"

#include "asset.h"
#include "cpu_projection.h"
#include "functions.h"
#include "heap.h"
#include "math_util.h"
#include "screen.h"
#include "sound.h"
#include "variables.h"

/* How many fireworks f_Fireworks has room for */
#define FIREWORK_CAPACITY 4

/* How many of them LoadFireworks sets up */
#define FIREWORK_COUNT 1

/* f_FireworkState's value while the fireworks' memory is allocated */
#define FIREWORKS_LOADED 1

/* Frames of a firework's timer at which it plays a sound and launches */
#define FIREWORK_SOUND_FRAME 160
#define FIREWORK_LAUNCH_FRAME 220

/* Below this life, a particle's life runs out five times as fast */
#define PARTICLE_FADE_LIFE 128

/* An image whose zero cells each become a particle of a firework */
typedef struct FireworkShape {
  /* 0x0 */ u8 unk0[3];
  /* 0x3 */ u8 width;
  /* 0x4 */ u8 unk4;
  /* 0x5 */ u8 height;
  /* 0x6 */ u8 unk6[2];
  /* 0x8 */ u8 cells[1]; /* width * height, by rows */
} FireworkShape;

typedef struct FireworkParticle {
  /* 0x00 */ Mtx matrices[2]; /* Indexed by g_MatrixBufferIndex */
  /* 0x80 */ f32 x;
  /* 0x84 */ f32 y;
  /* 0x88 */ f32 velocityX;
  /* 0x8C */ f32 velocityY;
  /* 0x90 */ u8 unk90[4];
  /* 0x94 */ s16 life; /* Drawn while above 0 */
} FireworkParticle; /* size = 0x98 */

typedef struct Firework {
  /* 0x00 */ FireworkParticle* particles;
  /* 0x04 */ s32 particleCount;
  /* 0x08 */ s32 timer;
  /* 0x0C */ s32 unkC;
  /* 0x10 */ u8 unk10[0x14];
  /* 0x24 */ u8 unk24;
  /* 0x25 */ u8 isActive;
  /* 0x28 */ FireworkShape* shape;
} Firework; /* size = 0x2C */

extern s32 f_FireworkCount;
extern s32 f_FireworkState;
extern Vtx f_FireworkParticleVertices[3];
/*
 * The shapes, by course record index: outlines of Toad, a Koopa Troopa, a Shy
 * Guy, Yoshi, a Boo, Wario, Mario and Peach
 */
extern u16 f_FireworkShapeAssetIds[8];
extern f32 f_FireworkHue;

/* Not static: the original left these to the linker's common block */
FireworkShape* f_FireworkShape;
Firework f_Fireworks[FIREWORK_CAPACITY];

/* Makes the first firework launch in 70 frames */
void StartFireworks(void) {
  f_Fireworks[0].timer = 150;
  f_Fireworks[0].isActive = TRUE;
}

void StopFireworks(void) {
  f_Fireworks[0].isActive = FALSE;
}

/*
 * Loads the shape for the course, picked by its record index, and allocates
 * the fireworks' particles
 */
#ifdef NON_MATCHING
void LoadFireworks(s32 courseRecordIndex) {
  EncodedAsset asset;
  s32 index;
  Firework* firework;
  s32 width;
  s32 height;
  s32 row;
  s32 column;

  f_FireworkShape =
      AllocMemory(OpenEncodedAsset(f_FireworkShapeAssetIds[courseRecordIndex %
                                       ARRAY_COUNT(f_FireworkShapeAssetIds)],
          &asset));
  ReadEncodedAsset(f_FireworkShape, &asset);
  for (index = 0; index != FIREWORK_COUNT; index++) {
    f_Fireworks[index].unk24 = 0;
    f_Fireworks[index].isActive = FALSE;
    f_Fireworks[index].timer = 159 - (index * 50);
    f_Fireworks[index].unkC = 0xFF;
    f_Fireworks[index].particleCount = 0;
    f_Fireworks[index].shape = f_FireworkShape;
  }

  f_FireworkCount = index;
  for (index = 0; index != f_FireworkCount; index++) {
    firework = &f_Fireworks[index];
    width = firework->shape->width;
    height = firework->shape->height;
    for (row = 0; row != height; row++) {
      for (column = 0; column != width; column++) {
        if (firework->shape->cells[column + (row * width)] == 0) {
          firework->particleCount++;
        }
      }
    }

    firework->particles =
        AllocMemory(firework->particleCount * sizeof(FireworkParticle));
    for (row = 0; row != firework->particleCount; row++) {
      firework->particles[row].life = 0;
    }
  }

  f_FireworkState = FIREWORKS_LOADED;
}

#else
INCLUDE_ASM("asm/nonmatchings/fireworks", LoadFireworks);
#endif

void FreeFireworks(void) {
  s32 index;

  if (f_FireworkState == FIREWORKS_LOADED) {
    FreeMemory(&f_FireworkShape);
    for (index = 0; index != f_FireworkCount; index++) {
      FreeMemory(&f_Fireworks[index].particles);
    }
  }

  f_FireworkState = 0;
}

/*
 * Unless the fireworks are loaded, counts a call and returns TRUE from the
 * third call after FreeFireworks on. Nothing calls it.
 */
s32 HaveFireworksStayedFreed(void) {
  if (f_FireworkState > 0) {
    return FALSE;
  }

  f_FireworkState--;
  return f_FireworkState < -2;
}

/* Sends every particle of the firework out from one random point */
#ifdef NON_MATCHING
void LaunchFirework(Firework* firework, s32 fireworkIndex) {
  s32 particleIndex;
  f32 originX;
  s32 random;
  f32 originY;
  u32 width;
  u32 height;
  s32 halfWidth;
  s32 row;
  s32 column;

  particleIndex = 0;
  originX = guRandom() % 32;
  random = guRandom() % 4;
  width = firework->shape->width;
  height = firework->shape->height;
  firework->unkC = 0xFF;
  originY = random + 64.0f;
  halfWidth = width / 2;
  for (row = 0; row != height; row++) {
    for (column = 0; column != width; column++) {
      if (firework->shape->cells[column + (row * width)] == 0) {
        firework->particles[particleIndex].x = originX;
        firework->particles[particleIndex].y = originY;
        firework->particles[particleIndex].velocityX =
            ((column - halfWidth) * 0.1f) +
            (((guRandom() % 64) - 32) * 0.0005f);
        firework->particles[particleIndex].velocityY =
            ((s32)((height / 2) - row) * 0.1f) - 0.5 +
            (((guRandom() % 64) - 32) * 0.0005f);
        firework->particles[particleIndex].life = (guRandom() % 128) + 192;
        particleIndex++;
      }
    }
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/fireworks", LaunchFirework);
#endif

/* Moves the active fireworks' particles and draws each as a triangle along its
 * path */
#ifdef NON_MATCHING
void UpdateAndDrawFireworks(Gfx** displayListHead) {
  Gfx* gfx;
  s32 unused[12]; /* The original frame has room for this */
  Mat4 transform;
  s32 unusedAfter[6]; /* The original frame has room for this too */
  s32 fireworkIndex;
  s32 index;
  s32 particleIndex;
  FireworkParticle* particle;
  f32 speed;
  f32 directionX;
  f32 directionY;
  f32 inverseSpeed;
  f32 length;
  s32 timer;
  s32 red;
  s32 green;
  s32 blue;
  s32 alpha;

  gfx = *displayListHead;
  if (f_FireworkState <= 0) {
    return;
  }

  for (fireworkIndex = 0; fireworkIndex != f_FireworkCount; fireworkIndex++) {
    if (f_Fireworks[fireworkIndex].isActive == TRUE) {
      timer = f_Fireworks[fireworkIndex].timer + 1;
      f_Fireworks[fireworkIndex].timer = timer;
      if (timer == FIREWORK_SOUND_FRAME) {
        PlaySound(0x31, 8, SOUND_FULL_SCALE);
      } else if (timer == FIREWORK_LAUNCH_FRAME) {
        f_Fireworks[fireworkIndex].timer = 0;
        LaunchFirework(&f_Fireworks[fireworkIndex], fireworkIndex);
        PlaySound(0x32, 8, SOUND_FULL_SCALE);
      }
    }
  }

  transform[0][2] = 0.0f;
  transform[0][3] = 0.0f;
  transform[1][2] = 0.0f;
  transform[1][3] = 0.0f;
  transform[2][0] = 0.0f;
  transform[2][1] = 0.0f;
  transform[2][3] = 0.0f;
  transform[2][2] = 0.1f;
  transform[3][3] = 1.0f;
  for (index = 0; index != f_FireworkCount; index++) {
    if (f_Fireworks[index].isActive == TRUE) {
      for (particleIndex = 0; particleIndex != f_Fireworks[index].particleCount;
          particleIndex++) {
        particle = &f_Fireworks[index].particles[particleIndex];
        if (particle->life > 0) {
          particle->x += particle->velocityX;
          particle->y += particle->velocityY;
          particle->velocityX *= 0.94f;
          particle->velocityY = (particle->velocityY * 0.94f) - 0.0005f;
          if (--particle->life < PARTICLE_FADE_LIFE) {
            particle->life -= 4;
          }

          speed = sqrtf((particle->velocityX * particle->velocityX) +
              (particle->velocityY * particle->velocityY));
          directionX = 0.0f;
          if (speed != 0.0f) {
            inverseSpeed = 1.0f / speed;
            directionX = particle->velocityX * inverseSpeed;
            directionY = particle->velocityY * inverseSpeed;
          } else {
            directionY = 0.1f;
            speed = 0.5f;
          }

          timer = f_Fireworks[index].timer;
          if (timer < 8) {
            length = speed * 0.5f * timer;
          } else if (timer > 64) {
            length = ((timer - 64) * 0.04f) + 1.0f;
          } else {
            length = speed * (4.0f - ((timer - 8) * 0.5f));
          }

          if (length < 1.0f) {
            length = 1.0f;
          }

          transform[0][0] = -directionY * 0.1f;
          transform[0][1] = directionX * 0.1f;
          transform[1][0] = -directionX * (length * 0.1f);
          transform[1][1] = -directionY * (length * 0.1f);
          transform[3][0] = particle->x;
          transform[3][1] = particle->y;
          transform[3][2] = -100.0f;
          ConvertMatrixToFixed(
              transform, &particle->matrices[g_MatrixBufferIndex]);
        }
      }
    }
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_OFF);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPSetCombineLERP(gfx++, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0,
      PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  f_FireworkHue += DEGREES_TO_RADIANS(3);

  /* index is f_FireworkCount here, so every firework shares one color */
  red = (s32)(sinf(f_FireworkHue + index) * 64.0f) + 127;
  green = (s32)(sinf(f_FireworkHue + 2.0f + index) * 64.0f) + 127;
  blue = (s32)(sinf(f_FireworkHue + 4.0f + index) * 64.0f) + 127;
  for (index = 0; index != f_FireworkCount; index++) {
    if (f_Fireworks[index].isActive == TRUE) {
      timer = f_Fireworks[index].timer;
      alpha = 0xFF;

      /* Fades out from frame 40, flickering on odd frames */
      if (timer >= 40) {
        alpha = 0xFF - ((timer - 40) * 8);
        if (timer & 1) {
          alpha += 0x18;
        }
      }

      if (alpha < 0) {
        alpha = 0;
      }

      if (alpha > 0xFF) {
        alpha = 0xFF;
      }

      gDPSetPrimColor(gfx++, 0, 0, red, green, blue, alpha);
      for (particleIndex = 0; particleIndex != f_Fireworks[index].particleCount;
          particleIndex++) {
        particle = &f_Fireworks[index].particles[particleIndex];
        if (particle->life > 0) {
          gDPPipeSync(gfx++);
          gSPMatrix(gfx++,
              OS_K0_TO_PHYSICAL(&particle->matrices[g_MatrixBufferIndex]),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
          gSPVertex(gfx++, f_FireworkParticleVertices,
              ARRAY_COUNT(f_FireworkParticleVertices), 0);
          gSP1Triangle(gfx++, 0, 1, 2, 0);
        }
      }
    }
  }

  *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/fireworks", UpdateAndDrawFireworks);
#endif
