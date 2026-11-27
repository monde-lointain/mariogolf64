/*
 * The fireworks that the turn starts in sudden death. Each one bursts from a
 * random point into the outline of a character, picked by the course.
 */

#include "round/fireworks.h"

#include <ultra64.h>

#include "audio/sound.h"
#include "camera/view_projection.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/round_hud.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"

// How many of them LoadFireworks sets up
#define FIREWORK_COUNT 1

// f_FireworkState's values: loaded while the fireworks' memory is allocated,
// and freed after FreeFireworks, below which HaveFireworksStayedFreed counts
// its calls down
#define FIREWORKS_LOADED 1
#define FIREWORKS_FREED 0
// The calls of HaveFireworksStayedFreed that return FALSE before TRUE
#define STAYED_FREED_CALL_COUNT 2

// Frames of a firework's timer at which it plays a sound and launches
#define FIREWORK_SOUND_FRAME 160
#define FIREWORK_LAUNCH_FRAME 220
// The fireworks' sounds play on this channel
#define FIREWORK_SOUND_CHANNEL 8
// Frames from StartFireworks to the first launch
#define FIREWORK_START_DELAY 70
// Frames between loaded fireworks' sounds; the first sounds at once
#define FIREWORK_SOUND_STAGGER 50
// The value written to a firework's ignoredLaunchValue
#define IGNORED_LAUNCH_VALUE 0xFF

// Below this life, a particle's life runs out five times as fast
#define PARTICLE_FADE_LIFE 128
#define PARTICLE_FADE_EXTRA_LIFE_LOSS 4

// A shape's cells with this value become particles
#define SHAPE_PARTICLE_CELL 0

// A particle's velocity keeps this much each frame, then gravity pulls it
#define PARTICLE_DRAG 0.94f
#define PARTICLE_GRAVITY 0.0005f
// A particle that has stopped points up, as if moving at this speed
#define STOPPED_PARTICLE_DIRECTION_Y 0.1f
#define STOPPED_PARTICLE_SPEED 0.5f

// After a launch, each particle's trail stretches from nothing to four times
// its speed over TRAIL_STRETCH_FRAMES, shrinks back at the same rate, and from
// TRAIL_REGROW_FRAME grows again from TRAIL_MIN_LENGTH, whatever the speed.
#define TRAIL_STRETCH_FRAMES 8
#define TRAIL_STRETCH_PER_FRAME 0.5f
#define TRAIL_STRETCH_MAX 4.0f
#define TRAIL_REGROW_FRAME 64
#define TRAIL_REGROWTH_PER_FRAME 0.04f
#define TRAIL_LENGTH_MIN 1.0f

// Each particle's triangle's scale and its depth in the orthographic view
#define PARTICLE_SCALE 0.1f
#define PARTICLE_DEPTH (-100.0f)

// The fireworks' color cycles through hues: each channel swings around the
// middle of its range, green and blue a fixed phase, in radians, ahead of red
#define HUE_STEP_DEGREES 3
#define COLOR_MIDDLE 127
#define COLOR_SWING 64.0f
#define GREEN_HUE_PHASE 2.0f
#define BLUE_HUE_PHASE 4.0f

// From FADE_START_FRAME after a launch, the fireworks fade, flickering
// brighter on odd frames
#define FADE_START_FRAME 40
#define FADE_ALPHA_PER_FRAME 8
#define FLICKER_ALPHA 0x18

// Launch positions and velocities are in the fireworks' local coordinates.
#define FIREWORK_LAUNCH_WIDTH 32
#define FIREWORK_LAUNCH_HEIGHT 64.0f
#define FIREWORK_LAUNCH_HEIGHT_JITTER 4
#define PARTICLE_SPEED_PER_CELL 0.1f
#define PARTICLE_VELOCITY_JITTER_STEPS 64
#define PARTICLE_VELOCITY_JITTER_STEP 0.0005f
#define PARTICLE_LAUNCH_DOWNWARD_SPEED 0.5
#define PARTICLE_LIFE_MIN 192
#define PARTICLE_LIFE_JITTER 128

