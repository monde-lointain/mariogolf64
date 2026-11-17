/*
 * Loads holes on the loader thread: decompressing their data, shrinking mini-
 * golf holes, choosing the wind, the rain and the scene colors, and setting up
 * the hole's state.
 */

#include "course/hole_loader.h"

#include <nusys.h>
#include <stdlib.h>
#include <ultra64.h>

#include "camera/shot_camera.h"
#include "characters/character_model.h"
#include "course/billboard.h"
#include "course/course.h"
#include "course/hole_builder.h"
#include "course/hole_conditions.h"
#include "course/hole_setup.h"
#include "course/hole_triangles.h"
#include "course/lz_decoder.h"
#include "course/terrain.h"
#include "course/work_area.h"
#include "debug/game_flags.h"
#include "game/scene.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/game_state.h"
#include "round/play.h"
#include "round/round_hud.h"
#include "round/scoring.h"
#include "round/turn.h"
#include "save/play_record.h"
#include "shot/ball_simulation.h"
#include "shot/current_shot.h"
#include "system/big_endian.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "system/thread_wait.h"

// ChooseHoleConditions gives up on finding a set cup position after these
#define CUP_ATTEMPT_COUNT_MAX 120
// Whether the hole has both its cup and its tee set
#define AreCupAndTeeSet() \
    (g_HoleSetup.cupPosition[AXIS_X] != 0 && \
        g_HoleSetup.cupPosition[AXIS_Z] != 0 && \
        g_HoleSetup.teePosition[AXIS_X] != 0 && \
        g_HoleSetup.teePosition[AXIS_Z] != 0)
// A hole's cup is drawn from (GetHoleRandom() >> CUP_DRAW_SHIFT) %
// CUP_DRAW_COUNT; a draw below an end picks that end's cup
#define CUP_DRAW_SHIFT 3
#define CUP_DRAW_COUNT 9
#define CUP_DRAW_FIRST_END 3
#define CUP_DRAW_SECOND_END 6
#define CUP_DRAW_THIRD_END 8

// A hole rains when rand() & RAIN_DRAW_MASK is DRY_DRAW_COUNT or more
#define RAIN_DRAW_MASK 3
#define DRY_DRAW_COUNT 3

// Wind speeds come in steps; lighter winds than MIN_WIND_SPEED are calm
#define WIND_SPEED_STEP 10000
#define WIND_SPEED_MIN 0x18000
#define DRIVING_RANGE_WIND_DIRECTION 0x4000

// The light turns this far, in binary angles, from each hole to the next
#define LIGHT_YAW_STEP_PER_HOLE 0x5000
#define HOLE_LIGHT_PITCH 0x3000

// A mini-golf course is a main course's hole shrunk by these, by axis
#define MINI_GOLF_SHRINK_X 4
#define MINI_GOLF_SHRINK_Y 4
#define MINI_GOLF_SHRINK_Z 8
// How much ShrinkMiniGolfHole raises the top corners of shaded triangles, and
// lowers the others
#define MINI_GOLF_SHADED_RAISE 30.0f
#define MINI_GOLF_SHADED_DROP 6.0f

// The loader thread waits this many frames after the ground was last drawn
// before loading
#define LOADER_FRAME_DELAY 3

#define LOADER_THREAD_ID 10
#define LOADER_THREAD_PRIORITY 20

// The Koopa Park hole whose high ground is not faded
#define KOOPA_PARK_UNFADED_HOLE_INDEX 7
// Whether a course's high ground fades partway toward white; Boo Valley's
// fades fully
#define IsHighGroundFadedCourse(courseRecordIndex) \
    ((courseRecordIndex) == COURSE_RECORD_TOAD_HIGHLANDS || \
        (courseRecordIndex) == COURSE_RECORD_KOOPA_PARK || \
        (courseRecordIndex) == COURSE_RECORD_YOSHIS_ISLAND || \
        (courseRecordIndex) == COURSE_RECORD_MARIOS_STAR)

