#include "common.h"
#include "functions.h"
#include "variables.h"

/* Screen sizes, in pixels */
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

/* The border around the main camera's scissor rectangle, in pixels */
#define SCREEN_BORDER 8

/* The farthest depth the 14-bit depth buffer holds */
#define MAX_DEPTH 0x3FFF

/* Set up the RDP and the RSP for a frame */
extern Gfx sRdpInitList[];
extern Gfx sRspInitList[];

/* The red, green and blue FillScreen fills with, 8 bits each */
extern s32 sFillColor[3];

void AppendRcpInit(s32 viewportIndex, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gSPSegment(gfx++, 0, 0);
    gSPViewport(gfx++, &g_Viewports[viewportIndex]);
    SetCpuProjection(NULL, NULL, &g_Viewports[viewportIndex]);
    gSPDisplayList(gfx++, sRdpInitList);
    gSPDisplayList(gfx++, sRspInitList);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void InitRcp(s32 viewportIndex) {
    AppendRcpInit(viewportIndex, &g_DisplayListHead);
}

void SetFillColor(s32 red, s32 green, s32 blue) {
    sFillColor[0] = red;
    sFillColor[1] = green;
    sFillColor[2] = blue;
}

/* Fills the screen inside its border with the color from SetFillColor */
void FillScreen(void) {
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
    gDPSetFillColor(g_DisplayListHead++, GPACK_RGBA5551(sFillColor[0], sFillColor[1], sFillColor[2], 1) << 16 |
                                             GPACK_RGBA5551(sFillColor[0], sFillColor[1], sFillColor[2], 1));
    gDPPipeSync(g_DisplayListHead++);
    gDPFillRectangle(g_DisplayListHead++, SCREEN_BORDER, SCREEN_BORDER, SCREEN_WIDTH - SCREEN_BORDER - 1,
                     SCREEN_HEIGHT - SCREEN_BORDER - 1);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
}

/* Clears the depth buffer and, depending on clearMode, the frame */
void AppendBufferClear(s32 clearMode, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 line;

    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetDepthImage(gfx++, OS_K0_TO_PHYSICAL(nuGfxZBuffer) & ~7);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, OS_K0_TO_PHYSICAL(nuGfxZBuffer) & ~7);
    gDPSetFillColor(gfx++, GPACK_ZDZ(MAX_DEPTH, 0) << 16 | GPACK_ZDZ(MAX_DEPTH, 0));
    gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
    if (clearMode == BUFFER_CLEAR_BY_LINE) {
        /* Stops a line short, which the screen's border hides */
        for (line = 0; line < SCREEN_HEIGHT - 1; line++) {
            gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
            gDPFillRectangle(gfx++, 0, line, SCREEN_WIDTH - 1, line);
        }
        gDPPipeSync(gfx++);
    } else if (clearMode == BUFFER_CLEAR_WHOLE) {
        gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
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

void ClearBuffers(s32 clearMode) {
    AppendBufferClear(clearMode, &g_DisplayListHead);
}
