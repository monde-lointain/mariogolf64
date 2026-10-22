#include "common.h"
#include "heap.h"
#include "performance_meter.h"
#include <lib/libnusys/nusys.h>

/*
 * The performance meter is the game's copy of NuSystem's nuDebTaskPerfBar1,
 * extended with a CPU bar, statistics for every bar and a numeric readout.
 */

/* A frame of time is this wide on the meter, in the meter's own units */
#define FRAME_WIDTH 280
/* A frame at 60 Hz lasts this many microseconds */
#define FRAME_MICROSECONDS 16666
/* The meter can span this many frames after the first */
#define MAX_EXTRA_FRAME_COUNT 5

/* Converts an RDP counter value to microseconds, as NuSystem does */
#define RDP_COUNT_TO_MICROSECONDS(count) ((count) * 1643 / 100000)

#define BAR_START_X 1
/* The meter's top edge, measured up from the bottom of the screen */
#define METER_TOP_Y 24

#define READOUT_X 264
#define READOUT_TOP_Y 2
#define READOUT_LINE_HEIGHT 8

#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 8
#define DIGIT_SPACING 6

extern Vtx sFrameVertices[];
extern Vtx sCpuBarVertices[];
extern Vtx sAudioBarVertices[];
extern Vtx sRspBarVertices[];
extern Vtx sRdpBarVertices[];
extern Vtx sRdpCommandBarVertices[];
extern Vtx sRdpPipeBarVertices[];
extern Vtx sRdpTmemBarVertices[];
extern Vp sMeterViewport;
extern Gfx sInitRcpDisplayList[];
extern Gfx sFrameDisplayList[];
extern f32 sFrameScales[];
extern u32 sMeterDisplayListIndex;
extern u8 sDigitTexture[];
/* Counts the digit strings drawn, modulo 1024; nothing else reads it */
extern u32 sDigitDrawCount;
extern u32 sCpuBarMaximum;
extern u32 sCpuBarAverage;
extern u32 sAudioBarMaximum;
extern u32 sAudioBarAverage;
extern u32 sRspBarMaximum;
extern u32 sRspBarAverage;
extern u32 sRdpBarMaximum;
extern u32 sRdpBarAverage;
extern u32 sRdpCommandBarMaximum;
extern u32 sRdpCommandBarAverage;
extern u32 sRdpPipeBarMaximum;
extern u32 sRdpPipeBarAverage;
extern u32 sRdpTmemBarMaximum;
extern u32 sRdpTmemBarAverage;
extern Mtx sMeterProjectionMatrix;
extern Mtx sMeterViewMatrix;
extern Mtx sMeterIdentityMatrix;
extern Mtx sMeterScaleMatrix;
extern Mtx sMeterTranslationMatrix;
extern Gfx sMeterDisplayLists[2][512];
extern char sReadoutText[];

/* Draws text made of digits and spaces into *gfxP at the pixel x, y */
static void DrawDigits(Gfx **gfxP, u16 x, u16 y, char *text) {
    Gfx *gfx = *gfxP;
    char character;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 200, 0, 255);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, (u32)sDigitTexture & ~7, G_IM_FMT_IA, 64, 8, 0, G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    sDigitDrawCount = (sDigitDrawCount + 1) & 0x3FF;
    gDPPipeSync(gfx++);

    for (; (character = *text) != '\0'; text++, x += DIGIT_SPACING) {
        if (character != ' ') {
            gSPTextureRectangle(gfx++, x << 2, y << 2, (x + DIGIT_WIDTH) << 2, (y + DIGIT_HEIGHT) << 2,
                                G_TX_RENDERTILE, ((character - '0') & 0xF) * DIGIT_WIDTH << 5, 0, 1 << 10, 1 << 10);
        }
    }

    gDPPipeSync(gfx++);
    *gfxP = gfx;
}

/*
 * Draws bars for the time the CPU, the audio and graphics tasks and the RDP
 * counters took, over 1 + extraFrameCount frames, and starts the display list
 * as its own graphics task. cpuStartTime and cpuEndTime are in microseconds.
 */
