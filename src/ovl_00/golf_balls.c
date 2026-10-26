#include "common.h"
#include "asset.h"
#include "ball_simulation.h"
#include "cpu_projection.h"
#include "debug_text.h"
#include "golf_balls.h"
#include "heap.h"
#include "pin.h"
#include "save.h"
#include "surface.h"
#include "variables.h"

/*
 * The golf balls on the course, one for each player, and the fading
 * afterimages that the ball in play leaves behind it.
 */

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
#define BALL_IMAGE_COUNT 5

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
    /* 0x80 */ SurfaceInfo *surface;
    /* 0x84 */ f32 position[3];
    /* 0x90 */ s32 alpha; /* Or BALL_HIDDEN */
    /* 0x94 */ u8 isInCup;
    /* 0x98 */ s32 state; /* A BALL_STATE_* value */
    /* 0x9C */ s8 markerPlayerIndex; /* Or BALL_NO_MARKER */
    /* 0x9D */ s8 purpose; /* A LAUNCH_PURPOSE_* value */
    /* 0x9E */ u8 unk9E;
} GolfBall; /* size = 0xA0 */

typedef struct BallAfterimage {
    /* 0x00 */ f32 position[3];
    /* 0x0C */ s32 alpha; /* Or AFTERIMAGE_UNUSED */
    /* 0x10 */ s32 isInCup;
} BallAfterimage; /* size = 0x14 */

/* Set while the camera shows the cup up close, at twice the distance from it */
extern s32 D_800BE69C;

extern GolfBall sGolfBalls[GOLF_BALL_COUNT];
extern BallAfterimage sBallAfterimages[AFTERIMAGE_MAX_LENGTH];
extern f32 sBallAfterimageOrigin[3];
/* The point that the next one is written over */
extern s32 sBallAfterimageHead;
extern s32 sIsBallAfterimageShown;
extern s32 sBallAfterimageFrameCount;
/* Whether the points are scaled for the close view of the cup */
extern s32 sIsBallAfterimageZoomed;
/* Set for the few afterimages that fade quickly */
extern s32 sIsBallAfterimageFast;
extern s32 sBallAfterimageLength;
extern u8 *sBallImages[BALL_IMAGE_COUNT];
extern u8 sBallImageHeap[BALL_IMAGE_HEAP_SIZE];
extern u16 sPikaBallTexture[];
/* How the ball has turned as it rolled */
extern Mat4 sBallRotation;
extern s32 sGolfBallUnk100;
/* The ball's position when last rolled */
extern s32 sLastRolledBallPosition[3];

void func_801FD5F0_ovl_00(void) {
}

void DrawPikaBallTexture(void) {
    DrawTextIntoTexture("PIKA", 4, sPikaBallTexture);
}

void LoadGolfBallImages(void) {
    EncodedAsset asset;

    ResetHeap(HEAP_BALL_IMAGES);
    AddHeapRegion(HEAP_BALL_IMAGES, sBallImageHeap, sBallImageHeap + BALL_IMAGE_HEAP_SIZE);
    sBallImages[0] = AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C7, &asset));
    ReadEncodedAsset(sBallImages[0], &asset);
    sBallImages[1] = AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C8, &asset));
    ReadEncodedAsset(sBallImages[1], &asset);
    sBallImages[2] = AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4DA, &asset));
    ReadEncodedAsset(sBallImages[2], &asset);
    sBallImages[3] = AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C6, &asset));
    ReadEncodedAsset(sBallImages[3], &asset);
    sBallImages[4] = AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x6BB, &asset));
    ReadEncodedAsset(sBallImages[4], &asset);
    osSyncPrintf("HEAP_BALLTEX LEFT%d\n", GetHeapFreeSize(HEAP_BALL_IMAGES));
    DrawPikaBallTexture();
    ResetGolfBalls();
    ResetBallRotation();
}

/* Turns the ball about the horizontal axis across the way that it moved */
void RollGolfBall(GolfBall *ball, BallSimulation *simulation) {
    Mat4 rotation;
    s32 movement[3];
    f32 distance;

    f32 GetLength(f32 x, f32 y, f32 z) {
        return sqrtf(x * x + y * y + z * z);
    }

    if (simulation->isRunning == BALL_STATE_FLYING || simulation->isRunning == BALL_STATE_ROLLING) {
        movement[0] = simulation->position[0] - sLastRolledBallPosition[0];
        movement[1] = simulation->position[1] - sLastRolledBallPosition[1];
        movement[2] = simulation->position[2] - sLastRolledBallPosition[2];
        sLastRolledBallPosition[0] = simulation->position[0];
        sLastRolledBallPosition[1] = simulation->position[1];
        sLastRolledBallPosition[2] = simulation->position[2];
    } else {
        movement[0] = simulation->velocity[0];
        movement[1] = simulation->velocity[1];
        movement[2] = simulation->velocity[2];
    }
    if (simulation->isRunning != BALL_STATE_STOPPED) {
        distance = GetLength(movement[0], movement[1], movement[2]);
        if (movement[2] != 0 || movement[0] != 0) {
            guRotateF(rotation, distance * BALL_ROLL_DEGREES_PER_UNIT, -movement[2], 0.0f, movement[0]);
            guMtxCatF(sBallRotation, rotation, sBallRotation);
        }
    }
}

void ResetBallAfterimage(void) {
    s32 i;

    for (i = 0; i != AFTERIMAGE_MAX_LENGTH; i++) {
        sBallAfterimages[i].alpha = AFTERIMAGE_UNUSED;
        sBallAfterimages[i].isInCup = FALSE;
    }
    sBallAfterimages[0].position[0] = sGolfBalls[g_GameState.unk20].position[0];
    sBallAfterimages[0].position[1] = sGolfBalls[g_GameState.unk20].position[1];
    sBallAfterimages[0].position[2] = sGolfBalls[g_GameState.unk20].position[2];
    sBallAfterimageOrigin[0] = sGolfBalls[g_GameState.unk20].position[0];
    sBallAfterimageOrigin[1] = sGolfBalls[g_GameState.unk20].position[1];
    sBallAfterimageHead = 0;
    sIsBallAfterimageShown = FALSE;
    sBallAfterimageFrameCount = 0;
    sIsBallAfterimageZoomed = FALSE;
    sBallAfterimageOrigin[2] = sGolfBalls[g_GameState.unk20].position[2];
}

void StartFastBallAfterimage(void) {
    sIsBallAfterimageShown = TRUE;
    sIsBallAfterimageFast = TRUE;
    sBallAfterimageLength = AFTERIMAGE_FAST_LENGTH;
}

void ResetBallRotation(void) {
    guMtxIdentF(sBallRotation);
}

void ResetGolfBalls(void) {
    s32 i;

    for (i = 0; i < GOLF_BALL_COUNT; i++) {
        sGolfBalls[i].alpha = BALL_HIDDEN;
        sGolfBalls[i].state = BALL_STATE_STOPPED;
    }
    ResetBallAfterimage();
    ResetBallRotation();
    sGolfBallUnk100 = 0;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/golf_balls", func_801FDA64_ovl_00);

s32 IsAnyBallInCup(void) {
    s32 isInCup = FALSE;
    s32 i;

    for (i = 0; i != GOLF_BALL_COUNT; i++) {
        if (sGolfBalls[i].isInCup == TRUE && sGolfBalls[i].alpha != BALL_HIDDEN) {
            isInCup = TRUE;
        }
    }
    return isInCup;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/golf_balls", func_801FE370_ovl_00);
