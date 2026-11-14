#include "hole_loader.h"

#include <nusys.h>
#include <stdlib.h>
#include <ultra64.h>

#include "ball_simulation.h"
#include "billboard.h"
#include "character_model.h"
#include "course.h"
#include "current_shot.h"
#include "game_flags.h"
#include "game_state.h"
#include "hole_builder.h"
#include "hole_conditions.h"
#include "hole_setup.h"
#include "hole_triangles.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "play.h"
#include "play_record.h"
#include "round_hud.h"
#include "scene.h"
#include "scoring.h"
#include "shot_camera.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "terrain.h"
#include "turn.h"
#include "work_area.h"

#define CUP_ATTEMPT_COUNT_MAX 120
#define AreCupAndTeeSet() \
    (g_HoleSetup.cupPosition[AXIS_X] != 0 && \
        g_HoleSetup.cupPosition[AXIS_Z] != 0 && \
        g_HoleSetup.teePosition[AXIS_X] != 0 && \
        g_HoleSetup.teePosition[AXIS_Z] != 0)
#define CUP_DRAW_SHIFT 3
#define CUP_DRAW_COUNT 9
#define CUP_DRAW_FIRST_END 3
#define CUP_DRAW_SECOND_END 6
#define CUP_DRAW_THIRD_END 8

#define RAIN_DRAW_MASK 3
#define DRY_DRAW_COUNT 3

#define WIND_SPEED_STEP 10000
#define WIND_SPEED_MIN 0x18000
#define DRIVING_RANGE_WIND_DIRECTION 0x4000

#define LIGHT_YAW_STEP_PER_HOLE 0x5000
#define HOLE_LIGHT_PITCH 0x3000

#define MINI_GOLF_SHRINK_X 4
#define MINI_GOLF_SHRINK_Y 4
#define MINI_GOLF_SHRINK_Z 8
#define MINI_GOLF_SHADED_RAISE 30.0f
#define MINI_GOLF_SHADED_DROP 6.0f

#define LOADER_FRAME_DELAY 3

#define LOADER_THREAD_ID 10
#define LOADER_THREAD_PRIORITY 20

#define KOOPA_PARK_UNFADED_HOLE_INDEX 7
#define IsHighGroundFadedCourse(courseRecordIndex) \
    ((courseRecordIndex) == COURSE_RECORD_TOAD_HIGHLANDS || \
        (courseRecordIndex) == COURSE_RECORD_KOOPA_PARK || \
        (courseRecordIndex) == COURSE_RECORD_YOSHIS_ISLAND || \
        (courseRecordIndex) == COURSE_RECORD_MARIOS_STAR)

#define LZ_STREAM_FLAG_FIRST (1 << 0)
#define LZ_STREAM_FLAG_LAST (1 << 1)
typedef u16 ULzStreamFlags16;
#define LZ_HEADER_WORD_COUNT 2
#define LZ_LITERAL_RUN_LENGTH 16
#define LZ_BUFFER_COUNT 2
#define LZ_CHUNK_SIZE 0x1000
#define LZ_CARRY_SIZE 0x20
#define LZ_RAW_BUFFER_SIZE 0x1040
#define LZ_WINDOW_MASK 0x7FF
#define PATCHED_LZ_CHUNK_SIZE 0x800
#define PATCHED_LZ_RAW_BUFFER_SIZE 0x840
#define LZ_HISTORY_MASK 0xFF
#define LZ_PATCH_KIND_LITERALS 0x8000
#define LZ_BUFFER_ALIGNMENT 16
#define AlignedLzBuffer(address) \
    (((u32)(address) + LZ_BUFFER_ALIGNMENT - 1) & ~(LZ_BUFFER_ALIGNMENT - 1))

#define LZ_REFERENCE_LENGTH_MASK 0x1F
#define LZ_REFERENCE_DISTANCE_SHIFT 5
#define LZ_REFERENCE_LENGTH_MIN 2

#define LZ_CONTROL_BIT_COUNT 16
#define LZ_CONTROL_END_BIT 0x8000
#define LZ_SPENT_CONTROL_BITS 0x80000000

#define LZ_PATCH_COUNT_MASK 0xFF
#define LZ_PATCH_KIND_MASK 0xFF00
#define LZ_PATCH_OFFSET_SHIFT 8

typedef struct LzStream {
    u16 *input;
    u16 *outputStart;
    u16 *nextInput;
    u16 *output;
    u16 *inputEnd;
    ULzStreamFlags16 flags;
    u16 *window;
    u16 windowIndex;
    u16 patchCount;
    u16 historyIndex;
    u16 *history;
} LzStream;

