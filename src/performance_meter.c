#include "performance_meter.h"

#include <nusys.h>
#include <ultra64.h>

#include "math/geometry.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"

#define FRAME_WIDTH 280
#define FRAME_MICROSECONDS 16666
#define EXTRA_FRAME_COUNT_MAX 5

#define RdpCountToMicroseconds(count) ((count) * 1643 / 100000)

#define BAR_START_X 1
#define METER_TOP_Y 24

#define READOUT_X 264
#define READOUT_TOP_Y 2
#define READOUT_LINE_HEIGHT 8
#define READOUT_TEXT_CAPACITY 32
#define MEMORY_BOX_LEFT 250
#define MEMORY_BOX_RIGHT 302
#define MEMORY_DIGITS_X (MEMORY_BOX_LEFT + 2)
#define MEMORY_BOX_TOP_OFFSET (-8)
#define MEMORY_BOX_BOTTOM_OFFSET 20
#define FRAMEBUFFER_COUNT_OFFSET (-7)
#define FREE_MEMORY_OFFSET 2
#define LARGEST_BLOCK_OFFSET 10

#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 8
#define DIGIT_SPACING 6

#define BAR_QUAD(topY, bottomY, z, leftRed, leftGreen, leftBlue, rightRed, rightGreen, rightBlue) \
    {{{0, topY, z}, 0, {0, 0}, {leftRed, leftGreen, leftBlue, 0xFF}}}, \
        {{{0, topY, z}, 0, {0, 0}, {rightRed, rightGreen, rightBlue, 0xFF}}}, \
        {{{0, bottomY, z}, 0, {0, 0}, {rightRed, rightGreen, rightBlue, 0xFF}}}, { \
        { \
            {0, bottomY, z}, 0, {0, 0}, {leftRed, leftGreen, leftBlue, 0xFF} \
        } \
    }
#define CPU_BAR_QUAD BAR_QUAD(22, 20, -1, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF)
#define AUDIO_BAR_QUAD BAR_QUAD(19, 17, -1, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF)
#define RSP_BAR_QUAD BAR_QUAD(16, 14, 0, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x00)
#define RDP_BAR_QUAD BAR_QUAD(13, 11, 0, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF)
#define RDP_COMMAND_BAR_QUAD BAR_QUAD(10, 8, 0, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00)
#define RDP_PIPE_BAR_QUAD BAR_QUAD(7, 5, 0, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00)
#define RDP_TMEM_BAR_QUAD BAR_QUAD(4, 2, 0, 0x80, 0x00, 0xFF, 0x80, 0xFF, 0x00)

