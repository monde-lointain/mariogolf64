#include "common.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "character_slot.h"
#include "course.h"
#include "current_shot.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "libc.h"
#include "overlay_manager.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "terrain.h"
#include "variables.h"
#include "work_area.h"

/* ChooseHoleConditions gives up on finding a set cup position after these */
#define MAX_CUP_ATTEMPT_COUNT 120
/*
 * A hole's cup is drawn from (GetHoleRandom() >> CUP_DRAW_SHIFT) % CUP_DRAW_COUNT;
 * a draw below an end picks that end's cup
 */
#define CUP_DRAW_SHIFT 3
#define CUP_DRAW_COUNT 9
#define CUP_DRAW_FIRST_END 3
#define CUP_DRAW_SECOND_END 6
#define CUP_DRAW_THIRD_END 8

/* A hole rains when rand() & RAIN_DRAW_MASK is DRY_DRAW_COUNT or more */
#define RAIN_DRAW_MASK 3
#define DRY_DRAW_COUNT 3

/* Wind speeds come in steps; lighter winds than MIN_WIND_SPEED are calm */
#define WIND_SPEED_STEP 10000
#define WIND_SPEED_STEP_MASK 0x3F
#define MIN_WIND_SPEED 0x18000
#define DRIVING_RANGE_WIND_DIRECTION 0x4000

/* The light turns this far, in binary angles, from each hole to the next */
#define LIGHT_YAW_STEP_PER_HOLE 0x5000
#define HOLE_LIGHT_PITCH 0x3000

/* A g_GameMode value whose purpose is not yet known */
#define GAME_MODE_UNK13 13

/* GetHoleRandom's generator, a linear congruential one */
#define HOLE_RANDOM_MULTIPLIER 0x5D588B65
#define HOLE_RANDOM_INCREMENT 1


/* A mini-golf course is a main course's hole shrunk by these, by axis */
#define MINI_GOLF_SHRINK_X 4
#define MINI_GOLF_SHRINK_Y 4
#define MINI_GOLF_SHRINK_Z 8
/* How much ShrinkMiniGolfHole raises the top corners of shaded triangles, and lowers the others */
#define MINI_GOLF_SHADED_RAISE 30.0f
#define MINI_GOLF_SHADED_DROP 6.0f

/* The loader thread waits this many frames after D_80105318's frame before loading */
#define LOADER_FRAME_DELAY 3

#define LOADER_THREAD_ID 10
#define LOADER_THREAD_PRIORITY 20
/* StartLoaderThread unloads one module and loads the other */
#define LOADER_REPLACED_MODULE_ID 8
#define LOADER_MODULE_ID 6

/* The Koopa Park hole that sets GAME_FLAG_UNK69 */
#define KOOPA_PARK_UNK69_HOLE_INDEX 7

#define LZ_STREAM_FLAG_FIRST (1 << 0)
#define LZ_STREAM_FLAG_LAST (1 << 1)
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
/* The DMA buffers' alignment, in bytes */
#define LZ_BUFFER_ALIGNMENT 16
#define ALIGN_BUFFER(address) (((u32)(address) + LZ_BUFFER_ALIGNMENT - 1) & ~(LZ_BUFFER_ALIGNMENT - 1))

/* A reference word holds a distance back into the output, then a length less than the copy's */
#define LZ_REFERENCE_LENGTH_MASK 0x1F
#define LZ_REFERENCE_DISTANCE_SHIFT 5
#define LZ_MIN_REFERENCE_LENGTH 2

/* A patch command holds its kind or offset in its high byte and its count less one in its low byte */
#define LZ_PATCH_COUNT_MASK 0xFF
#define LZ_PATCH_KIND_MASK 0xFF00
#define LZ_PATCH_OFFSET_SHIFT 8