void DrawPerformanceMeter(u32 extraFrameCount, u32 y, u32 cpuStartTime, u32 cpuEndTime, s32 taskFlags,
                          s32 readoutMode) {
    u32 i;
    s64 taskTime;
    s16 startX;
    s16 barLength;
    s32 cpuBarLength;
    s32 rspBarLength;
    s32 rdpBarLength;
    u32 vertexIndex;
    /* The readout never prints these lengths; they are kept to match */
    s32 audioBarLength;
    s32 rdpCommandBarLength;
    s32 rdpPipeBarLength;
    s32 rdpTmemBarLength;
    f32 posY;
    Gfx *gfx;
    u32 viMode;
    u32 scale;

    gfx = sMeterDisplayLists[sMeterDisplayListIndex];
    viMode = osViGetCurrentMode();
    if (viMode < OS_VI_NTSC_HPN1) {
        scale = 1;
    } else if (viMode <= OS_VI_NTSC_HPF2) {
        y /= 2;
        scale = 2;
    }

    sMeterViewport.vp.vscale[0] = sMeterViewport.vp.vtrans[0] = 320 * 2 * scale;
    sMeterViewport.vp.vscale[1] = sMeterViewport.vp.vtrans[1] = 240 * 2 * scale;
    gSPViewport(gfx++, &sMeterViewport);
    gSPDisplayList(gfx++, OS_K0_TO_PHYSICAL(sInitRcpDisplayList));
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320 * scale, osVirtualToPhysical(nuGfxCfb_ptr) & ~7);

    /* The projection's origin is at the bottom left, so y is flipped */
    posY = (240.0 - METER_TOP_Y) - y;
    if (posY == 0.0) {
        posY = 0.1f;
    }
    if (extraFrameCount > MAX_EXTRA_FRAME_COUNT) {
        extraFrameCount = MAX_EXTRA_FRAME_COUNT;
    }

    guOrtho(&sMeterProjectionMatrix, 0.0f, (f32)(320 * scale), 0.0f, (f32)(240 * scale), -1.0f, 10.0f, 1.0f);
    guLookAt(&sMeterViewMatrix, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    guScale(&sMeterScaleMatrix, sFrameScales[extraFrameCount] * (f32)scale, (f32)scale, 1.0f);
    /* Moves the meter 20 pixels right of the screen's left edge */
    guTranslate(&sMeterTranslationMatrix, 20.0 / sFrameScales[extraFrameCount], posY, 0.0f);
    gSPMatrix(gfx++, &sMeterProjectionMatrix, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &sMeterViewMatrix, G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
    guMtxIdent(&sMeterIdentityMatrix);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sMeterIdentityMatrix), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sMeterScaleMatrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sMeterTranslationMatrix), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);

    gSPDisplayList(gfx++, OS_K0_TO_PHYSICAL(sFrameDisplayList));
    sFrameVertices[1].v.ob[0] = FRAME_WIDTH * (extraFrameCount + 1) + 2;
    sFrameVertices[2].v.ob[0] = FRAME_WIDTH * (extraFrameCount + 1) + 2;
    for (i = 0; i < extraFrameCount + 1; i++) {
        vertexIndex = i * 4 + 4;
        gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 1, vertexIndex + 2, 0, vertexIndex + 2, vertexIndex + 3,
                      vertexIndex + 0, 0);
    }

    gSPVertex(gfx++, sCpuBarVertices, 4, 0);
    startX = BAR_START_X;
    taskTime = cpuEndTime - cpuStartTime;
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    cpuBarLength = barLength;
    if (sCpuBarAverage == 0) {
        sCpuBarAverage = barLength;
    } else {
        sCpuBarAverage = (sCpuBarAverage + barLength) >> 1;
    }
    if (sCpuBarMaximum < barLength) {
        sCpuBarMaximum = barLength;
    }
    sCpuBarVertices[0].v.ob[0] = startX;
    sCpuBarVertices[1].v.ob[0] = startX + barLength;
    sCpuBarVertices[2].v.ob[0] = startX + barLength;
    sCpuBarVertices[3].v.ob[0] = startX;
    gSP2Triangles(gfx++, 0, 2, 1, 0, 0, 3, 2, 0);

    gSPVertex(gfx++, sAudioBarVertices, 16, 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->auTaskCnt; i++) {
        vertexIndex = i * 4;
        taskTime = nuDebTaskPerfPtr->auTaskTime[i].rspEnd - nuDebTaskPerfPtr->auTaskTime[i].rspStart;
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        audioBarLength = barLength;
        if (sAudioBarAverage == 0) {
            sAudioBarAverage = barLength;
        } else {
            sAudioBarAverage = (sAudioBarAverage + barLength) >> 1;
        }
        if (sAudioBarMaximum < barLength) {
            sAudioBarMaximum = barLength;
        }
        sAudioBarVertices[vertexIndex + 0].v.ob[0] = startX;
        sAudioBarVertices[vertexIndex + 1].v.ob[0] = startX + barLength;
        sAudioBarVertices[vertexIndex + 2].v.ob[0] = startX + barLength;
        sAudioBarVertices[vertexIndex + 3].v.ob[0] = startX;
        gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0, vertexIndex + 0, vertexIndex + 3,
                      vertexIndex + 2, 0);
        startX += barLength;
    }

    gSPVertex(gfx++, sRspBarVertices, 32, 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * 4;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].rspEnd - nuDebTaskPerfPtr->gfxTaskTime[i].rspStart;
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rspBarLength = barLength;
        if (sRspBarAverage == 0) {
            sRspBarAverage = barLength;
        } else {
            sRspBarAverage = (sRspBarAverage + barLength) >> 1;
        }
        if (sRspBarMaximum < barLength) {
            sRspBarMaximum = barLength;
        }
        sRspBarVertices[vertexIndex + 0].v.ob[0] = startX;
        sRspBarVertices[vertexIndex + 1].v.ob[0] = startX + barLength;
        sRspBarVertices[vertexIndex + 2].v.ob[0] = startX + barLength;
        sRspBarVertices[vertexIndex + 3].v.ob[0] = startX;
        gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0, vertexIndex + 0, vertexIndex + 3,
                      vertexIndex + 2, 0);
        startX += barLength;
    }

    gSPVertex(gfx++, sRdpBarVertices, 32, 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * 4;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].rdpEnd - nuDebTaskPerfPtr->gfxTaskTime[i].rspStart;
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpBarLength = barLength;
        if (sRdpBarAverage == 0) {
            sRdpBarAverage = barLength;
        } else {
            sRdpBarAverage = (sRdpBarAverage + barLength) >> 1;
        }
        if (sRdpBarMaximum < barLength) {
            sRdpBarMaximum = barLength;
        }
        sRdpBarVertices[vertexIndex + 0].v.ob[0] = startX;
        sRdpBarVertices[vertexIndex + 1].v.ob[0] = barLength;
        sRdpBarVertices[vertexIndex + 2].v.ob[0] = barLength;
        sRdpBarVertices[vertexIndex + 3].v.ob[0] = startX;
        gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0, vertexIndex + 0, vertexIndex + 3,
                      vertexIndex + 2, 0);
        startX += barLength;
    }

    gSPVertex(gfx++, sRdpCommandBarVertices, 32, 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * 4;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_CMD_CTR];
        taskTime = RDP_COUNT_TO_MICROSECONDS(taskTime);
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpCommandBarLength = barLength;
        if (sRdpCommandBarAverage == 0) {
            sRdpCommandBarAverage = barLength;
        } else {
            sRdpCommandBarAverage = (sRdpCommandBarAverage + barLength) >> 1;
        }
        if (sRdpCommandBarMaximum < barLength) {
            sRdpCommandBarMaximum = barLength;
        }
        sRdpCommandBarVertices[vertexIndex + 0].v.ob[0] = startX;
        sRdpCommandBarVertices[vertexIndex + 1].v.ob[0] = barLength;
        sRdpCommandBarVertices[vertexIndex + 2].v.ob[0] = barLength;
        sRdpCommandBarVertices[vertexIndex + 3].v.ob[0] = startX;
        gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0, vertexIndex + 0, vertexIndex + 3,
                      vertexIndex + 2, 0);
        startX += barLength;
    }

    gSPVertex(gfx++, sRdpPipeBarVertices, 32, 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * 4;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_PIPE_CTR];
        taskTime = RDP_COUNT_TO_MICROSECONDS(taskTime);
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpPipeBarLength = barLength;
        if (sRdpPipeBarAverage == 0) {
            sRdpPipeBarAverage = barLength;
        } else {
            sRdpPipeBarAverage = (sRdpPipeBarAverage + barLength) >> 1;
        }
        if (sRdpPipeBarMaximum < barLength) {
            sRdpPipeBarMaximum = barLength;
        }
        sRdpPipeBarVertices[vertexIndex + 0].v.ob[0] = startX;
        sRdpPipeBarVertices[vertexIndex + 1].v.ob[0] = barLength;
        sRdpPipeBarVertices[vertexIndex + 2].v.ob[0] = barLength;
        sRdpPipeBarVertices[vertexIndex + 3].v.ob[0] = startX;
        gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0, vertexIndex + 0, vertexIndex + 3,
                      vertexIndex + 2, 0);
        startX += barLength;
    }

    gSPVertex(gfx++, sRdpTmemBarVertices, 32, 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * 4;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_TMEM_CTR];
        taskTime = RDP_COUNT_TO_MICROSECONDS(taskTime);
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpTmemBarLength = barLength;
        if (sRdpTmemBarAverage == 0) {
            sRdpTmemBarAverage = barLength;
        } else {
            sRdpTmemBarAverage = (sRdpTmemBarAverage + barLength) >> 1;
        }
        if (sRdpTmemBarMaximum < barLength) {
            sRdpTmemBarMaximum = barLength;
        }
        sRdpTmemBarVertices[vertexIndex + 0].v.ob[0] = startX;
        sRdpTmemBarVertices[vertexIndex + 1].v.ob[0] = barLength;
        sRdpTmemBarVertices[vertexIndex + 2].v.ob[0] = barLength;
        sRdpTmemBarVertices[vertexIndex + 3].v.ob[0] = startX;
        gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0, vertexIndex + 0, vertexIndex + 3,
                      vertexIndex + 2, 0);
        startX += barLength;
    }

    if (readoutMode & METER_READOUT_FREE_MEMORY) {
        sprintf(sReadoutText, "%06d", GetFreeMemorySize());
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, sReadoutText);
    } else if (readoutMode & METER_READOUT_CURRENT) {
        sprintf(sReadoutText, "%05d", cpuBarLength);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, sReadoutText);
    } else if (readoutMode & METER_READOUT_MAXIMUM) {
        sprintf(sReadoutText, "%05d", sCpuBarMaximum);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, sReadoutText);
    } else if (readoutMode & METER_READOUT_AVERAGE) {
        sprintf(sReadoutText, "%05d", sCpuBarAverage);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, sReadoutText);
    }

    if (readoutMode & METER_READOUT_CURRENT) {
        sprintf(sReadoutText, "%05d", rspBarLength);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT, sReadoutText);
    } else if (readoutMode & METER_READOUT_MAXIMUM) {
        sprintf(sReadoutText, "%05d", sRspBarMaximum);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT, sReadoutText);
    } else if (readoutMode & METER_READOUT_AVERAGE) {
        sprintf(sReadoutText, "%05d", sRspBarAverage);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT, sReadoutText);
    }

    if (readoutMode & METER_READOUT_CURRENT) {
        sprintf(sReadoutText, "%05d", rdpBarLength);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT * 2, sReadoutText);
    } else if (readoutMode & METER_READOUT_MAXIMUM) {
        sprintf(sReadoutText, "%05d", sRdpBarMaximum);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT * 2, sReadoutText);
    } else if (readoutMode & METER_READOUT_AVERAGE) {
        sprintf(sReadoutText, "%05d", sRdpBarAverage);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT * 2, sReadoutText);
    }

    gDPFullSync(gfx++);
    gSPEndDisplayList(gfx++);
    nuGfxTaskStart(sMeterDisplayLists[sMeterDisplayListIndex],
                   (s32)(gfx - sMeterDisplayLists[sMeterDisplayListIndex]) * sizeof(Gfx),
                   NU_GFX_UCODE_F3DEX, taskFlags);
    sMeterDisplayListIndex++;
    sMeterDisplayListIndex &= 1;
}