#define LZ_BUFFER_COUNT 2
#define LZ_CHUNK_SIZE 0x1000
#define LZ_CARRY_SIZE 0x20
#define LZ_RAW_BUFFER_SIZE 0x1040
#define PATCHED_LZ_CHUNK_SIZE 0x800
#define PATCHED_LZ_RAW_BUFFER_SIZE 0x840
// The DMA buffers' alignment, in bytes
#define LZ_BUFFER_ALIGNMENT 16
#define AlignedLzBuffer(address) \
    ((AddressNumber(address) + LZ_BUFFER_ALIGNMENT - 1) & ~(LZ_BUFFER_ALIGNMENT - 1))

static s32 f_HoleSeed = 0;
static s32 f_CycledCupIndex = 0;
// hole_loader.h declares this without volatile, as the files that only read
// it were compiled; the loader thread waits on it, so here it is volatile
volatile Boolean g_HasLoaderFinished = TRUE;
static s32 f_UnusedWordAfterLoaderFinished UNUSED = 0;
static OSThread f_LoaderThread;
HolePoints g_HolePoints RCP_ALIGNED;
s32 g_HoleTimeFrames[COURSE_HOLE_COUNT];
s32 g_HoleTimeHundredths[COURSE_HOLE_COUNT];
s32 g_HoleTimeMinutes[COURSE_HOLE_COUNT];
s32 g_HoleTimeSeconds[COURSE_HOLE_COUNT];

// Decompresses the size bytes at romAddress into destination with
// DecodeLzChunk, reading the ROM in chunks through two DMA buffers so that
// one fills while the other is decoded. Returns the decompressed size.
s32 LoadLzData(u32 romAddress, void *destination, u32 size) {
    OSIoMesg ioMesgs[LZ_BUFFER_COUNT];
    OSMesgQueue queue;
    OSMesg mesgs[LZ_BUFFER_COUNT];
    LzStream stream;
    u8 rawBuffers[LZ_BUFFER_COUNT][LZ_RAW_BUFFER_SIZE];
    u16 *buffers[LZ_BUFFER_COUNT];
    s32 dmaIndex;
    s32 receiveIndex;
    s32 pendingCount;
    s32 receivedCount;
    s32 otherIndex;
    u32 chunkSize;
    s32 decodedSize;

    receivedCount = 0;
    dmaIndex = 0;
    pendingCount = 0;
    receiveIndex = 0;
    buffers[0] = (u16 *)(AlignedLzBuffer(rawBuffers[0]) + LZ_CARRY_SIZE);
    buffers[1] = (u16 *)(AlignedLzBuffer(rawBuffers[1]) + LZ_CARRY_SIZE);
    stream.input = buffers[0];
    stream.outputStart = destination;
    stream.nextInput = buffers[1];
    stream.flags = LZ_STREAM_FLAG_FIRST;
    osCreateMesgQueue(&queue, mesgs, LZ_BUFFER_COUNT);
    while (TRUE) {
        if (size != 0) {
            chunkSize = size;
            if (size > LZ_CHUNK_SIZE) {
                chunkSize = LZ_CHUNK_SIZE;
            }

            ioMesgs[dmaIndex].hdr.pri = OS_MESG_PRI_NORMAL;
            ioMesgs[dmaIndex].hdr.retQueue = &queue;
            ioMesgs[dmaIndex].dramAddr = buffers[dmaIndex];
            ioMesgs[dmaIndex].devAddr = romAddress;
            ioMesgs[dmaIndex].size = chunkSize;
            size -= chunkSize;
            romAddress += chunkSize;
            pendingCount++;
            osInvalDCache(buffers[dmaIndex], chunkSize);
            osEPiStartDma(nuPiCartHandle, &ioMesgs[dmaIndex], OS_READ);
            dmaIndex ^= 1;
        }

        if (osRecvMesg(&queue, &mesgs[receiveIndex], OS_MESG_NOBLOCK) != -1) {
            receiveIndex ^= 1;
            receivedCount++;
            pendingCount--;
        }

        if (receivedCount == 0) {
            if (pendingCount == LZ_BUFFER_COUNT || size == 0) {
                osRecvMesg(&queue, &mesgs[receiveIndex], OS_MESG_BLOCK);
                receiveIndex ^= 1;
                receivedCount = 1;
                pendingCount--;
            }

            if (receivedCount == 0) {
                continue;
            }
        }

        otherIndex = receiveIndex ^ 1;
        osInvalDCache(buffers[otherIndex], ioMesgs[otherIndex].size);
        stream.nextInput = buffers[receiveIndex];
        stream.inputEnd = buffers[otherIndex] + (LZ_CHUNK_SIZE / sizeof(u16));
        if (pendingCount == 0 && size == 0) {
            stream.flags |= LZ_STREAM_FLAG_LAST;
        }

        decodedSize = DecodeLzChunk(&stream);
        receivedCount--;
        if (decodedSize >= 0) {
            break;
        }

        stream.flags &= ~LZ_STREAM_FLAG_FIRST;
    }

    osWritebackDCache(destination, decodedSize);
    // What it unpacked is the ROM's data, in the ROM's order
    MarkRomOrder(destination, decodedSize);
    return decodedSize;
}