typedef struct LzStream {
    /* 0x00 */ u16 *input;
    /* 0x04 */ u16 *outputStart;
    /* 0x08 */ u16 *nextInput;
    /* 0x0C */ u16 *output;
    /* 0x10 */ u16 *inputEnd;
    /* 0x14 */ u16 flags;
    /* 0x18 */ u16 *window;
    /* 0x1C */ u16 windowIndex;
    /* 0x1E */ u16 patchCount;
    /* 0x20 */ u16 historyIndex;
    /* 0x24 */ u16 *history;
} LzStream; /* size = 0x28 */

/* How ShrinkMiniGolfHole scales a course down to mini-golf size, by axis */
extern Vec3 g_MiniGolfScale;
extern OSPiHandle *nuPiCartHandle;
extern s32 D_800BB0E8;
extern s32 sHoleSeed;
/*
 * hole_loader.h declares this without volatile, as the files that only read
 * it were compiled; the loader thread waits on it, so here it is volatile
 */
extern volatile s32 g_HasLoaderFinished;
extern OSThread sLoaderThread;
extern u8 D_8025D350[];
extern u32 D_80105318;
extern s8 D_80104B6D;
extern s8 D_801B60B8;
extern s32 D_801B60A0;
extern s32 D_801B60B4;
extern s32 sCycledCupIndex;
/* The cup index after the chosen one; nothing reads it */
extern s8 sNextCupIndex;
extern Unk800BA9D8 D_80105F40;
/* menu.c declares this as Unk68[MAX_PLAYER_COUNT]; only its first bytes are copied here */
extern Unk800BA9D8 D_800FEDD8;

void func_8005F1C8(void);
void PlacePlayersOnTee(void);
void func_80253234(s32);
void ToggleGroundTileBorders(void);
void BuildHole(s32 courseRecordIndex, s32 holeRecordIndex);
void BuildGroundDisplayLists(void);
void ShrinkMiniGolfHolePoints(void);
f32 MeasureGroundTiles(void);
s32 ChooseHoleConditions(void);

/*
 * Decodes the compressed words from stream->input to stream->inputEnd onto
 * the end of the output. Returns the output's size in bytes once the stream
 * ends. If the chunk runs low first and more is to come, moves the words left
 * to just before stream->nextInput, saves its place and returns -1.
 */
s32 DecodeLzChunk(LzStream *stream) {
    u16 *input;
    u16 *output;
    u16 *inputEnd;
    u16 *copy;
    u16 *source;
    s32 controlBits;
    u16 reference;
    u16 *referenceEnd;
    u16 *repeat;
    u16 evenWord;
    u16 oddWord;
    s32 hasMoreInput;

    if (stream->flags & LZ_STREAM_FLAG_FIRST) {
        input = stream->input;
        output = stream->outputStart;
        input += LZ_HEADER_WORD_COUNT;
    } else {
        input = stream->input;
        output = stream->output;
    }
    inputEnd = stream->inputEnd;
    hasMoreInput = !(stream->flags & LZ_STREAM_FLAG_LAST);
    while (TRUE) {
        if (input >= inputEnd - LZ_LITERAL_RUN_LENGTH) {
            if (hasMoreInput) {
                copy = stream->nextInput;
                while (input < inputEnd) {
                    *--copy = *--inputEnd;
                }
                stream->input = copy;
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
        /* The bits are read from the top; the 1 below them marks their end */
        controlBits <<= 16;
        controlBits |= 0x8000;
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
                return (u8 *)output - (u8 *)stream->outputStart;
            }
            source = output - (reference >> LZ_REFERENCE_DISTANCE_SHIFT);
            referenceEnd = output + ((reference & LZ_REFERENCE_LENGTH_MASK) + LZ_MIN_REFERENCE_LENGTH);
            *output = *source++;
            repeat = output + 1;
            do {
                *repeat++ = *source++;
            } while (repeat != referenceEnd);
            output = repeat;
        }
    }
}

