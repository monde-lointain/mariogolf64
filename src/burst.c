#include "common.h"
#include "asset.h"
#include "cpu_projection.h"
#include "functions.h"
#include "heap.h"

/* How many bursts sBursts has room for */
#define BURST_CAPACITY 4

/* How many of them LoadBursts sets up */
#define BURST_COUNT 1

/* sBurstState's value while the bursts' memory is allocated */
#define BURSTS_LOADED 1

/* Frames of a burst's timer at which it plays a sound and launches */
#define BURST_SOUND_FRAME 160
#define BURST_LAUNCH_FRAME 220

/* Below this life, a particle's life runs out five times as fast */
#define PARTICLE_FADE_LIFE 128

#define DEGREES_TO_RADIANS(degrees) ((degrees) * (M_PI / 180))

/* An image whose zero cells each become a particle of a burst */
typedef struct BurstShape {
    /* 0x0 */ u8 unk0[3];
    /* 0x3 */ u8 width;
    /* 0x4 */ u8 unk4;
    /* 0x5 */ u8 height;
    /* 0x6 */ u8 unk6[2];
    /* 0x8 */ u8 cells[1]; /* width * height, by rows */
} BurstShape;

typedef struct BurstParticle {
    /* 0x00 */ Mtx matrices[2]; /* Indexed by g_MatrixBufferIndex */
    /* 0x80 */ f32 x;
    /* 0x84 */ f32 y;
    /* 0x88 */ f32 velocityX;
    /* 0x8C */ f32 velocityY;
    /* 0x90 */ u8 unk90[4];
    /* 0x94 */ s16 life; /* Drawn while above 0 */
} BurstParticle; /* size = 0x98 */

typedef struct Burst {
    /* 0x00 */ BurstParticle *particles;
    /* 0x04 */ s32 particleCount;
    /* 0x08 */ s32 timer;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u8 unk10[0x14];
    /* 0x24 */ u8 unk24;
    /* 0x25 */ u8 isActive;
    /* 0x28 */ BurstShape *shape;
} Burst; /* size = 0x2C */

extern s32 sBurstCount;
extern s32 sBurstState;
extern Vtx sBurstParticleVertices[3];
extern u16 sBurstShapeAssetIds[8];
extern f32 sBurstHue;
extern BurstShape *sBurstShape;
extern Burst sBursts[BURST_CAPACITY];
extern s32 g_MatrixBufferIndex;
extern Mtx g_ScreenOrthoMatrix;

void func_80050BF0(s32, s32, s32);

/* Makes the first burst launch in 70 frames */
void StartBursts(void) {
    sBursts[0].timer = 150;
    sBursts[0].isActive = TRUE;
}

void StopBursts(void) {
    sBursts[0].isActive = FALSE;
}

/* Loads the shape for the course record and allocates the bursts' particles */
#ifdef NON_MATCHING
void LoadBursts(s32 courseRecordIndex) {
    EncodedAsset asset;
    s32 index;
    Burst *burst;
    s32 width;
    s32 height;
    s32 row;
    s32 column;

    sBurstShape = AllocMemory(OpenEncodedAsset(
        sBurstShapeAssetIds[courseRecordIndex % ARRAY_COUNT(sBurstShapeAssetIds)], &asset));
    ReadEncodedAsset(sBurstShape, &asset);
    for (index = 0; index != BURST_COUNT; index++) {
        sBursts[index].unk24 = 0;
        sBursts[index].isActive = FALSE;
        sBursts[index].timer = 159 - index * 50;
        sBursts[index].unkC = 0xFF;
        sBursts[index].particleCount = 0;
        sBursts[index].shape = sBurstShape;
    }
    sBurstCount = index;
    for (index = 0; index != sBurstCount; index++) {
        burst = &sBursts[index];
        width = burst->shape->width;
        height = burst->shape->height;
        for (row = 0; row != height; row++) {
            for (column = 0; column != width; column++) {
                if (burst->shape->cells[column + row * width] == 0) {
                    burst->particleCount++;
                }
            }
        }
        burst->particles = AllocMemory(burst->particleCount * sizeof(BurstParticle));
        for (row = 0; row != burst->particleCount; row++) {
            burst->particles[row].life = 0;
        }
    }
    sBurstState = BURSTS_LOADED;
}

#else
INCLUDE_ASM("asm/nonmatchings/burst", LoadBursts);
#endif

void FreeBursts(void) {
    s32 index;

    if (sBurstState == BURSTS_LOADED) {
        FreeMemory(&sBurstShape);
        for (index = 0; index != sBurstCount; index++) {
            FreeMemory(&sBursts[index].particles);
        }
    }
    sBurstState = 0;
}

/*
 * Unless the bursts are loaded, counts a call and returns TRUE from the
 * third call after FreeBursts on. Nothing calls it.
 */
s32 HaveBurstsStayedFreed(void) {
    if (sBurstState > 0) {
        return FALSE;
    }
    sBurstState--;
    return sBurstState < -2;
}

