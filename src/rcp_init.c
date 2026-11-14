#include "rcp_init.h"

#include <nusys.h>
#include <ultra64.h>

#include "address.h"
#include "cpu_projection.h"
#include "game.h"
#include "gbi_helpers.h"
#include "screen.h"

#define DEPTH_MAX 0x3FFF

static Gfx f_RdpInitList[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_OPA_SURF, G_RM_OPA_SURF2),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsDPPipeSync(),
    gsDPSetColorDither(G_CD_MAGICSQ),
    gsSPEndDisplayList(),
};
static Gfx f_RspInitList[] = {
    gsSPLoadGeometryMode(0),
    gsSPSetGeometryMode(G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH),
    gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF),
    gsSPEndDisplayList(),
};

static s32 f_FillColor[RGB_CHANNEL_COUNT] = {0};

void AppendRcpInit(s32 viewportIndex, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gSPSegment(gfx++, 0, 0);
    gSPViewport(gfx++, &g_Viewports[viewportIndex]);
    SetCpuProjection(NULL, NULL, &g_Viewports[viewportIndex]);
    gSPDisplayList(gfx++, f_RspInitList);
    gSPDisplayList(gfx++, f_RdpInitList);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void InitRcp(s32 viewportIndex) {
    AppendRcpInit(viewportIndex, &g_DisplayListHead);
}

void SetFillColor(s32 red, s32 green, s32 blue) {
    f_FillColor[RED_CHANNEL] = red;
    f_FillColor[GREEN_CHANNEL] = green;
    f_FillColor[BLUE_CHANNEL] = blue;
}

void FillScreen(void) {
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
    gDPSetFillColor(
        g_DisplayListHead++,
        FillColor16(
            f_FillColor[RED_CHANNEL],
            f_FillColor[GREEN_CHANNEL],
            f_FillColor[BLUE_CHANNEL],
            1
        )
    );
    gDPPipeSync(g_DisplayListHead++);
    gDPFillRectangle(
        g_DisplayListHead++,
        SCREEN_BORDER,
        SCREEN_BORDER,
        SCREEN_WIDTH - SCREEN_BORDER - 1,
        SCREEN_HEIGHT - SCREEN_BORDER - 1
    );
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
}

void AppendBufferClear(BufferClear clearMode, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 line;

    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetDepthImage(gfx++, RenderTargetAddress(PhysicalAddress(nuGfxZBuffer)));
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        RenderTargetAddress(PhysicalAddress(nuGfxZBuffer))
    );
    gDPSetFillColor(gfx++, GPACK_ZDZ(DEPTH_MAX, 0) << 16 | GPACK_ZDZ(DEPTH_MAX, 0));
    gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
    );
    if (clearMode == BUFFER_CLEAR_BY_LINE) {
        for (line = 0; line < SCREEN_HEIGHT - 1; line++) {
            gDPSetFillColor(gfx++, FillColor16(0, 0, 0, 1));
            gDPFillRectangle(gfx++, 0, line, SCREEN_WIDTH - 1, line);
        }

        gDPPipeSync(gfx++);
    }
    else if (clearMode == BUFFER_CLEAR_WHOLE) {
        gDPSetFillColor(gfx++, FillColor16(0, 0, 0, 1));
        gDPPipeSync(gfx++);
        gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void ClearBuffers(BufferClear clearMode) {
    AppendBufferClear(clearMode, &g_DisplayListHead);
}