static s32 f_HoleSeed = 0;
static s32 f_CycledCupIndex = 0;
volatile Boolean g_HasLoaderFinished = TRUE;
static s32 f_UnusedWordAfterLoaderFinished UNUSED = 0;
static OSThread f_LoaderThread;
HolePoints g_HolePoints;
s32 g_HoleTimeFrames[COURSE_HOLE_COUNT];
s32 g_HoleTimeHundredths[COURSE_HOLE_COUNT];
s32 g_HoleTimeMinutes[COURSE_HOLE_COUNT];
s32 g_HoleTimeSeconds[COURSE_HOLE_COUNT];

s32 DecodeLzChunk(LzStream *stream) {
    u16 *input;
    u16 *output;
    u16 *inputEnd;
    u16 *carriedInput;
    u16 *source;
    s32 controlBits;
    u16 reference;
    u16 *referenceEnd;
    u16 *referenceOutput;
    u16 evenWord;
    u16 oddWord;
    Boolean hasMoreInput;

    if (stream->flags & LZ_STREAM_FLAG_FIRST) {
        input = stream->input;
        output = stream->outputStart;
        input += LZ_HEADER_WORD_COUNT;
    }
    else {
        input = stream->input;
        output = stream->output;
    }

    inputEnd = stream->inputEnd;
    hasMoreInput = !(stream->flags & LZ_STREAM_FLAG_LAST);
    while (TRUE) {
        if (input >= inputEnd - LZ_LITERAL_RUN_LENGTH) {
            if (hasMoreInput) {
                carriedInput = stream->nextInput;
                while (input < inputEnd) {
                    *--carriedInput = *--inputEnd;
                }

                stream->input = carriedInput;
                stream->output = output;
                return -1;
            }
        }

        controlBits = *input++;
        if (controlBits == 0) {
            evenWord = input[0];
            oddWord = input[1];
            output[0] = evenWord;
            evenWord = input[2];
            output[1] = oddWord;
            oddWord = input[3];
            output[2] = evenWord;
            evenWord = input[4];
            output[3] = oddWord;
            oddWord = input[5];
            output[4] = evenWord;
            evenWord = input[6];
            output[5] = oddWord;
            oddWord = input[7];
            output[6] = evenWord;
            evenWord = input[8];
            output[7] = oddWord;
            oddWord = input[9];
            output[8] = evenWord;
            evenWord = input[10];
            output[9] = oddWord;
            oddWord = input[11];
            output[10] = evenWord;
            evenWord = input[12];
            output[11] = oddWord;
            oddWord = input[13];
            output[12] = evenWord;
            evenWord = input[14];
            output[13] = oddWord;
            oddWord = input[15];
            output[14] = evenWord;
            output[15] = oddWord;
            input += LZ_LITERAL_RUN_LENGTH;
            output += LZ_LITERAL_RUN_LENGTH;
            continue;
        }

        controlBits <<= LZ_CONTROL_BIT_COUNT;
        controlBits |= LZ_CONTROL_END_BIT;
        while (TRUE) {
            while (controlBits >= 0) {
                *output++ = *input++;
                controlBits <<= 1;
            }

            controlBits <<= 1;
            if (controlBits == 0) {
                break;
            }

            reference = *input++;
            if (reference == 0) {
                return BytesBetween(stream->outputStart, output);
            }

            source = output - (reference >> LZ_REFERENCE_DISTANCE_SHIFT);
            referenceEnd =
                output + ((reference & LZ_REFERENCE_LENGTH_MASK) + LZ_REFERENCE_LENGTH_MIN);
            *output = *source++;
            referenceOutput = output + 1;
            do {
                *referenceOutput++ = *source++;
            }
            while (referenceOutput != referenceEnd);
            output = referenceOutput;
        }
    }
}