// LoadLzData for data compressed for DecodePatchedLzChunk
s32 LoadPatchedLzData(u32 romAddress, void *destination, u32 size) {
    OSIoMesg ioMesgs[LZ_BUFFER_COUNT];
    OSMesgQueue queue;
    OSMesg mesgs[LZ_BUFFER_COUNT];
    LzStream stream;
    u8 rawBuffers[LZ_BUFFER_COUNT][PATCHED_LZ_RAW_BUFFER_SIZE];
    u16 *buffers[LZ_BUFFER_COUNT];
    u16 window[LZ_WINDOW_MASK + 1];
    u16 history[LZ_HISTORY_MASK + 1];
    s32 dmaIndex;
    s32 receiveIndex;
    s32 pendingCount;
    s32 receivedCount;
    s32 otherIndex;
    u32 chunkSize;
    s32 decodedSize;

    receivedCount = 0;
    dmaIndex = 0;
    pendingCount = 0;
    receiveIndex = 0;
    buffers[0] = (u16 *)(AlignedLzBuffer(rawBuffers[0]) + LZ_CARRY_SIZE);
    buffers[1] = (u16 *)(AlignedLzBuffer(rawBuffers[1]) + LZ_CARRY_SIZE);
    stream.input = buffers[0];
    stream.outputStart = destination;
    stream.nextInput = buffers[1];
    stream.flags = LZ_STREAM_FLAG_FIRST;
    stream.window = window;
    stream.history = history;
    osCreateMesgQueue(&queue, mesgs, LZ_BUFFER_COUNT);
    while (TRUE) {
        if (size != 0) {
            chunkSize = size;
            if (size > PATCHED_LZ_CHUNK_SIZE) {
                chunkSize = PATCHED_LZ_CHUNK_SIZE;
            }

            ioMesgs[dmaIndex].hdr.pri = OS_MESG_PRI_NORMAL;
            ioMesgs[dmaIndex].hdr.retQueue = &queue;
            ioMesgs[dmaIndex].dramAddr = buffers[dmaIndex];
            ioMesgs[dmaIndex].devAddr = romAddress;
            ioMesgs[dmaIndex].size = chunkSize;
            size -= chunkSize;
            romAddress += chunkSize;
            pendingCount++;
            osInvalDCache(buffers[dmaIndex], chunkSize);
            osEPiStartDma(nuPiCartHandle, &ioMesgs[dmaIndex], OS_READ);
            dmaIndex ^= 1;
        }

        if (osRecvMesg(&queue, &mesgs[receiveIndex], OS_MESG_NOBLOCK) != -1) {
            receiveIndex ^= 1;
            receivedCount++;
            pendingCount--;
        }

        if (receivedCount == 0) {
            if (pendingCount == LZ_BUFFER_COUNT || size == 0) {
                osRecvMesg(&queue, &mesgs[receiveIndex], OS_MESG_BLOCK);
                receiveIndex ^= 1;
                receivedCount = 1;
                pendingCount--;
            }

            if (receivedCount == 0) {
                continue;
            }
        }

        otherIndex = receiveIndex ^ 1;
        osInvalDCache(buffers[otherIndex], ioMesgs[otherIndex].size);
        stream.nextInput = buffers[receiveIndex];
        stream.inputEnd = buffers[otherIndex] + (PATCHED_LZ_CHUNK_SIZE / sizeof(u16));
        if (pendingCount == 0 && size == 0) {
            stream.flags |= LZ_STREAM_FLAG_LAST;
        }

        decodedSize = DecodePatchedLzChunk(&stream);
        receivedCount--;
        if (decodedSize >= 0) {
            break;
        }

        stream.flags &= ~LZ_STREAM_FLAG_FIRST;
    }

    osWritebackDCache(destination, decodedSize);
    // What it unpacked is the ROM's data, in the ROM's order
    MarkRomOrder(destination, decodedSize);
    return decodedSize;
}