/*
 * Like DecodeLzChunk, but the decoded words are commands that patch the data
 * already in the output buffer: a run of literal words, a copy from the same
 * or a later word of the buffer, or a copy from the words it overwrote.
 */
s32 DecodePatchedLzChunk(LzStream *stream) {
    u16 patchCount;
    u16 *history;
    u16 historyIndex;
    u16 *output;
    u16 *input;
    u16 *inputEnd;
    u16 *window;
    u16 *copy;
    u16 windowIndex;
    s32 controlBits;
    s32 reference;
    s32 length;
    s32 index;
    u16 word;
    s32 hasMoreInput;

    auto inline void ApplyPatchCommand(u16 command);

    /* Nested, so that it reaches the patch state through the static chain as the original does */
    inline void ApplyPatchCommand(u16 command) {
        s16 offset;
        u16 value;

        patchCount = (command & LZ_PATCH_COUNT_MASK) + 1;
        if ((command & LZ_PATCH_KIND_MASK) != LZ_PATCH_KIND_LITERALS) {
            offset = (s16)(command & LZ_PATCH_KIND_MASK) >> LZ_PATCH_OFFSET_SHIFT;
            if (offset < 0) {
                while (patchCount != 0) {
                    value = history[(historyIndex + offset) & LZ_HISTORY_MASK];
                    history[historyIndex++] = *output;
                    historyIndex &= LZ_HISTORY_MASK;
                    *output++ = value;
                    patchCount--;
                }
            } else {
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
        controlBits = 0x80000000;
        historyIndex = 0;
        patchCount = 0;
        input += LZ_HEADER_WORD_COUNT;
        output = stream->outputStart;
        history = stream->history;
        goto decode;
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
                copy = stream->nextInput;
                while (input < inputEnd) {
                    *--copy = *--inputEnd;
                }
                stream->input = copy;
                stream->windowIndex = windowIndex;
                stream->output = output;
                stream->patchCount = patchCount;
                stream->historyIndex = historyIndex;
                return -1;
            }
        }
        controlBits = *input++;
        controlBits <<= 16;
        controlBits |= 0x8000;
    decode:
        while (TRUE) {
            while (controlBits >= 0) {
                word = *input++;
                window[windowIndex++] = word;
                windowIndex &= LZ_WINDOW_MASK;
                if (patchCount == 0) {
                    ApplyPatchCommand(word);
                } else {
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
            /* The u16 casts are redundant, but they are needed to match */
            if ((u16)reference == 0) {
                return (u8 *)output - (u8 *)stream->outputStart;
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
                } else {
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

/*
 * Decompresses the size bytes at romAddress into destination with
 * DecodeLzChunk, reading the ROM in chunks through two DMA buffers so that
 * one fills while the other is decoded. Returns the decompressed size.
 */
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
    s32 result;

    receivedCount = 0;
    dmaIndex = 0;
    pendingCount = 0;
    receiveIndex = 0;
    buffers[0] = (u16 *)(ALIGN_BUFFER(rawBuffers[0]) + LZ_CARRY_SIZE);
    buffers[1] = (u16 *)(ALIGN_BUFFER(rawBuffers[1]) + LZ_CARRY_SIZE);
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
        stream.inputEnd = buffers[otherIndex] + LZ_CHUNK_SIZE / sizeof(u16);
        if (pendingCount == 0 && size == 0) {
            stream.flags |= LZ_STREAM_FLAG_LAST;
        }
        result = DecodeLzChunk(&stream);
        receivedCount--;
        if (result >= 0) {
            break;
        }
        stream.flags &= ~LZ_STREAM_FLAG_FIRST;
    }
    osWritebackDCache(destination, result);
    return result;
}

/* LoadLzData for data compressed for DecodePatchedLzChunk */
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
    s32 result;

    receivedCount = 0;
    dmaIndex = 0;
    pendingCount = 0;
    receiveIndex = 0;
    buffers[0] = (u16 *)(ALIGN_BUFFER(rawBuffers[0]) + LZ_CARRY_SIZE);
    buffers[1] = (u16 *)(ALIGN_BUFFER(rawBuffers[1]) + LZ_CARRY_SIZE);
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
        stream.inputEnd = buffers[otherIndex] + PATCHED_LZ_CHUNK_SIZE / sizeof(u16);
        if (pendingCount == 0 && size == 0) {
            stream.flags |= LZ_STREAM_FLAG_LAST;
        }
        result = DecodePatchedLzChunk(&stream);
        receivedCount--;
        if (result >= 0) {
            break;
        }
        stream.flags &= ~LZ_STREAM_FLAG_FIRST;
    }
    osWritebackDCache(destination, result);
    return result;
}


/* A pseudo-random number that is the same each time for a seed and a hole */
s32 GetHoleRandom(s32 seed, s32 holeIndex) {
    void SetHoleSeed(s32 value) {
        sHoleSeed = value;
    }
    s32 NextHoleRandom(void) {
        return sHoleSeed = sHoleSeed * HOLE_RANDOM_MULTIPLIER + HOLE_RANDOM_INCREMENT;
    }

    SetHoleSeed(holeIndex);
    SetHoleSeed(seed + NextHoleRandom());
    return NextHoleRandom();
}

/* On a mini-golf course, scales the hole's points down with g_MiniGolfScale */
void ShrinkMiniGolfHolePoints(void) {
    s32 axis;
    s32 index;

    if (g_IsMiniGolf) {
        g_MiniGolfScale[AXIS_X] = 1.0f / MINI_GOLF_SHRINK_X;
        g_MiniGolfScale[AXIS_Y] = 1.0f / MINI_GOLF_SHRINK_Y;
        g_MiniGolfScale[AXIS_Z] = 1.0f / MINI_GOLF_SHRINK_Z;
        for (axis = 0; axis < AXIS_COUNT; axis++) {
            for (index = 0; index < CUP_POSITION_COUNT; index++) {
                g_HolePoints.cupPositions[index][axis] *= g_MiniGolfScale[axis];
                g_HolePoints.unk30[index][axis] *= g_MiniGolfScale[axis];
            }
            for (index = 0; index < ARRAY_COUNT(g_HolePoints.unk60); index++) {
                g_HolePoints.unk60[index][axis] *= g_MiniGolfScale[axis];
                g_HolePoints.routePoints[index][axis] *= g_MiniGolfScale[axis];
            }
        }
    }
}

/* Reads the current hole's asset files, then shrinks its points for mini-golf */
void LoadHole(void) {
    osSyncPrintf("putter%d:course%d:hole%d\n", g_IsMiniGolf, g_CourseIndex, g_HoleIndex);
    BuildHole(GetCourseRecordIndex(), GetHoleRecordIndex());
    ShrinkMiniGolfHolePoints();
}

/*
 * Whether ShrinkMiniGolfHole raises a shaded triangle's vertex, rather
 * than drops it. An odd triangle's last two vertices are raised, and an
 * even triangle's last one. It is a macro, as an inline function compiles
 * differently from the original.
 */
#define IsRaisedShadedVertex(index, vertexIndex)                                                                      \
    ((((index) & 1) && (((vertexIndex) == 1) || ((vertexIndex) == 2))) || (!((index) & 1) && ((vertexIndex) == 2)))

/*
 * On a mini-golf course, scales the hole's ground, billboards and triangles
 * down with g_MiniGolfScale
 */
void ShrinkMiniGolfHole(void) {
    Vtx_t point;
    s32 row;
    s32 column;
    s32 index;
    s32 vertexIndex;
    Vtx *vertex;

    if (g_IsMiniGolf) {
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
            for (vertexIndex = 0; vertexIndex < HOLE_TRIANGLE_VERTEX_COUNT; vertexIndex++) {
                vertex = g_WorkArea.hole.triangles[index].vertices;
                vertex[vertexIndex].v.ob[AXIS_X] *= g_MiniGolfScale[AXIS_X];
                vertex[vertexIndex].v.ob[AXIS_Y] *= g_MiniGolfScale[AXIS_Y];
                vertex[vertexIndex].v.ob[AXIS_Z] *= g_MiniGolfScale[AXIS_Z];
                if (g_WorkArea.hole.triangles[index].kind == HOLE_TRIANGLE_KIND_SHADED) {
                    if (IsRaisedShadedVertex(index, vertexIndex)) {
                        vertex[vertexIndex].v.ob[AXIS_Y] += MINI_GOLF_SHADED_RAISE;
                    } else {
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
    s32 unk;

    D_800BB0E8 = 1;
    switch (g_HoleConditions.sceneColorIndex) {
        case SCENE_COLOR_INDEX_ROUND_START:
            unk = 2;
            break;
        case SCENE_COLOR_INDEX_ROUND_END:
            unk = 0;
            break;
        case SCENE_COLOR_INDEX_DRIVING_RANGE:
            unk = 1;
            break;
    }
    if (!IsGameFlagSet(GAME_FLAG_UNK11)) {
        func_80253234(unk);
    }
    D_800BB0E8 = 0;
    ToggleGroundTileBorders();
    SetCharacterLights();
    ShrinkMiniGolfHole();
}

/*
 * Chooses the wind, the rain and the scene's colors for the hole about to be
 * played, then its cup from the hole's cup positions. In some scenes and modes
 * the cup comes from D_80105F40 or D_800FEDD8 instead.
 *
 * Returns no value, but is declared s32 because that matches.
 */
s32 ChooseHoleConditions(void) {
    s32 cupIndex;
    s32 attemptCount;
    u32 random;
    s32 courseRecordIndex;

    D_80104B6D = -1;
    if (g_Scene == SCENE_PLAY_UNK13 || g_GameMode == GAME_MODE_UNK8 || g_GameState.unk31 != 0) {
        if (g_GameMode == GAME_MODE_RING_SHOT) {
            D_800BA9D8 = D_80105F40;
        } else {
            D_800BA9D8 = D_800FEDD8;
        }
        return;
    }
    if (D_801B60B8 != 0) {
        return;
    }
    if (g_GameMode != GAME_MODE_UNK12 && g_GameMode != GAME_MODE_UNK13) {
        /* The first hole, mini-golf and GAME_MODE_TRAINING stay dry */
        if (g_GameState.holeIndex != 0 && g_GameMode != GAME_MODE_TRAINING && !g_GameState.isMiniGolf) {
            if ((rand() & RAIN_DRAW_MASK) < DRY_DRAW_COUNT) {
                g_HoleConditions.isRaining = FALSE;
            } else {
                g_HoleConditions.isRaining = TRUE;
            }
            if (g_GameState.courseIndex == COURSE_SHY_GUY_DESERT && (rand() & RAIN_DRAW_MASK) < DRY_DRAW_COUNT) {
                g_HoleConditions.isRaining = FALSE;
            }
        } else {
            g_HoleConditions.isRaining = FALSE;
        }
        /* The first hole always has some wind */
        do {
            g_HoleConditions.windSpeed = (rand() & WIND_SPEED_STEP_MASK) * WIND_SPEED_STEP;
            if (g_HoleConditions.windSpeed < MIN_WIND_SPEED) {
                g_HoleConditions.windSpeed = 0;
            }
        } while (g_GameState.holeIndex == 0 && g_HoleConditions.windSpeed == 0);
        g_HoleConditions.windDirection = rand() * 2;
        if (g_GameState.isMiniGolf) {
            g_HoleConditions.windSpeed = 0;
        }
        g_HoleConditions.lightPitch = HOLE_LIGHT_PITCH;
        if (g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX) {
            g_HoleConditions.windDirection = DRIVING_RANGE_WIND_DIRECTION;
            g_HoleConditions.windSpeed = 0;
            g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
            g_HoleConditions.lightYaw = rand() * 4;
        } else {
            if (g_GameState.holeIndex == 0) {
                g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_START;
            } else {
                s32 firstHoleIndex = GetFirstHoleIndex();
                s32 colorIndex = (g_GameState.holeIndex - firstHoleIndex) * (SCENE_COLOR_INDEX_ROUND_END + 1) /
                                 GetRoundHoleCount();

                if (colorIndex > SCENE_COLOR_INDEX_ROUND_END) {
                    colorIndex = SCENE_COLOR_INDEX_ROUND_END;
                }
                g_HoleConditions.sceneColorIndex = colorIndex;
                g_HoleConditions.lightYaw += g_GameState.holeIndex * LIGHT_YAW_STEP_PER_HOLE;
            }
            if (g_IsMiniGolf) {
                g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_START;
            }
            if (D_801B60A0 != 0) {
                g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_UNK11;
            }
        }
        courseRecordIndex = GetCourseRecordIndex();
        g_HolePar = g_CourseRecords[courseRecordIndex].holes[GetHoleRecordIndex()].par;
        if (IsGameFlagSet(GAME_FLAG_UNK7C)) {
            g_HoleConditions.isRaining = FALSE;
            g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
        }
        if (IsGameFlagSet(GAME_FLAG_UNK7D)) {
            g_HoleConditions.isRaining = FALSE;
            g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_END;
        }
    }
    if (D_801B60B8 != 0) {
        return;
    }
    attemptCount = 0;
    if (g_GameMode == GAME_MODE_RING_SHOT) {
        cupIndex = 0;
    } else if (g_IsMiniGolf) {
        cupIndex = (g_CourseIndex - MINI_GOLF_COURSE_INDEX) % MINI_GOLF_COURSES_PER_RECORD;
    } else if (g_GameMode == GAME_MODE_TRAINING) {
        cupIndex = sCycledCupIndex++ & (CUP_POSITION_COUNT - 1);
    } else {
        /* The cups are drawn three, three, two and one times in nine */
        random = (u32)GetHoleRandom(D_801B60B4, g_HoleIndex) >> CUP_DRAW_SHIFT;
        random %= CUP_DRAW_COUNT;
        if (random < CUP_DRAW_FIRST_END) {
            cupIndex = 0;
        } else if (random < CUP_DRAW_SECOND_END) {
            cupIndex = 1;
        } else if (random < CUP_DRAW_THIRD_END) {
            cupIndex = 2;
        } else {
            cupIndex = 3;
        }
    }
    /* Holes leave unused cup positions at zero, so skip to one that is set */
    for (;;) {
        D_800BA9D8.cupPosition[AXIS_X] = g_HolePoints.cupPositions[cupIndex][AXIS_X];
        D_800BA9D8.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[cupIndex][AXIS_Y];
        D_800BA9D8.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[cupIndex][AXIS_Z];
        D_800BA9D8.teePosition[AXIS_X] = g_HolePoints.unk30[0][AXIS_X];
        D_800BA9D8.teePosition[AXIS_Y] = g_HolePoints.unk30[0][AXIS_Y];
        D_800BA9D8.teePosition[AXIS_Z] = g_HolePoints.unk30[0][AXIS_Z];
        attemptCount++;
        cupIndex = (cupIndex + 1) & (CUP_POSITION_COUNT - 1);
        if (attemptCount >= MAX_CUP_ATTEMPT_COUNT) {
            break;
        }
        if (D_800BA9D8.cupPosition[AXIS_X] != 0 && D_800BA9D8.cupPosition[AXIS_Z] != 0 &&
            D_800BA9D8.teePosition[AXIS_X] != 0 && D_800BA9D8.teePosition[AXIS_Z] != 0) {
            break;
        }
    }
    sNextCupIndex = cupIndex;
}

/*
 * Sets the cup on the ground and resets the shot and the effects; on a
 * round's first hole, also clears four per-hole tables
 */
void InitHoleState(void) {
    s32 index;

    D_800BA9D8.cupPosition[AXIS_Y] = GetGroundHeight(D_800BA9D8.cupPosition[AXIS_X], D_800BA9D8.cupPosition[AXIS_Z]);
    if (IsGameFlagSet(GAME_FLAG_HALT_HOLE_LOADER)) {
        while (TRUE) {}
    }
    PlacePlayersOnTee();
    g_CurrentShot.surface = 0;
    osSyncPrintf("mp2sub1\n");
    PrepareHoleDisplay();
    osSyncPrintf("mp2sub2\n");
    func_8005F1C8();
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

/*
 * The loader thread's entry: loads the current hole each time
 * g_HasLoaderFinished is cleared, then sets it
 */
void RunLoaderThread(void) {
    s32 courseRecordIndex;

    while (TRUE) {
        while (g_HasLoaderFinished) {}
        courseRecordIndex = GetCourseRecordIndex();
        while (g_FrameCount - D_80105318 < LOADER_FRAME_DELAY) {}
        osSyncPrintf("MPSTART\n");
        while (IsAnyCharacterSlotBusy()) {
            osSyncPrintf("Wow! Polychar is alive\n");
        }
        if (IsGameFlagSet(GAME_FLAG_UNK51) && g_HoleIndex == 0) {
            SetGameFlag(GAME_FLAG_UNK12);
        } else {
            ClearGameFlag(GAME_FLAG_UNK12);
        }
        ClearGameFlag(GAME_FLAG_HOLE_LOADED);
        if (courseRecordIndex == COURSE_RECORD_BOO_VALLEY) {
            SetGameFlag(GAME_FLAG_UNK12);
            SetGameFlag(GAME_FLAG_UNK60);
        } else {
            if (courseRecordIndex == COURSE_RECORD_TOAD_HIGHLANDS || courseRecordIndex == COURSE_RECORD_KOOPA_PARK ||
                courseRecordIndex == COURSE_RECORD_YOSHIS_ISLAND || courseRecordIndex == COURSE_RECORD_MARIOS_STAR) {
                SetGameFlag(GAME_FLAG_UNK12);
            } else {
                ClearGameFlag(GAME_FLAG_UNK12);
            }
            ClearGameFlag(GAME_FLAG_UNK60);
        }
        if (courseRecordIndex == COURSE_RECORD_KOOPA_PARK && g_HoleIndex == KOOPA_PARK_UNK69_HOLE_INDEX) {
            SetGameFlag(GAME_FLAG_UNK69);
        } else {
            ClearGameFlag(GAME_FLAG_UNK69);
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
            while (TRUE) {}
        }
        g_HasLoaderFinished = TRUE;
        osDestroyThread(&sLoaderThread);
    }
}

void ResetLoaderThread(void) {
    g_HasLoaderFinished = TRUE;
}

void StartLoaderThread(void) {
    UnloadOverlay(GetOverlayOfModule(LOADER_REPLACED_MODULE_ID));
    LoadOverlay(GetOverlayOfModule(LOADER_MODULE_ID));
    osCreateThread(&sLoaderThread, LOADER_THREAD_ID, (void (*)(void *))RunLoaderThread, NULL, D_8025D350,
                   LOADER_THREAD_PRIORITY);
    osStartThread(&sLoaderThread);
    g_HasLoaderFinished = FALSE;
    BuildNoiseTexture();
}

void StopLoaderThread(void) {
    if (!g_HasLoaderFinished) {
        osDestroyThread(&sLoaderThread);
        g_HasLoaderFinished = TRUE;
    }
}