s32 DecodePatchedLzChunk(LzStream *stream) {
    u16 patchCount;
    u16 *history;
    u16 historyIndex;
    u16 *output;
    u16 *input;
    u16 *inputEnd;
    u16 *window;
    u16 *carriedInput;
    u16 windowIndex;
    s32 controlBits;
    s32 reference;
    s32 length;
    s32 index;
    u16 word;
    Boolean hasMoreInput;

    auto inline void ApplyPatchCommand(u16 command);

    inline void ApplyPatchCommand(u16 command) {
        s16 offset;
        u16 copiedWord;

        patchCount = (command & LZ_PATCH_COUNT_MASK) + 1;
        if ((command & LZ_PATCH_KIND_MASK) != LZ_PATCH_KIND_LITERALS) {
            offset = (s16)(command & LZ_PATCH_KIND_MASK) >> LZ_PATCH_OFFSET_SHIFT;
            if (offset < 0) {
                while (patchCount != 0) {
                    copiedWord = history[(historyIndex + offset) & LZ_HISTORY_MASK];
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output++ = copiedWord;
                    patchCount--;
                }
            }
            else {
                while (patchCount != 0) {
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output = output[offset];
                    output++;
                    patchCount--;
                }
            }
        }
    }

    inputEnd = stream->inputEnd;
    hasMoreInput = !(stream->flags & LZ_STREAM_FLAG_LAST);
    if (stream->flags & LZ_STREAM_FLAG_FIRST) {
        input = stream->input;
        window = stream->window;
        windowIndex = 0;
        controlBits = LZ_SPENT_CONTROL_BITS;
        historyIndex = 0;
        patchCount = 0;
        input += LZ_HEADER_WORD_COUNT;
        output = stream->outputStart;
        history = stream->history;
        goto DECODE;
    }

    output = stream->output;
    input = stream->input;
    window = stream->window;
    windowIndex = stream->windowIndex;
    historyIndex = stream->historyIndex;
    patchCount = stream->patchCount;
    history = stream->history;
    while (TRUE) {
        if (input >= inputEnd - LZ_LITERAL_RUN_LENGTH) {
            if (hasMoreInput) {
                carriedInput = stream->nextInput;
                while (input < inputEnd) {
                    *--carriedInput = *--inputEnd;
                }

                stream->input = carriedInput;
                stream->windowIndex = windowIndex;
                stream->output = output;
                stream->patchCount = patchCount;
                stream->historyIndex = historyIndex;
                return -1;
            }
        }

        controlBits = *input++;
        controlBits <<= LZ_CONTROL_BIT_COUNT;
        controlBits |= LZ_CONTROL_END_BIT;

DECODE:

        while (TRUE) {
            while (controlBits >= 0) {
                word = *input++;
                window[windowIndex++] = word;
                windowIndex &= LZ_WINDOW_MASK;
                if (patchCount == 0) {
                    ApplyPatchCommand(word);
                }
                else {
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output++ = word;
                    patchCount--;
                }

                controlBits <<= 1;
            }

            controlBits <<= 1;
            if (controlBits == 0) {
                break;
            }

            reference = *input++;

            if ((u16)reference == 0) {
                return BytesBetween(stream->outputStart, output);
            }

            length = reference & 0x1F;
            reference = (u16)reference >> LZ_REFERENCE_DISTANCE_SHIFT;
            length++;
            while (TRUE) {
                index = windowIndex;
                windowIndex = (windowIndex + 1) & LZ_WINDOW_MASK;
                word = window[(index - reference) & LZ_WINDOW_MASK];
                window[index] = word;
                if (patchCount == 0) {
                    ApplyPatchCommand(word);
                }
                else {
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output++ = word;
                    patchCount--;
                }

                if (length == 0) {
                    break;
                }

                length--;
            }
        }
    }
}

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
    return decodedSize;
}

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
    return decodedSize;
}

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

#define IsRaisedShadedVertex(index, vertexIndex) \
    ((((index) & 1) && (((vertexIndex) == 1) || ((vertexIndex) == 2))) || \
        (!((index) & 1) && ((vertexIndex) == 2)))

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

void InitHoleState(void) {
    s32 index;

    g_HoleSetup.cupPosition[AXIS_Y] =
        GetGroundHeight(g_HoleSetup.cupPosition[AXIS_X], g_HoleSetup.cupPosition[AXIS_Z]);
    if (IsGameFlagSet(GAME_FLAG_HALT_HOLE_LOADER)) {
        while (TRUE) {
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

void RunLoaderThread(void) {
    CourseRecordIndex courseRecordIndex;

    while (TRUE) {
        while (g_HasLoaderFinished) {
        }

        courseRecordIndex = GetCourseRecordIndex();
        while (g_FrameCount - g_GroundDrawFrame < LOADER_FRAME_DELAY) {
        }

        osSyncPrintf("MPSTART\n");
        while (IsAnyCharacterSlotBusy()) {
            osSyncPrintf("Wow! Polychar is alive\n");
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
            }
        }

        g_HasLoaderFinished = TRUE;
        osDestroyThread(&f_LoaderThread);
    }
}

void ResetLoaderThread(void) {
    g_HasLoaderFinished = TRUE;
}

void StartLoaderThread(void) {
    UnloadOverlay(GetOverlayOfModule(MODULE_SHOT_SIMULATION));
    LoadOverlay(GetOverlayOfModule(MODULE_HOLE_BUILDER));
    osCreateThread(
        &f_LoaderThread,
        LOADER_THREAD_ID,
        (void (*)(void *))RunLoaderThread,
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