// A pseudo-random number that is the same each time for a seed and a hole
s32 GetHoleRandom(s32 seed, s32 holeIndex) {
    void SetHoleSeed(s32 newSeed) {
        f_HoleSeed = newSeed;
    }

    s32 NextHoleRandom(void) {
        return f_HoleSeed = NextRandomSeed(f_HoleSeed);
    }

    SetHoleSeed(holeIndex);
    SetHoleSeed(seed + NextHoleRandom());
    return NextHoleRandom();
}

// On a mini-golf course, scales the hole's points down with g_MiniGolfScale
void ShrinkMiniGolfHolePoints(void) {
    s32 axis;
    s32 index;

    if (g_GameState.isMiniGolf) {
        g_MiniGolfScale[AXIS_X] = 1.0f / MINI_GOLF_SHRINK_X;
        g_MiniGolfScale[AXIS_Y] = 1.0f / MINI_GOLF_SHRINK_Y;
        g_MiniGolfScale[AXIS_Z] = 1.0f / MINI_GOLF_SHRINK_Z;
        for (axis = 0; axis < AXIS_COUNT; axis++) {
            for (index = 0; index < CUP_POSITION_COUNT; index++) {
                g_HolePoints.cupPositions[index][axis] *= g_MiniGolfScale[axis];
                g_HolePoints.teePositions[index][axis] *= g_MiniGolfScale[axis];
            }

            for (index = 0; index < ARRAY_COUNT(g_HolePoints.aimPoints); index++) {
                g_HolePoints.aimPoints[index][axis] *= g_MiniGolfScale[axis];
                g_HolePoints.routePoints[index][axis] *= g_MiniGolfScale[axis];
            }
        }
    }
}

// Reads the current hole's asset files, then shrinks its points for mini-golf
void LoadHole(void) {
    osSyncPrintf(
        "putter%d:course%d:hole%d\n",
        g_GameState.isMiniGolf,
        g_GameState.courseIndex,
        g_GameState.holeIndex
    );
    BuildHole(GetCourseRecordIndex(), GetHoleRecordIndex());
    ShrinkMiniGolfHolePoints();
}

// Whether ShrinkMiniGolfHole raises a shaded triangle's vertex, rather
// than drops it. An odd triangle's last two vertices are raised, and an
// even triangle's last one. It is a macro, as an inline function compiles
// differently from the original.
#define IsRaisedShadedVertex(index, vertexIndex) \
    ((((index) & 1) && (((vertexIndex) == 1) || ((vertexIndex) == 2))) || \
        (!((index) & 1) && ((vertexIndex) == 2)))