// A random per-axis velocity offset.
#define RandomParticleVelocityJitter() \
    (((guRandom() % PARTICLE_VELOCITY_JITTER_STEPS) - (PARTICLE_VELOCITY_JITTER_STEPS / 2)) * \
        PARTICLE_VELOCITY_JITTER_STEP)

// An image whose zero cells each become a particle of a firework. It begins
// with an ImageHeader whose format is 4, one byte a cell, read here a byte at
// a time: only the low bytes of the width and height are read.
struct FireworkShape {
    u16 format;
    u8 widthHighByte;
    u8 width;
    u8 heightHighByte;
    u8 height;
    u16 unusedAfterHeight;
    u8 cells[TRAILING_ARRAY_LENGTH]; // width * height, by rows
};

struct FireworkParticle {
    Mtx matrices[MATRIX_BUFFER_COUNT];
    f32 x;
    f32 y;
    f32 velocityX;
    f32 velocityY;
    u8 unusedAfterVelocity[4];
    s16 life; // Drawn while above 0
};

static s32 f_FireworkCount = 0;
static s32 f_FireworkState = 0;
static Vtx f_FireworkParticleVertices[TRIANGLE_VERTEX_COUNT] = {
    {{{0, 32, 0}, 0, {0, 0}, {0xFF, 0x7F, 0x00, 0x7F}}},
    {{{-10, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xBF, 0xBF}}},
    {{{10, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xBF, 0xBF}}},
};
// The shapes, by course record index: outlines of Toad, a Koopa Troopa, a Shy
// Guy, Yoshi, a Boo, Wario, Mario and Peach
static AssetId f_FireworkShapeAssetIds[8] =
    {0x95F, 0x960, 0x961, 0x962, 0x963, 0x964, 0x965, 0x966};
static f32 f_FireworkHue = 0.0f;
static u8 f_UnusedBufferAfterFireworkHue[0xC04] UNUSED = {0};

FILE_STATIC FireworkShape *f_FireworkShape;
FILE_STATIC Firework f_Fireworks[FIREWORK_CAPACITY];

// Makes the first firework launch in FIREWORK_START_DELAY frames
void StartFireworks(void) {
    f_Fireworks[0].timer = FIREWORK_LAUNCH_FRAME - FIREWORK_START_DELAY;
    f_Fireworks[0].isActive = TRUE;
}

void StopFireworks(void) {
    f_Fireworks[0].isActive = FALSE;
}

// Loads the shape for the course, picked by its record index, and allocates
// the fireworks' particles
void LoadFireworks(CourseRecordIndex courseRecordIndex) {
    EncodedAsset asset;
    s32 index;
    Firework *firework;
    s32 width;
    s32 height;
    s32 row;
    s32 column;
    u8 *cell;

    f_FireworkShape = AllocMemory(OpenEncodedAsset(
        f_FireworkShapeAssetIds[courseRecordIndex % ARRAY_COUNT(f_FireworkShapeAssetIds)],
        &asset
    ));
    ReadEncodedAsset(f_FireworkShape, &asset);
    for (index = 0; index != FIREWORK_COUNT; index++) {
        f_Fireworks[index].ignoredLoadValue = 0;
        f_Fireworks[index].isActive = FALSE;
        f_Fireworks[index].timer = (FIREWORK_SOUND_FRAME - 1) - (index * FIREWORK_SOUND_STAGGER);
        f_Fireworks[index].ignoredLaunchValue = IGNORED_LAUNCH_VALUE;
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
                if (*cell == SHAPE_PARTICLE_CELL) {
                    firework->particleCount++;
                }
            }
        }

        firework->particles = AllocMemory(firework->particleCount * sizeof(FireworkParticle));
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

    f_FireworkState = FIREWORKS_FREED;
}

// Unless the fireworks are loaded, counts a call and returns TRUE from the
// third call after FreeFireworks on. Nothing calls it.
Boolean HaveFireworksStayedFreed(void) {
    if (f_FireworkState > FIREWORKS_FREED) {
        return FALSE;
    }

    f_FireworkState--;
    return f_FireworkState < FIREWORKS_FREED - STAYED_FREED_CALL_COUNT;
}