/*
 * Prints the framebuffer count, the free memory and the largest free block,
 * and starts the display list as its own graphics task.
 */
void DrawMemoryMeter(u32 y, s32 taskFlags) {
    Gfx *gfx = sMeterDisplayLists[sMeterDisplayListIndex];

    gSPDisplayList(gfx++, OS_K0_TO_PHYSICAL(sInitRcpDisplayList));
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 191);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, 250 << 2, (y - 8) << 2, 302 << 2, (y + 20) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                        1 << 10);
    sprintf(sReadoutText, "% 8d", nuGfxCfbNum);
    DrawDigits(&gfx, 252, y - 7, sReadoutText);
    sprintf(sReadoutText, "%08d", GetFreeMemorySize());
    DrawDigits(&gfx, 252, y + 2, sReadoutText);
    sprintf(sReadoutText, "%08d", FindLargestFreeMemorySize());
    DrawDigits(&gfx, 252, y + 10, sReadoutText);
    gDPFullSync(gfx++);
    gSPEndDisplayList(gfx++);
    nuGfxTaskStart(sMeterDisplayLists[sMeterDisplayListIndex],
                   (s32)(gfx - sMeterDisplayLists[sMeterDisplayListIndex]) * sizeof(Gfx),
                   NU_GFX_UCODE_F3DEX, taskFlags);
    sMeterDisplayListIndex = (sMeterDisplayListIndex + 1) & 1;
}