// On a mini-golf course, scales the hole's ground, billboards and triangles
// down with g_MiniGolfScale
void ShrinkMiniGolfHole(void) {
    Vtx_t point;
    s32 row;
    s32 column;
    s32 index;
    s32 vertexIndex;
    Vtx *vertex;

    if (g_GameState.isMiniGolf) {
        g_MiniGolfScale[AXIS_X] = 1.0f / MINI_GOLF_SHRINK_X;
        g_MiniGolfScale[AXIS_Y] = 1.0f / MINI_GOLF_SHRINK_Y;
        g_MiniGolfScale[AXIS_Z] = 1.0f / MINI_GOLF_SHRINK_Z;
        for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
            for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
                point = GetGroundPoint(column, row)->v;
                point.ob[AXIS_X] /= MINI_GOLF_SHRINK_X;
                point.ob[AXIS_Y] /= MINI_GOLF_SHRINK_Y;
                point.ob[AXIS_Z] /= MINI_GOLF_SHRINK_Z;
                SetGroundPoint(column, row, &point);
            }
        }

        MeasureGroundTiles();
        for (index = 0; index < BILLBOARD_COUNT; index++) {
            if (g_Billboards[index].textureIndex >= 0) {
                g_Billboards[index].x *= g_MiniGolfScale[AXIS_X];
                g_Billboards[index].y *= g_MiniGolfScale[AXIS_Y];
                g_Billboards[index].z *= g_MiniGolfScale[AXIS_Z];
                g_Billboards[index].width *= g_MiniGolfScale[AXIS_X];
                g_Billboards[index].height *= g_MiniGolfScale[AXIS_Y];
            }
        }

        for (index = 0; index < HOLE_TRIANGLE_COUNT; index++) {
            for (vertexIndex = 0; vertexIndex < TRIANGLE_VERTEX_COUNT; vertexIndex++) {
                vertex = g_WorkArea.hole.triangles[index].vertices;
                vertex[vertexIndex].v.ob[AXIS_X] *= g_MiniGolfScale[AXIS_X];
                vertex[vertexIndex].v.ob[AXIS_Y] *= g_MiniGolfScale[AXIS_Y];
                vertex[vertexIndex].v.ob[AXIS_Z] *= g_MiniGolfScale[AXIS_Z];
                if (g_WorkArea.hole.triangles[index].kind == HOLE_TRIANGLE_KIND_SHADED) {
                    if (IsRaisedShadedVertex(index, vertexIndex)) {
                        vertex[vertexIndex].v.ob[AXIS_Y] += MINI_GOLF_SHADED_RAISE;
                    }
                    else {
                        vertex[vertexIndex].v.ob[AXIS_Y] -= MINI_GOLF_SHADED_DROP;
                    }
                }
            }
        }

        PrepareHoleTriangles();
        PlaceBillboards();
    }
}

void PrepareHoleScene(void) {
    s32 shadeVariant;

    g_UnreadGroundQueryMarker = 1;
    switch (g_HoleSetup.conditions.sceneColorIndex) {
        case SCENE_COLOR_INDEX_ROUND_START:
            shadeVariant = 2;
            break;
        case SCENE_COLOR_INDEX_ROUND_END:
#ifdef AVOID_UB
        default:
#endif
            shadeVariant = 0;
            break;
        case SCENE_COLOR_INDEX_DRIVING_RANGE:
            shadeVariant = 1;
            break;
    }

    if (!IsGameFlagSet(GAME_FLAG_UNSHADED_GROUND)) {
        ShadeGround(shadeVariant);
    }

    g_UnreadGroundQueryMarker = 0;
    ToggleGroundTileBorders();
    SetCharacterLights();
    ShrinkMiniGolfHole();
}

