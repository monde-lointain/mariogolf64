/*
 * The fireworks that the turn starts in sudden death. Each one bursts from a
 * random point into the outline of a character, picked by the course.
 */
#include "common.h"

#include "fireworks.h"

#include "asset.h"
#include "cpu_projection.h"
#include "heap.h"
#include "math_util.h"
#include "round_hud.h"
#include "screen.h"
#include "sound.h"

/* How many of them LoadFireworks sets up */
#define FIREWORK_COUNT 1

/* f_FireworkState's value while the fireworks' memory is allocated */
#define FIREWORKS_LOADED 1

/* Frames of a firework's timer at which it plays a sound and launches */
#define FIREWORK_SOUND_FRAME 160
#define FIREWORK_LAUNCH_FRAME 220
/* The fireworks' sounds play on this channel */
#define FIREWORK_SOUND_CHANNEL 8

/* Below this life, a particle's life runs out five times as fast */
#define PARTICLE_FADE_LIFE 128

/* Launch positions and velocities are in the fireworks' local coordinates. */
#define FIREWORK_LAUNCH_WIDTH 32
#define FIREWORK_LAUNCH_HEIGHT 64.0f
#define FIREWORK_LAUNCH_HEIGHT_JITTER 4
#define PARTICLE_SPEED_PER_CELL 0.1f
#define PARTICLE_VELOCITY_JITTER_STEPS 64
#define PARTICLE_VELOCITY_JITTER_STEP 0.0005f
#define PARTICLE_LAUNCH_DOWNWARD_SPEED 0.5
#define PARTICLE_MIN_LIFE 192
#define PARTICLE_LIFE_JITTER 128

/* A random per-axis velocity offset. */
#define RandomParticleVelocityJitter() \
  (((guRandom() % PARTICLE_VELOCITY_JITTER_STEPS) - \
       (PARTICLE_VELOCITY_JITTER_STEPS / 2)) * \
      PARTICLE_VELOCITY_JITTER_STEP)

/* An image whose zero cells each become a particle of a firework */
struct FireworkShape {
  /* 0x0 */ u8 unk0[3];
  /* 0x3 */ u8 width;
  /* 0x4 */ u8 unk4;
  /* 0x5 */ u8 height;
  /* 0x6 */ u8 unk6[2];
  /* 0x8 */ u8 cells[TRAILING_ARRAY_LENGTH]; /* width * height, by rows */
};

struct FireworkParticle {
  /* 0x00 */ Mtx matrices[MATRIX_BUFFER_COUNT];
  /* 0x80 */ f32 x;
  /* 0x84 */ f32 y;
  /* 0x88 */ f32 velocityX;
  /* 0x8C */ f32 velocityY;
  /* 0x90 */ u8 unk90[4];
  /* 0x94 */ s16 life; /* Drawn while above 0 */
}; /* size = 0x98 */

static s32 f_FireworkCount = 0;
static s32 f_FireworkState = 0;
static Vtx f_FireworkParticleVertices[3] = {
    {{{0, 32, 0}, 0, {0, 0}, {0xFF, 0x7F, 0x00, 0x7F}}},
    {{{-10, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xBF, 0xBF}}},
    {{{10, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xBF, 0xBF}}}};
/*
 * The shapes, by course record index: outlines of Toad, a Koopa Troopa, a Shy
 * Guy, Yoshi, a Boo, Wario, Mario and Peach
 */
static AssetId f_FireworkShapeAssetIds[8] = {
    0x95F, 0x960, 0x961, 0x962, 0x963, 0x964, 0x965, 0x966};
static f32 f_FireworkHue = 0.0f;
/* Read by no routine */
static u8 f_UnusedBufferAfterFireworkHue[0xC04] UNUSED = {0};

