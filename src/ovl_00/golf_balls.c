#include "common.h"
#include "asset.h"
#include "ball_simulation.h"
#include "billboard.h"
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

/* g_GameState.unk20, under a symbol of its own */
extern s32 D_801B60A8;

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

#ifdef NON_MATCHING
void UpdateGolfBalls(void) {
    s32 currentIndex;
    s32 i;
    UnkB8 *player;
    GolfBall *ball;
    BallSimulation *simulation;
    BallAfterimage *head;
    BallAfterimage *afterimage;
    BallAfterimage *previous;
    Mat4 scaling;
    Mat4 rotated;
    Mat4 translation;
    f32 scale;
    f32 distance;
    s8 markerPlayerIndex;

    currentIndex = 0;
    if (g_GameState.unk0 == GAME_STATE_UNK6) {
        currentIndex = D_801B60A8;
    }
    for (i = 0, player = g_Players, ball = sGolfBalls; i < g_GameState.playerCount; i++, player++, ball++) {
        if (player->unk26 != 0 || player->strokeCount == 0) {
            ball->alpha = BALL_HIDDEN;
        }
        if (currentIndex != i) {
            ball->state = BALL_STATE_STOPPED;
            if (GetSurfaceInfo((s8)player->ballSurface)->kind == SURFACE_KIND_GREEN) {
                ball->markerPlayerIndex = i;
            } else if (g_IsMiniGolf) {
                ball->markerPlayerIndex = i;
            } else {
                ball->markerPlayerIndex = BALL_NO_MARKER;
            }
        }
    }
    if (g_GameState.unk0 == GAME_STATE_UNK6 || g_GameState.unk0 == GAME_STATE_UNK11) {
        ball = &sGolfBalls[currentIndex];
        for (i = 0; i < BALL_SIMULATION_COUNT; i++) {
            simulation = &g_BallSimulations[i];
            ball->isInCup = FALSE;
            if (simulation->playerIndex >= 0 && g_Players[currentIndex].unk26 == 0) {
                if (g_IsCupViewShown == TRUE) {
                    ball->position[0] = (simulation->position[0] - (f32)(D_800BA9D8.cupPosition[0] - simulation->position[0])) * COURSE_TO_WORLD_SCALE;
                    ball->position[1] = (simulation->position[1] - (f32)(D_800BA9D8.cupPosition[1] - simulation->position[1])) * COURSE_TO_WORLD_SCALE;
                    ball->position[2] = (simulation->position[2] - (f32)(D_800BA9D8.cupPosition[2] - simulation->position[2])) * COURSE_TO_WORLD_SCALE;
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
                    simulation->isRunning == BALL_STATE_IN_CUP || simulation->unkA0 != 0) {
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
                    (GetSurfaceInfo(simulation->surface)->kind == SURFACE_KIND_GREEN || g_IsMiniGolf)) {
                    markerPlayerIndex = simulation->playerIndex;
                }
                ball->markerPlayerIndex = markerPlayerIndex;
                scale = g_PinVertexScale * BALL_MODEL_SCALE;
                guScaleF(scaling, scale, scale, scale);
                guMtxCatF(sBallRotation, scaling, rotated);
                guTranslateF(translation, ball->position[0],
                             ball->position[1] - (g_PinVertexScale * BALL_RADIUS) / 5120.0, ball->position[2]);
                MultiplyMatrices(rotated, translation, scaling);
                ConvertMatrixToFixed(scaling, &ball->matrices[g_DrawBufferIndex]);
            }
        }
    }
    if (sIsBallAfterimageShown == TRUE) {
        if (g_IsCupViewShown == TRUE) {
            if (!sIsBallAfterimageZoomed) {
                sIsBallAfterimageZoomed = TRUE;
                for (i = 0, afterimage = sBallAfterimages; i != sBallAfterimageLength; i++, afterimage++) {
                    if (afterimage->alpha != AFTERIMAGE_UNUSED) {
                        s32 coursePosition[3];

                        coursePosition[0] = afterimage->position[0] * 1024.0f;
                        coursePosition[1] = afterimage->position[1] * 1024.0f;
                        coursePosition[2] = afterimage->position[2] * 1024.0f;
                        afterimage->position[0] += afterimage->position[0] - D_800BA9D8.cupPosition[0] * COURSE_TO_WORLD_SCALE;
                        afterimage->position[1] += afterimage->position[1] - D_800BA9D8.cupPosition[1] * COURSE_TO_WORLD_SCALE;
                        afterimage->position[2] += afterimage->position[2] - D_800BA9D8.cupPosition[2] * COURSE_TO_WORLD_SCALE;
                    }
                }
            }
        } else if (sIsBallAfterimageZoomed == TRUE) {
            sIsBallAfterimageZoomed = FALSE;
            for (i = 0, afterimage = sBallAfterimages; i != sBallAfterimageLength; i++, afterimage++) {
                if (afterimage->alpha != AFTERIMAGE_UNUSED) {
                    afterimage->position[0] -= (afterimage->position[0] - D_800BA9D8.cupPosition[0] * COURSE_TO_WORLD_SCALE) * 0.5f;
                    afterimage->position[1] -= (afterimage->position[1] - D_800BA9D8.cupPosition[1] * COURSE_TO_WORLD_SCALE) * 0.5f;
                    afterimage->position[2] -= (afterimage->position[2] - D_800BA9D8.cupPosition[2] * COURSE_TO_WORLD_SCALE) * 0.5f;
                }
            }
        }
        head = &sBallAfterimages[sBallAfterimageHead];
        ball = &sGolfBalls[currentIndex];
        if (sBallAfterimageHead == 0) {
            previous = &sBallAfterimages[sBallAfterimageLength - 1];
        } else {
            previous = &sBallAfterimages[sBallAfterimageHead - 1];
        }
        {
            f32 offset[3];

            offset[0] = ball->position[0] - previous->position[0];
            offset[1] = ball->position[1] - previous->position[1];
            offset[2] = ball->position[2] - previous->position[2];
            distance = sqrtf(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
        }
        if (sIsBallAfterimageFast == TRUE) {
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
                sBallAfterimageHead++;
                if (sBallAfterimageHead == sBallAfterimageLength) {
                    sBallAfterimageHead = 0;
                }
            }
            for (i = 0, afterimage = sBallAfterimages; i != sBallAfterimageLength; i++, afterimage++) {
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
                sBallAfterimageHead++;
                if (sBallAfterimageHead == sBallAfterimageLength) {
                    sBallAfterimageHead = 0;
                }
            }
            if (sBallAfterimageFrameCount % AFTERIMAGE_SLOW_FADE_PERIOD == 0) {
                for (i = 0, afterimage = sBallAfterimages; i != sBallAfterimageLength; i++, afterimage++) {
                    if (afterimage->alpha > 0) {
                        afterimage->alpha--;
                    }
                }
            }
        }
        sBallAfterimageFrameCount++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/golf_balls", UpdateGolfBalls);
#endif

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

INCLUDE_ASM("asm/nonmatchings/ovl_00/golf_balls", DrawGolfBalls);