// Chooses the wind, the rain and the scene's colors for the hole about to be
// played, then its cup from the hole's cup positions. In some scenes and modes
// the cup comes from g_RingShotRecords or g_PlayRecords instead.
NO_RESULT ChooseHoleConditions(void) {
    s32 cupIndex;
    s32 attemptCount;
    u32 cupDraw;
    CourseRecordIndex courseRecordIndex;

    g_TurnBallSimulation.playerIndex = -1;
    if (g_Scene == SCENE_PLAY_BEST_SHOT_REPLAY ||
        g_GameState.gameMode == GAME_MODE_RECORD_REPLAY ||
        g_GameState.replayKind != REPLAY_KIND_NONE) {
        if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            g_HoleSetup = g_RingShotRecords[0].hole;
        }
        else {
            g_HoleSetup = g_PlayRecords[0].hole;
        }

        return;
    }

    if (g_GameState.isRoundContinued) {
        return;
    }

    if (g_GameState.gameMode != GAME_MODE_TITLE_HOLE &&
        g_GameState.gameMode != GAME_MODE_STAFF_ROLL) {
        // The first hole, mini-golf and GAME_MODE_TRAINING stay dry
        if (g_GameState.holeIndex != 0 &&
            g_GameState.gameMode != GAME_MODE_TRAINING &&
            !g_GameState.isMiniGolf) {
            if ((rand() & RAIN_DRAW_MASK) < DRY_DRAW_COUNT) {
                g_HoleSetup.conditions.isRaining = FALSE;
            }
            else {
                g_HoleSetup.conditions.isRaining = TRUE;
            }

            if (g_GameState.courseIndex == COURSE_SHY_GUY_DESERT &&
                (rand() & RAIN_DRAW_MASK) < DRY_DRAW_COUNT) {
                g_HoleSetup.conditions.isRaining = FALSE;
            }
        }
        else {
            g_HoleSetup.conditions.isRaining = FALSE;
        }

        // The first hole always has some wind
        do {
            g_HoleSetup.conditions.windSpeed = (rand() & RANDOM_WIND_STEP_MASK) * WIND_SPEED_STEP;
            if (g_HoleSetup.conditions.windSpeed < WIND_SPEED_MIN) {
                g_HoleSetup.conditions.windSpeed = 0;
            }
        }
        while (g_GameState.holeIndex == 0 && g_HoleSetup.conditions.windSpeed == 0);
        g_HoleSetup.conditions.windDirection = rand() * 2;
        if (g_GameState.isMiniGolf) {
            g_HoleSetup.conditions.windSpeed = 0;
        }

        g_HoleSetup.conditions.lightPitch = HOLE_LIGHT_PITCH;
        if (g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX) {
            g_HoleSetup.conditions.windDirection = DRIVING_RANGE_WIND_DIRECTION;
            g_HoleSetup.conditions.windSpeed = 0;
            g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
            g_HoleSetup.conditions.lightYaw = rand() * 4;
        }
        else {
            if (g_GameState.holeIndex == 0) {
                g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_START;
            }
            else {
                s32 firstHoleIndex = GetFirstHoleIndex();
                s32 colorIndex = (g_GameState.holeIndex - firstHoleIndex) *
                    (SCENE_COLOR_INDEX_ROUND_END + 1) / GetRoundHoleCount();

                if (colorIndex > SCENE_COLOR_INDEX_ROUND_END) {
                    colorIndex = SCENE_COLOR_INDEX_ROUND_END;
                }

                g_HoleSetup.conditions.sceneColorIndex = colorIndex;
                g_HoleSetup.conditions.lightYaw += g_GameState.holeIndex * LIGHT_YAW_STEP_PER_HOLE;
            }

            if (g_GameState.isMiniGolf) {
                g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_START;
            }

            if (g_GameState.isSuddenDeath) {
                g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_SUDDEN_DEATH;
            }
        }

        courseRecordIndex = GetCourseRecordIndex();
        g_HoleSetup.par = g_CourseRecords[courseRecordIndex].holes[GetHoleRecordIndex()].par;
        if (IsGameFlagSet(GAME_FLAG_DRIVING_RANGE_COLORS)) {
            g_HoleSetup.conditions.isRaining = FALSE;
            g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
        }

        if (IsGameFlagSet(GAME_FLAG_ROUND_END_COLORS)) {
            g_HoleSetup.conditions.isRaining = FALSE;
            g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_END;
        }
    }

    if (g_GameState.isRoundContinued) {
        return;
    }

    attemptCount = 0;
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        cupIndex = 0;
    }
    else if (g_GameState.isMiniGolf) {
        cupIndex =
            (g_GameState.courseIndex - MINI_GOLF_COURSE_INDEX) % MINI_GOLF_COURSES_PER_RECORD;
    }
    else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
        cupIndex = f_CycledCupIndex++ & (CUP_POSITION_COUNT - 1);
    }
    else {
        // The cups are drawn three, three, two and one times in nine
        cupDraw =
            (u32)GetHoleRandom(g_GameState.roundSeed, g_GameState.holeIndex) >> CUP_DRAW_SHIFT;
        cupDraw %= CUP_DRAW_COUNT;
        if (cupDraw < CUP_DRAW_FIRST_END) {
            cupIndex = 0;
        }
        else if (cupDraw < CUP_DRAW_SECOND_END) {
            cupIndex = 1;
        }
        else if (cupDraw < CUP_DRAW_THIRD_END) {
            cupIndex = 2;
        }
        else {
            cupIndex = 3;
        }
    }

    // Holes leave unused cup positions at zero, so skip to one that is set
    for (;;) {
        g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[cupIndex][AXIS_X];
        g_HoleSetup.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[cupIndex][AXIS_Y];
        g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[cupIndex][AXIS_Z];
        g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_X];
        g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Y];
        g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Z];
        attemptCount++;
        cupIndex = (cupIndex + 1) & (CUP_POSITION_COUNT - 1);
        if (attemptCount >= CUP_ATTEMPT_COUNT_MAX) {
            break;
        }

        if (AreCupAndTeeSet()) {
            break;
        }
    }

    g_HoleSetup.nextCupIndex = cupIndex;
}