FILE_STATIC FireworkShape* f_FireworkShape;
FILE_STATIC Firework f_Fireworks[FIREWORK_CAPACITY];

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
void LoadFireworks(s32 courseRecordIndex) {
  EncodedAsset asset;
  s32 index;
  Firework* firework;
  s32 width;
  s32 height;
  s32 row;
  s32 column;
  u8* cell;

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
        cell = &firework->shape->cells[column + (row * width)];
        if (*cell == 0) {
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

/*
 * The random part of a firework's launch height, 0 to 3. A function rather
 * than inline code to match: LaunchFirework's draft gives 50 differing words
 * with it, 69 with guRandom() % 4 written in place, and 68 with & 3.
 */
static inline s32 RandomLaunchHeightJitter(void) {
  return guRandom() % FIREWORK_LAUNCH_HEIGHT_JITTER;
}

/*
 * Sends every particle of the firework out from one random point. Doesn't
 * match: the original also keeps a copy of the row's first cell index, beside
 * the register loop.c steps it in, and spills the copy, which moves width and
 * height to other registers. A rowStart variable compiles the same as row *
 * width. Keeping the row's offset from the middle in a local gives 108 words, a
 * halfHeight beside halfWidth 118. The register oracle, forcing 6 pseudos'
 * registers, leaves 24 words at best, so the code differs too. 50 words differ,
 * 14 in shape.
 */
#ifdef NON_MATCHING
void LaunchFirework(Firework* firework, s32 fireworkIndex UNUSED) {
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
  originX = guRandom() % FIREWORK_LAUNCH_WIDTH;
  random = RandomLaunchHeightJitter();
  width = firework->shape->width;
  height = firework->shape->height;
  firework->unkC = 0xFF;
  originY = random + FIREWORK_LAUNCH_HEIGHT;
  for (row = 0; (u32)row != height; row++) {
    halfWidth = width / 2;
    for (column = 0; (u32)column != width; column++) {
      if (firework->shape->cells[column + (row * width)] == 0) {
        firework->particles[particleIndex].x = originX;
        firework->particles[particleIndex].y = originY;
        firework->particles[particleIndex].velocityX =
            ((column - halfWidth) * PARTICLE_SPEED_PER_CELL) +
            RandomParticleVelocityJitter();
        firework->particles[particleIndex].velocityY =
            ((s32)((height / 2) - row) * PARTICLE_SPEED_PER_CELL) -
            PARTICLE_LAUNCH_DOWNWARD_SPEED + RandomParticleVelocityJitter();
        firework->particles[particleIndex].life =
            (guRandom() % PARTICLE_LIFE_JITTER) + PARTICLE_MIN_LIFE;
        particleIndex++;
      }
    }
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/fireworks", LaunchFirework);
#endif

/*
 * Moves the active fireworks' particles and draws each as a triangle along its
 * path
 */
void UpdateAndDrawFireworks(Gfx** displayListHead) {
  Gfx* gfx;
  /* The original frame has room for this; without it 136 words differ */
  s32 unused[12] UNUSED;
  Mat4 transform;
  /* The original frame has room for this too; without it 100 words differ */
  s32 unusedAfter[6] UNUSED;
  /*
   * One index for the fireworks and the particles, to match: a particleIndex
   * for the particles gives 34 words differing, and for the updated particles
   * alone 62, none in shape
   */
  s32 i;
  s32 index;
  FireworkParticle* particle;
  FireworkParticle* drawnParticle;
  f32 directionX;
  f32 directionY;
  f32 inverseSpeed;
  /*
   * The particle's speed, until it scales into its trail's length, to match:
   * a separate speed variable gives 30 words differing, none in shape
   */
  f32 length;
  f32 tailScale;
  s32 timer;
  s32 fadeTimer;
  s32 trailTimer;
  s32 red;
  s32 green;
  s32 blue;
  s32 alpha;

  gfx = *displayListHead;
  if (f_FireworkState <= 0) {
    return;
  }

  for (i = 0; i != f_FireworkCount; i++) {
    if (f_Fireworks[i].isActive == TRUE) {
      timer = f_Fireworks[i].timer + 1;
      f_Fireworks[i].timer = timer;
      if (timer == FIREWORK_SOUND_FRAME) {
        PlaySound(
            SOUND_FIREWORK_LEAD_IN, FIREWORK_SOUND_CHANNEL, SOUND_FULL_SCALE);
      } else if (timer == FIREWORK_LAUNCH_FRAME) {
        f_Fireworks[i].timer = 0;
        LaunchFirework(&f_Fireworks[i], i);
        PlaySound(
            SOUND_FIREWORK_LAUNCH, FIREWORK_SOUND_CHANNEL, SOUND_FULL_SCALE);
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
      for (i = 0; i != f_Fireworks[index].particleCount; i++) {
        particle = &f_Fireworks[index].particles[i];
        if (particle->life > 0) {
          particle->x += particle->velocityX;
          particle->y += particle->velocityY;
          particle->velocityX *= 0.94f;
          particle->velocityY = (particle->velocityY * 0.94f) - 0.0005f;
          if (--particle->life < PARTICLE_FADE_LIFE) {
            particle->life -= 4;
          }

          length = sqrtf((particle->velocityX * particle->velocityX) +
              (particle->velocityY * particle->velocityY));
          if (length != 0.0f) {
            inverseSpeed = 1.0f / length;
            directionX = particle->velocityX * inverseSpeed;
            directionY = particle->velocityY * inverseSpeed;
          } else {
            directionX = 0.0f;
            directionY = 0.1f;
            length = 0.5f;
          }

          trailTimer = f_Fireworks[index].timer;
          if (trailTimer < 8) {
            length *= 0.5f * trailTimer;
          } else if (trailTimer > 64) {
            length = ((trailTimer - 64) * 0.04f) + 1.0f;
          } else {
            length *= 4.0f - ((trailTimer - 8) * 0.5f);
          }

          if (length < 1.0f) {
            length = 1.0f;
          }

          transform[0][0] = -directionY * 0.1f;
          transform[0][1] = directionX * 0.1f;
          tailScale = length * 0.1f;
          transform[1][0] = -directionX * tailScale;
          transform[1][1] = -directionY * tailScale;
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
  gSPMatrix(gfx++, PhysicalAddress(&g_ScreenOrthoMatrix),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  f_FireworkHue += DegreesToRadians(3);

  /* index is f_FireworkCount here, so every firework shares one color */
  red = (s32)(sinf(f_FireworkHue + index) * 64.0f) + 127;
  green = (s32)(sinf(f_FireworkHue + 2.0f + index) * 64.0f) + 127;
  blue = (s32)(sinf(f_FireworkHue + 4.0f + index) * 64.0f) + 127;
  for (index = 0; index != f_FireworkCount; index++) {
    if (f_Fireworks[index].isActive == TRUE) {
      fadeTimer = f_Fireworks[index].timer;
      /* Fades out from frame 40, flickering on odd frames */
      if (fadeTimer < 40) {
        alpha = 0xFF;
      } else {
        alpha = 0xFF - ((fadeTimer - 40) * 8);
        if (fadeTimer & 1) {
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
      for (i = 0; i != f_Fireworks[index].particleCount; i++) {
        drawnParticle = &f_Fireworks[index].particles[i];
        if (drawnParticle->life > 0) {
          gDPPipeSync(gfx++);
          gSPMatrix(gfx++,
              PhysicalAddress(&drawnParticle->matrices[g_MatrixBufferIndex]),
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