/* Sends every particle of the burst out from one random point */
#ifdef NON_MATCHING
void LaunchBurst(Burst *burst, s32 burstIndex) {
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
    width = burst->shape->width;
    height = burst->shape->height;
    burst->unkC = 0xFF;
    originY = random + 64.0f;
    halfWidth = width / 2;
    for (row = 0; row != height; row++) {
        for (column = 0; column != width; column++) {
            if (burst->shape->cells[column + row * width] == 0) {
                burst->particles[particleIndex].x = originX;
                burst->particles[particleIndex].y = originY;
                burst->particles[particleIndex].velocityX =
                    (column - halfWidth) * 0.1f + (guRandom() % 64 - 32) * 0.0005f;
                burst->particles[particleIndex].velocityY =
                    (s32)(height / 2 - row) * 0.1f - 0.5 + (guRandom() % 64 - 32) * 0.0005f;
                burst->particles[particleIndex].life = guRandom() % 128 + 192;
                particleIndex++;
            }
        }
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/burst", LaunchBurst);
#endif

/* Moves the active bursts' particles and draws each as a triangle along its path */
#ifdef NON_MATCHING
void UpdateAndDrawBursts(Gfx **displayListHead) {
    Gfx *gfx;
    s32 unused[12]; /* The original frame has room for this */
    Mat4 transform;
    s32 unusedAfter[6]; /* The original frame has room for this too */
    s32 burstIndex;
    s32 index;
    s32 particleIndex;
    BurstParticle *particle;
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
    if (sBurstState <= 0) {
        return;
    }
    for (burstIndex = 0; burstIndex != sBurstCount; burstIndex++) {
        if (sBursts[burstIndex].isActive == TRUE) {
            timer = sBursts[burstIndex].timer + 1;
            sBursts[burstIndex].timer = timer;
            if (timer == BURST_SOUND_FRAME) {
                func_80050BF0(0x31, 8, 0x7F);
            } else if (timer == BURST_LAUNCH_FRAME) {
                sBursts[burstIndex].timer = 0;
                LaunchBurst(&sBursts[burstIndex], burstIndex);
                func_80050BF0(0x32, 8, 0x7F);
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
    for (index = 0; index != sBurstCount; index++) {
        if (sBursts[index].isActive == TRUE) {
            for (particleIndex = 0; particleIndex != sBursts[index].particleCount; particleIndex++) {
                particle = &sBursts[index].particles[particleIndex];
                if (particle->life > 0) {
                    particle->x += particle->velocityX;
                    particle->y += particle->velocityY;
                    particle->velocityX *= 0.94f;
                    particle->velocityY = particle->velocityY * 0.94f - 0.0005f;
                    if (--particle->life < PARTICLE_FADE_LIFE) {
                        particle->life -= 4;
                    }
                    speed = sqrtf(particle->velocityX * particle->velocityX
                                  + particle->velocityY * particle->velocityY);
                    directionX = 0.0f;
                    if (speed != 0.0f) {
                        inverseSpeed = 1.0f / speed;
                        directionX = particle->velocityX * inverseSpeed;
                        directionY = particle->velocityY * inverseSpeed;
                    } else {
                        directionY = 0.1f;
                        speed = 0.5f;
                    }
                    timer = sBursts[index].timer;
                    if (timer < 8) {
                        length = speed * 0.5f * timer;
                    } else if (timer > 64) {
                        length = (timer - 64) * 0.04f + 1.0f;
                    } else {
                        length = speed * (4.0f - (timer - 8) * 0.5f);
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
                    ConvertMatrixToFixed(transform, &particle->matrices[g_MatrixBufferIndex]);
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
    gDPSetCombineLERP(gfx++, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0,
                      SHADE, 0);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    sBurstHue += DEGREES_TO_RADIANS(3);
    /* index is sBurstCount here, so every burst shares one color */
    red = (s32)(sinf(sBurstHue + index) * 64.0f) + 127;
    green = (s32)(sinf(sBurstHue + 2.0f + index) * 64.0f) + 127;
    blue = (s32)(sinf(sBurstHue + 4.0f + index) * 64.0f) + 127;
    for (index = 0; index != sBurstCount; index++) {
        if (sBursts[index].isActive == TRUE) {
            timer = sBursts[index].timer;
            alpha = 0xFF;
            /* Fades out from frame 40, flickering on odd frames */
            if (timer >= 40) {
                alpha = 0xFF - (timer - 40) * 8;
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
            for (particleIndex = 0; particleIndex != sBursts[index].particleCount; particleIndex++) {
                particle = &sBursts[index].particles[particleIndex];
                if (particle->life > 0) {
                    gDPPipeSync(gfx++);
                    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&particle->matrices[g_MatrixBufferIndex]),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPVertex(gfx++, sBurstParticleVertices, ARRAY_COUNT(sBurstParticleVertices), 0);
                    gSP1Triangle(gfx++, 0, 1, 2, 0);
                }
            }
        }
    }
    *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/burst", UpdateAndDrawBursts);
#endif