// The random part of a firework's launch height, 0 to 3
static inline s32 RandomLaunchHeightJitter(void) {
    return guRandom() % FIREWORK_LAUNCH_HEIGHT_JITTER;
}

// Sends every particle of the firework out from one random point
void LaunchFirework(Firework *firework, s32 fireworkIndex UNUSED) {
    s32 particleIndex;
    f32 originX;
    s32 heightJitter;
    f32 originY;
    s32 width;
    s32 height;
    s32 row;
    s32 column;

    particleIndex = 0;
    originX = guRandom() % FIREWORK_LAUNCH_WIDTH;
    heightJitter = RandomLaunchHeightJitter();
    width = firework->shape->width;
    height = firework->shape->height;
    firework->ignoredLaunchValue = IGNORED_LAUNCH_VALUE;
    originY = heightJitter + FIREWORK_LAUNCH_HEIGHT;
    for (row = 0; row != height; row++) {
        for (column = 0; column != width; column++) {
            if (*(firework->shape->cells + (column + (row * width))) == SHAPE_PARTICLE_CELL) {
                firework->particles[particleIndex].x = originX;
                firework->particles[particleIndex].y = originY;
                firework->particles[particleIndex].velocityX =
                    ((column - (width / 2)) * PARTICLE_SPEED_PER_CELL) +
                    RandomParticleVelocityJitter();
                firework->particles[particleIndex].velocityY =
                    (((height / 2) - row) * PARTICLE_SPEED_PER_CELL) -
                    PARTICLE_LAUNCH_DOWNWARD_SPEED + RandomParticleVelocityJitter();
                firework->particles[particleIndex].life =
                    (guRandom() % PARTICLE_LIFE_JITTER) + PARTICLE_LIFE_MIN;
                particleIndex++;
            }
        }
    }
}