static Vtx f_FrameVertices[] = {
    {{{0, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{1962, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{1962, 24, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{0, 24, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{1, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x40, 0x80}}},
    {{{281, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x40, 0x80}}},
    {{{281, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x40, 0x80}}},
    {{{1, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x40, 0x80}}},
    {{{281, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{561, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{561, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{281, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{561, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{841, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{841, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{561, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{841, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{1121, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{1121, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{841, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{1121, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{1401, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{1401, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{1121, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{1401, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{1681, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{1681, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{1401, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0xC0}}},
    {{{1681, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{1961, -2, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{1961, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
    {{{1681, 23, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x80}}},
};
static Vtx f_CpuBarVertices[QUAD_VERTEX_COUNT] = {
    CPU_BAR_QUAD,
};
static Vtx f_AudioBarVertices[] = {
    AUDIO_BAR_QUAD,
    AUDIO_BAR_QUAD,
    AUDIO_BAR_QUAD,
    AUDIO_BAR_QUAD,
};
static Vtx f_RspBarVertices[] = {
    RSP_BAR_QUAD,
    RSP_BAR_QUAD,
    RSP_BAR_QUAD,
    RSP_BAR_QUAD,
    RSP_BAR_QUAD,
    RSP_BAR_QUAD,
    RSP_BAR_QUAD,
    RSP_BAR_QUAD,
};
static Vtx f_RdpBarVertices[] = {
    RDP_BAR_QUAD,
    RDP_BAR_QUAD,
    RDP_BAR_QUAD,
    RDP_BAR_QUAD,
    RDP_BAR_QUAD,
    RDP_BAR_QUAD,
    RDP_BAR_QUAD,
    RDP_BAR_QUAD,
};
static Vtx f_RdpCommandBarVertices[] = {
    RDP_COMMAND_BAR_QUAD,
    RDP_COMMAND_BAR_QUAD,
    RDP_COMMAND_BAR_QUAD,
    RDP_COMMAND_BAR_QUAD,
    RDP_COMMAND_BAR_QUAD,
    RDP_COMMAND_BAR_QUAD,
    RDP_COMMAND_BAR_QUAD,
    RDP_COMMAND_BAR_QUAD,
};
static Vtx f_RdpPipeBarVertices[] = {
    RDP_PIPE_BAR_QUAD,
    RDP_PIPE_BAR_QUAD,
    RDP_PIPE_BAR_QUAD,
    RDP_PIPE_BAR_QUAD,
    RDP_PIPE_BAR_QUAD,
    RDP_PIPE_BAR_QUAD,
    RDP_PIPE_BAR_QUAD,
    RDP_PIPE_BAR_QUAD,
};
static Vtx f_RdpTmemBarVertices[] = {
    RDP_TMEM_BAR_QUAD,
    RDP_TMEM_BAR_QUAD,
    RDP_TMEM_BAR_QUAD,
    RDP_TMEM_BAR_QUAD,
    RDP_TMEM_BAR_QUAD,
    RDP_TMEM_BAR_QUAD,
    RDP_TMEM_BAR_QUAD,
    RDP_TMEM_BAR_QUAD,
};
static Vp f_MeterViewport = {
    {
        {SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, G_MAXZ / 2, 0},
        {SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, G_MAXZ / 2, 0},
    },
};
static Gfx f_InitRcpDisplayList[] = {
    gsSPSegment(0x00, 0x00000000),
    gsSPGeometryMode(~0, 0),
    gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH),
    gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF),
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPPipeSync(),
    gsDPPipelineMode(G_PM_NPRIMITIVE),
    gsDPPipeSync(),
    gsDPSetColorDither(G_CD_BAYER),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF2),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsDPPipeSync(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};
static Gfx f_FrameDisplayList[] = {
    gsSPVertex(f_FrameVertices, ARRAY_COUNT(f_FrameVertices), 0),
    gsSP2Triangles(0, 1, 2, 0, 2, 3, 0, 0),
    gsSPEndDisplayList(),
};
static f32 f_FrameScales[EXTRA_FRAME_COUNT_MAX + 1] = {1.0f, 0.5f, 0.3333f, 0.25f, 0.2f, 0.16667f};
static u32 f_MeterDisplayListIndex = 0;
static Gfx f_UnusedMeterDisplayList[] UNUSED = {
    gsSPEndDisplayList(),
};
static u16 f_DigitTexture[] = {
#include "assets/performance_meter/digit_texture.ia4.inc.c"
};
static u32 f_DigitDrawCount = 0;
static u32 f_CpuBarMaximum = 0;
static u32 f_CpuBarAverage = 0;
static u32 f_AudioBarMaximum = 0;
static u32 f_AudioBarAverage = 0;
static u32 f_RspBarMaximum = 0;
static u32 f_RspBarAverage = 0;
static u32 f_RdpBarMaximum = 0;
static u32 f_RdpBarAverage = 0;
static u32 f_RdpCommandBarMaximum = 0;
static u32 f_RdpCommandBarAverage = 0;
static u32 f_RdpPipeBarMaximum = 0;
static u32 f_RdpPipeBarAverage = 0;
static u32 f_RdpTmemBarMaximum = 0;
static u32 f_RdpTmemBarAverage = 0;
static Mtx f_MeterProjectionMatrix;
static Mtx f_MeterViewMatrix;
static Mtx f_MeterIdentityMatrix;
static Mtx f_MeterScaleMatrix;
static Mtx f_MeterTranslationMatrix;
static Gfx f_MeterDisplayLists[2][512];
static char f_ReadoutText[READOUT_TEXT_CAPACITY];

static void DrawDigits(Gfx **displayListHead, u16 x, u16 y, char *text) {
    Gfx *gfx = *displayListHead;
    char character;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 200, 0, OPAQUE_ALPHA);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(f_DigitTexture),
        G_IM_FMT_IA,
        64,
        8,
        0,
        G_TX_WRAP,
        G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    f_DigitDrawCount = (f_DigitDrawCount + 1) & 0x3FF;
    gDPPipeSync(gfx++);

    for (; (character = *text) != '\0'; text++) {
        if (character != ' ') {
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(x),
                QuarterPixels(y),
                QuarterPixels(x + DIGIT_WIDTH),
                QuarterPixels(y + DIGIT_HEIGHT),
                G_TX_RENDERTILE,
                TexelCoordinate(((character - '0') & 0xF) * DIGIT_WIDTH),
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        x += DIGIT_SPACING;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define KeepBarStats(average, maximum, length) \
    { \
        if ((average) == 0) { \
            (average) = (length); \
        } \
        else { \
            (average) = ((average) + (length)) >> 1; \
        } \
\
        if ((maximum) < (u32)(length)) { \
            (maximum) = (length); \
        } \
    }

void DrawPerformanceMeter(
    u32 extraFrameCount,
    u32 y,
    u32 cpuStartTime,
    u32 cpuEndTime,
    s32 taskFlags,
    s32 readoutMode
) {
    u32 i;
    s64 taskTime;
    s16 startX;
    s16 barLength;
    s32 cpuBarLength;
    s32 rspBarLength;
    s32 rdpBarLength;
    u32 vertexIndex;

    s32 audioBarLength UNUSED;
    s32 rdpCommandBarLength UNUSED;
    s32 rdpPipeBarLength UNUSED;
    s32 rdpTmemBarLength UNUSED;
    f32 translationY;
    Gfx *gfx;
    u32 viMode;
    u32 scale;

#ifdef AVOID_UB
    scale = 1;
    rspBarLength = 0;
    rdpBarLength = 0;
#endif
    gfx = f_MeterDisplayLists[f_MeterDisplayListIndex];
    viMode = osViGetCurrentMode();
    if (viMode < OS_VI_NTSC_HPN1) {
        scale = 1;
    }
    else if (viMode <= OS_VI_NTSC_HPF2) {
        y /= 2;
        scale = 2;
    }

    f_MeterViewport.vp.vscale[0] = f_MeterViewport.vp.vtrans[0] = SCREEN_WIDTH * 2 * scale;
    f_MeterViewport.vp.vscale[1] = f_MeterViewport.vp.vtrans[1] = SCREEN_HEIGHT * 2 * scale;
    gSPViewport(gfx++, &f_MeterViewport);
    gSPDisplayList(gfx++, PhysicalAddress(f_InitRcpDisplayList));
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH * scale,
        RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
    );

    translationY = (240.0 - METER_TOP_Y) - y;
    if (translationY == 0.0) {
        translationY = 0.1f;
    }

    if (extraFrameCount > EXTRA_FRAME_COUNT_MAX) {
        extraFrameCount = EXTRA_FRAME_COUNT_MAX;
    }

    guOrtho(
        &f_MeterProjectionMatrix,
        0.0f,
        (f32)(SCREEN_WIDTH * scale),
        0.0f,
        (f32)(SCREEN_HEIGHT * scale),
        -1.0f,
        10.0f,
        1.0f
    );
    guLookAt(&f_MeterViewMatrix, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    guScale(&f_MeterScaleMatrix, f_FrameScales[extraFrameCount] * (f32)scale, (f32)scale, 1.0f);
    guTranslate(
        &f_MeterTranslationMatrix,
        20.0 / f_FrameScales[extraFrameCount],
        translationY,
        0.0f
    );
    gSPMatrix(gfx++, &f_MeterProjectionMatrix, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &f_MeterViewMatrix, G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
    guMtxIdent(&f_MeterIdentityMatrix);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_MeterIdentityMatrix),
        G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_MeterScaleMatrix),
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_MeterTranslationMatrix),
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
    );

    gSPDisplayList(gfx++, PhysicalAddress(f_FrameDisplayList));
    f_FrameVertices[1].v.ob[AXIS_X] = (FRAME_WIDTH * (extraFrameCount + 1)) + 2;
    f_FrameVertices[2].v.ob[AXIS_X] = (FRAME_WIDTH * (extraFrameCount + 1)) + 2;
    for (i = 0; i < extraFrameCount + 1; i++) {
        vertexIndex = (i * QUAD_VERTEX_COUNT) + QUAD_VERTEX_COUNT;
        gSP2Triangles(
            gfx++,
            vertexIndex + 0,
            vertexIndex + 1,
            vertexIndex + 2,
            0,
            vertexIndex + 2,
            vertexIndex + 3,
            vertexIndex + 0,
            0
        );
    }

    gSPVertex(gfx++, f_CpuBarVertices, QUAD_VERTEX_COUNT, 0);
    startX = BAR_START_X;
    taskTime = cpuEndTime - cpuStartTime;
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    cpuBarLength = barLength;
    KeepBarStats(f_CpuBarAverage, f_CpuBarMaximum, barLength);

    f_CpuBarVertices[0].v.ob[AXIS_X] = startX;
    f_CpuBarVertices[1].v.ob[AXIS_X] = startX + barLength;
    f_CpuBarVertices[2].v.ob[AXIS_X] = startX + barLength;
    f_CpuBarVertices[3].v.ob[AXIS_X] = startX;
    gSP2Triangles(gfx++, 0, 2, 1, 0, 0, 3, 2, 0);

    gSPVertex(gfx++, f_AudioBarVertices, ARRAY_COUNT(f_AudioBarVertices), 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->auTaskCnt; i++) {
        vertexIndex = i * QUAD_VERTEX_COUNT;
        taskTime =
            nuDebTaskPerfPtr->auTaskTime[i].rspEnd - nuDebTaskPerfPtr->auTaskTime[i].rspStart;
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        audioBarLength = barLength;
        KeepBarStats(f_AudioBarAverage, f_AudioBarMaximum, barLength);

        f_AudioBarVertices[vertexIndex + 0].v.ob[AXIS_X] = startX;
        f_AudioBarVertices[vertexIndex + 1].v.ob[AXIS_X] = startX + barLength;
        f_AudioBarVertices[vertexIndex + 2].v.ob[AXIS_X] = startX + barLength;
        f_AudioBarVertices[vertexIndex + 3].v.ob[AXIS_X] = startX;
        gSP2Triangles(
            gfx++,
            vertexIndex + 0,
            vertexIndex + 2,
            vertexIndex + 1,
            0,
            vertexIndex + 0,
            vertexIndex + 3,
            vertexIndex + 2,
            0
        );
        startX += barLength;
    }

    gSPVertex(gfx++, f_RspBarVertices, ARRAY_COUNT(f_RspBarVertices), 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * QUAD_VERTEX_COUNT;
        taskTime =
            nuDebTaskPerfPtr->gfxTaskTime[i].rspEnd - nuDebTaskPerfPtr->gfxTaskTime[i].rspStart;
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rspBarLength = barLength;
        KeepBarStats(f_RspBarAverage, f_RspBarMaximum, barLength);

        f_RspBarVertices[vertexIndex + 0].v.ob[AXIS_X] = startX;
        f_RspBarVertices[vertexIndex + 1].v.ob[AXIS_X] = startX + barLength;
        f_RspBarVertices[vertexIndex + 2].v.ob[AXIS_X] = startX + barLength;
        f_RspBarVertices[vertexIndex + 3].v.ob[AXIS_X] = startX;
        gSP2Triangles(
            gfx++,
            vertexIndex + 0,
            vertexIndex + 2,
            vertexIndex + 1,
            0,
            vertexIndex + 0,
            vertexIndex + 3,
            vertexIndex + 2,
            0
        );
        startX += barLength;
    }

    gSPVertex(gfx++, f_RdpBarVertices, ARRAY_COUNT(f_RdpBarVertices), 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * QUAD_VERTEX_COUNT;
        taskTime =
            nuDebTaskPerfPtr->gfxTaskTime[i].rdpEnd - nuDebTaskPerfPtr->gfxTaskTime[i].rspStart;
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpBarLength = barLength;
        KeepBarStats(f_RdpBarAverage, f_RdpBarMaximum, barLength);

        f_RdpBarVertices[vertexIndex + 0].v.ob[AXIS_X] = startX;
        f_RdpBarVertices[vertexIndex + 1].v.ob[AXIS_X] = barLength;
        f_RdpBarVertices[vertexIndex + 2].v.ob[AXIS_X] = barLength;
        f_RdpBarVertices[vertexIndex + 3].v.ob[AXIS_X] = startX;
        gSP2Triangles(
            gfx++,
            vertexIndex + 0,
            vertexIndex + 2,
            vertexIndex + 1,
            0,
            vertexIndex + 0,
            vertexIndex + 3,
            vertexIndex + 2,
            0
        );
        startX += barLength;
    }

    gSPVertex(gfx++, f_RdpCommandBarVertices, ARRAY_COUNT(f_RdpCommandBarVertices), 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * QUAD_VERTEX_COUNT;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_CMD_CTR];
        taskTime = RdpCountToMicroseconds(taskTime);
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpCommandBarLength = barLength;
        KeepBarStats(f_RdpCommandBarAverage, f_RdpCommandBarMaximum, barLength);

        f_RdpCommandBarVertices[vertexIndex + 0].v.ob[AXIS_X] = startX;
        f_RdpCommandBarVertices[vertexIndex + 1].v.ob[AXIS_X] = barLength;
        f_RdpCommandBarVertices[vertexIndex + 2].v.ob[AXIS_X] = barLength;
        f_RdpCommandBarVertices[vertexIndex + 3].v.ob[AXIS_X] = startX;
        gSP2Triangles(
            gfx++,
            vertexIndex + 0,
            vertexIndex + 2,
            vertexIndex + 1,
            0,
            vertexIndex + 0,
            vertexIndex + 3,
            vertexIndex + 2,
            0
        );
        startX += barLength;
    }

    gSPVertex(gfx++, f_RdpPipeBarVertices, ARRAY_COUNT(f_RdpPipeBarVertices), 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * QUAD_VERTEX_COUNT;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_PIPE_CTR];
        taskTime = RdpCountToMicroseconds(taskTime);
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpPipeBarLength = barLength;
        KeepBarStats(f_RdpPipeBarAverage, f_RdpPipeBarMaximum, barLength);

        f_RdpPipeBarVertices[vertexIndex + 0].v.ob[AXIS_X] = startX;
        f_RdpPipeBarVertices[vertexIndex + 1].v.ob[AXIS_X] = barLength;
        f_RdpPipeBarVertices[vertexIndex + 2].v.ob[AXIS_X] = barLength;
        f_RdpPipeBarVertices[vertexIndex + 3].v.ob[AXIS_X] = startX;
        gSP2Triangles(
            gfx++,
            vertexIndex + 0,
            vertexIndex + 2,
            vertexIndex + 1,
            0,
            vertexIndex + 0,
            vertexIndex + 3,
            vertexIndex + 2,
            0
        );
        startX += barLength;
    }

    gSPVertex(gfx++, f_RdpTmemBarVertices, ARRAY_COUNT(f_RdpTmemBarVertices), 0);
    startX = BAR_START_X;
    for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
        vertexIndex = i * QUAD_VERTEX_COUNT;
        taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_TMEM_CTR];
        taskTime = RdpCountToMicroseconds(taskTime);
        barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
        rdpTmemBarLength = barLength;
        KeepBarStats(f_RdpTmemBarAverage, f_RdpTmemBarMaximum, barLength);

        f_RdpTmemBarVertices[vertexIndex + 0].v.ob[AXIS_X] = startX;
        f_RdpTmemBarVertices[vertexIndex + 1].v.ob[AXIS_X] = barLength;
        f_RdpTmemBarVertices[vertexIndex + 2].v.ob[AXIS_X] = barLength;
        f_RdpTmemBarVertices[vertexIndex + 3].v.ob[AXIS_X] = startX;
        gSP2Triangles(
            gfx++,
            vertexIndex + 0,
            vertexIndex + 2,
            vertexIndex + 1,
            0,
            vertexIndex + 0,
            vertexIndex + 3,
            vertexIndex + 2,
            0
        );
        startX += barLength;
    }

    if (readoutMode & METER_READOUT_FREE_MEMORY) {
        sprintf(f_ReadoutText, "%06d", (int)(GetFreeMemorySize()));
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
    }
    else if (readoutMode & METER_READOUT_CURRENT) {
        sprintf(f_ReadoutText, "%05d", (int)cpuBarLength);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
    }
    else if (readoutMode & METER_READOUT_MAXIMUM) {
        sprintf(f_ReadoutText, "%05d", (int)f_CpuBarMaximum);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
    }
    else if (readoutMode & METER_READOUT_AVERAGE) {
        sprintf(f_ReadoutText, "%05d", (int)f_CpuBarAverage);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
    }

    if (readoutMode & METER_READOUT_CURRENT) {
        sprintf(f_ReadoutText, "%05d", (int)rspBarLength);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT, f_ReadoutText);
    }
    else if (readoutMode & METER_READOUT_MAXIMUM) {
        sprintf(f_ReadoutText, "%05d", (int)f_RspBarMaximum);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT, f_ReadoutText);
    }
    else if (readoutMode & METER_READOUT_AVERAGE) {
        sprintf(f_ReadoutText, "%05d", (int)f_RspBarAverage);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT, f_ReadoutText);
    }

    if (readoutMode & METER_READOUT_CURRENT) {
        sprintf(f_ReadoutText, "%05d", (int)rdpBarLength);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + (READOUT_LINE_HEIGHT * 2), f_ReadoutText);
    }
    else if (readoutMode & METER_READOUT_MAXIMUM) {
        sprintf(f_ReadoutText, "%05d", (int)f_RdpBarMaximum);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + (READOUT_LINE_HEIGHT * 2), f_ReadoutText);
    }
    else if (readoutMode & METER_READOUT_AVERAGE) {
        sprintf(f_ReadoutText, "%05d", (int)f_RdpBarAverage);
        DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + (READOUT_LINE_HEIGHT * 2), f_ReadoutText);
    }

    gDPFullSync(gfx++);
    gSPEndDisplayList(gfx++);
    nuGfxTaskStart(
        f_MeterDisplayLists[f_MeterDisplayListIndex],
        (s32)(gfx - f_MeterDisplayLists[f_MeterDisplayListIndex]) * sizeof(Gfx),
        NU_GFX_UCODE_F3DEX,
        taskFlags
    );
    f_MeterDisplayListIndex++;
    f_MeterDisplayListIndex &= 1;
}

void DrawMemoryMeter(u32 y, s32 taskFlags) {
    Gfx *gfx = f_MeterDisplayLists[f_MeterDisplayListIndex];

    gSPDisplayList(gfx++, PhysicalAddress(f_InitRcpDisplayList));
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
    );
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
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(MEMORY_BOX_LEFT),
        QuarterPixels(y + MEMORY_BOX_TOP_OFFSET),
        QuarterPixels(MEMORY_BOX_RIGHT),
        QuarterPixels(y + MEMORY_BOX_BOTTOM_OFFSET),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    sprintf(f_ReadoutText, "% 8d", (int)nuGfxCfbNum);
    DrawDigits(&gfx, MEMORY_DIGITS_X, y + FRAMEBUFFER_COUNT_OFFSET, f_ReadoutText);
    sprintf(f_ReadoutText, "%08d", (int)(GetFreeMemorySize()));
    DrawDigits(&gfx, MEMORY_DIGITS_X, y + FREE_MEMORY_OFFSET, f_ReadoutText);
    sprintf(f_ReadoutText, "%08d", (int)(FindLargestFreeMemorySize()));
    DrawDigits(&gfx, MEMORY_DIGITS_X, y + LARGEST_BLOCK_OFFSET, f_ReadoutText);
    gDPFullSync(gfx++);
    gSPEndDisplayList(gfx++);
    nuGfxTaskStart(
        f_MeterDisplayLists[f_MeterDisplayListIndex],
        (s32)(gfx - f_MeterDisplayLists[f_MeterDisplayListIndex]) * sizeof(Gfx),
        NU_GFX_UCODE_F3DEX,
        taskFlags
    );
    f_MeterDisplayListIndex = (f_MeterDisplayListIndex + 1) & 1;
}