// Sets the cup on the ground and resets the shot and the effects; on a
// round's first hole, also clears four per-hole tables
void InitHoleState(void) {
    s32 index;

    g_HoleSetup.cupPosition[AXIS_Y] =
        GetGroundHeight(g_HoleSetup.cupPosition[AXIS_X], g_HoleSetup.cupPosition[AXIS_Z]);
    if (IsGameFlagSet(GAME_FLAG_HALT_HOLE_LOADER)) {
        while (TRUE) {
            WaitForOtherThreads();
        }
    }

    PlacePlayersOnTee();
    g_CurrentShot.surface = SURFACE_OUT_OF_BOUNDS;
    osSyncPrintf("mp2sub1\n");
    PrepareHoleDisplay();
    osSyncPrintf("mp2sub2\n");
    ResetShotSetup();
    osSyncPrintf("mp2sub3\n");
    ResetBallSimulations();
    g_HoleTimeFrames[g_GameState.holeIndex] = 0;
    if (g_GameState.holeIndex == 0) {
        g_GameState.stopwatchFrameCount = 0;
        for (index = 0; index != COURSE_HOLE_COUNT; index++) {
            g_HoleTimeFrames[index] = 0;
            g_HoleTimeHundredths[index] = 0;
            g_HoleTimeSeconds[index] = 0;
            g_HoleTimeMinutes[index] = 0;
        }
    }
}

void ChooseConditionsAndInitHole(void) {
    ChooseHoleConditions();
    InitHoleState();
}