// Moves the active fireworks' particles and draws each as a triangle along its
// path
void UpdateAndDrawFireworks(Gfx **displayListHead) {
    Gfx *gfx;
    s32 unused[12] UNUSED;
    Mat4 transform;
    s32 unusedAfter[6] UNUSED;
    // One index for the fireworks and the particles
    s32 i;
    s32 index;
    FireworkParticle *particle;
    FireworkParticle *drawnParticle;
    f32 directionX;
    f32 directionY;
    f32 inverseSpeed;
    // The particle's speed, until it scales into its trail's length
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
    if (f_FireworkState <= FIREWORKS_FREED) {
        return;
    }

    for (i = 0; i != f_FireworkCount; i++) {
        if (f_Fireworks[i].isActive == TRUE) {
            timer = f_Fireworks[i].timer + 1;
            f_Fireworks[i].timer = timer;
            if (timer == FIREWORK_SOUND_FRAME) {
                PlaySound(SOUND_FIREWORK_LEAD_IN, FIREWORK_SOUND_CHANNEL, SOUND_FULL_SCALE);
            }
            else if (timer == FIREWORK_LAUNCH_FRAME) {
                f_Fireworks[i].timer = 0;
                LaunchFirework(&f_Fireworks[i], i);
                PlaySound(SOUND_FIREWORK_LAUNCH, FIREWORK_SOUND_CHANNEL, SOUND_FULL_SCALE);
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
    transform[2][2] = PARTICLE_SCALE;
    transform[3][3] = 1.0f;
    for (index = 0; index != f_FireworkCount; index++) {
        if (f_Fireworks[index].isActive == TRUE) {
            for (i = 0; i != f_Fireworks[index].particleCount; i++) {
                particle = &f_Fireworks[index].particles[i];
                if (particle->life > 0) {
                    particle->x += particle->velocityX;
                    particle->y += particle->velocityY;
                    particle->velocityX *= PARTICLE_DRAG;
                    particle->velocityY = (particle->velocityY * PARTICLE_DRAG) - PARTICLE_GRAVITY;
                    if (--particle->life < PARTICLE_FADE_LIFE) {
                        particle->life -= PARTICLE_FADE_EXTRA_LIFE_LOSS;
                    }

                    length = sqrtf(
                        (particle->velocityX * particle->velocityX) +
                        (particle->velocityY * particle->velocityY)
                    );
                    if (length != 0.0f) {
                        inverseSpeed = 1.0f / length;
                        directionX = particle->velocityX * inverseSpeed;
                        directionY = particle->velocityY * inverseSpeed;
                    }
                    else {
                        directionX = 0.0f;
                        directionY = STOPPED_PARTICLE_DIRECTION_Y;
                        length = STOPPED_PARTICLE_SPEED;
                    }

                    trailTimer = f_Fireworks[index].timer;
                    if (trailTimer < TRAIL_STRETCH_FRAMES) {
                        length *= TRAIL_STRETCH_PER_FRAME * trailTimer;
                    }
                    else if (trailTimer > TRAIL_REGROW_FRAME) {
                        length = ((trailTimer - TRAIL_REGROW_FRAME) * TRAIL_REGROWTH_PER_FRAME) +
                            TRAIL_LENGTH_MIN;
                    }
                    else {
                        length *= TRAIL_STRETCH_MAX -
                            ((trailTimer - TRAIL_STRETCH_FRAMES) * TRAIL_STRETCH_PER_FRAME);
                    }

                    if (length < TRAIL_LENGTH_MIN) {
                        length = TRAIL_LENGTH_MIN;
                    }

                    transform[0][0] = -directionY * PARTICLE_SCALE;
                    transform[0][1] = directionX * PARTICLE_SCALE;
                    tailScale = length * PARTICLE_SCALE;
                    transform[1][0] = -directionX * tailScale;
                    transform[1][1] = -directionY * tailScale;
                    transform[3][0] = particle->x;
                    transform[3][1] = particle->y;
                    transform[3][2] = PARTICLE_DEPTH;
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
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_OFF);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        0,
        SHADE,
        0,
        PRIMITIVE,
        0,
        SHADE,
        0,
        PRIMITIVE,
        0,
        SHADE,
        0,
        PRIMITIVE,
        0,
        SHADE,
        0
    );
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    f_FireworkHue += DegreesToRadians(HUE_STEP_DEGREES);

    // index is f_FireworkCount here, so every firework shares one color
    red = (s32)(sinf(f_FireworkHue + index) * COLOR_SWING) + COLOR_MIDDLE;
    green = (s32)(sinf(f_FireworkHue + GREEN_HUE_PHASE + index) * COLOR_SWING) + COLOR_MIDDLE;
    blue = (s32)(sinf(f_FireworkHue + BLUE_HUE_PHASE + index) * COLOR_SWING) + COLOR_MIDDLE;
    for (index = 0; index != f_FireworkCount; index++) {
        if (f_Fireworks[index].isActive == TRUE) {
            fadeTimer = f_Fireworks[index].timer;
            if (fadeTimer < FADE_START_FRAME) {
                alpha = OPAQUE_ALPHA;
            }
            else {
                alpha = OPAQUE_ALPHA - ((fadeTimer - FADE_START_FRAME) * FADE_ALPHA_PER_FRAME);
                if (fadeTimer & 1) {
                    alpha += FLICKER_ALPHA;
                }
            }

            if (alpha < TRANSPARENT_ALPHA) {
                alpha = TRANSPARENT_ALPHA;
            }

            if (alpha > OPAQUE_ALPHA) {
                alpha = OPAQUE_ALPHA;
            }

            gDPSetPrimColor(gfx++, 0, 0, red, green, blue, alpha);
            for (i = 0; i != f_Fireworks[index].particleCount; i++) {
                drawnParticle = &f_Fireworks[index].particles[i];
                if (drawnParticle->life > 0) {
                    gDPPipeSync(gfx++);
                    gSPMatrix(
                        gfx++,
                        PhysicalAddress(&drawnParticle->matrices[g_MatrixBufferIndex]),
                        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                    );
                    gSPVertex(
                        gfx++,
                        f_FireworkParticleVertices,
                        ARRAY_COUNT(f_FireworkParticleVertices),
                        0
                    );
                    gSP1Triangle(gfx++, 0, 1, 2, 0);
                }
            }
        }
    }

    *displayListHead = gfx;
}