// The loader thread's entry: loads the current hole each time
// g_HasLoaderFinished is cleared, then sets it
void RunLoaderThread(void) {
    CourseRecordIndex courseRecordIndex;

    while (TRUE) {
        while (g_HasLoaderFinished) {
            WaitForOtherThreads();
        }

        courseRecordIndex = GetCourseRecordIndex();
        while (g_FrameCount - g_GroundDrawFrame < LOADER_FRAME_DELAY) {
            WaitForOtherThreads();
        }

        osSyncPrintf("MPSTART\n");
        while (IsAnyCharacterSlotBusy()) {
            osSyncPrintf("Wow! Polychar is alive\n");
            WaitForOtherThreads();
        }

        if (IsGameFlagSet(GAME_FLAG_NEXT_COURSE_FIRST_HOLE) && g_GameState.holeIndex == 0) {
            SetGameFlag(GAME_FLAG_FADE_HIGH_GROUND);
        }
        else {
            ClearGameFlag(GAME_FLAG_FADE_HIGH_GROUND);
        }

        ClearGameFlag(GAME_FLAG_HOLE_LOADED);
        if (courseRecordIndex == COURSE_RECORD_BOO_VALLEY) {
            SetGameFlag(GAME_FLAG_FADE_HIGH_GROUND);
            SetGameFlag(GAME_FLAG_FADE_HIGH_GROUND_FULLY);
        }
        else {
            if (IsHighGroundFadedCourse(courseRecordIndex)) {
                SetGameFlag(GAME_FLAG_FADE_HIGH_GROUND);
            }
            else {
                ClearGameFlag(GAME_FLAG_FADE_HIGH_GROUND);
            }

            ClearGameFlag(GAME_FLAG_FADE_HIGH_GROUND_FULLY);
        }

        if (courseRecordIndex == COURSE_RECORD_KOOPA_PARK &&
            g_GameState.holeIndex == KOOPA_PARK_UNFADED_HOLE_INDEX) {
            SetGameFlag(GAME_FLAG_NO_HIGH_GROUND_FADE);
        }
        else {
            ClearGameFlag(GAME_FLAG_NO_HIGH_GROUND_FADE);
        }

        osSyncPrintf("MP1\n");
        LoadHole();
        osSyncPrintf("MP2\n");
        ChooseHoleConditions();
        osSyncPrintf("MP3\n");
        PrepareHoleScene();
        InitHoleState();
        osSyncPrintf("MP4\n");
        BuildGroundDisplayLists();
        osSyncPrintf("MP5\n");
        InitRoundHud();
        SetGameFlag(GAME_FLAG_HOLE_LOADED);
        if (IsGameFlagSet(GAME_FLAG_HALT_HOLE_LOADER)) {
            while (TRUE) {
                WaitForOtherThreads();
            }
        }

        g_HasLoaderFinished = TRUE;
        osDestroyThread(&f_LoaderThread);
    }
}

void ResetLoaderThread(void) {
    g_HasLoaderFinished = TRUE;
}

#ifdef AVOID_UB
// The loader thread's entry as osCreateThread calls it, with an argument.
// Calling RunLoaderThread through a pointer of that type is undefined.
static void EnterLoaderThread(void *argument UNUSED) {
    RunLoaderThread();
}
#endif

void StartLoaderThread(void) {
    UnloadOverlay(GetOverlayOfModule(MODULE_SHOT_SIMULATION));
    LoadOverlay(GetOverlayOfModule(MODULE_HOLE_BUILDER));
    osCreateThread(
        &f_LoaderThread,
        LOADER_THREAD_ID,
#ifdef AVOID_UB
        EnterLoaderThread,
#else
        (void (*)(void *))RunLoaderThread,
#endif
        NULL,
        &g_LoaderThreadStack[LOADER_THREAD_STACK_SIZE],
        LOADER_THREAD_PRIORITY
    );
    osStartThread(&f_LoaderThread);
    g_HasLoaderFinished = FALSE;
    BuildNoiseTexture();
}

void StopLoaderThread(void) {
    if (!g_HasLoaderFinished) {
        osDestroyThread(&f_LoaderThread);
        g_HasLoaderFinished = TRUE;
    }
}
