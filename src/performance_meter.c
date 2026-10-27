#include "common.h"

#include "performance_meter.h"

#include <lib/libnusys/nusys.h>

#include "heap.h"

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

extern Vtx f_FrameVertices[];
extern Vtx f_CpuBarVertices[];
extern Vtx f_AudioBarVertices[];
extern Vtx f_RspBarVertices[];
extern Vtx f_RdpBarVertices[];
extern Vtx f_RdpCommandBarVertices[];
extern Vtx f_RdpPipeBarVertices[];
extern Vtx f_RdpTmemBarVertices[];
extern Vp f_MeterViewport;
extern Gfx f_InitRcpDisplayList[];
extern Gfx f_FrameDisplayList[];
extern f32 f_FrameScales[];
extern u32 f_MeterDisplayListIndex;
extern u8 f_DigitTexture[];
/* Counts the digit strings drawn, modulo 1024; nothing else reads it */
extern u32 f_DigitDrawCount;
extern u32 f_CpuBarMaximum;
extern u32 f_CpuBarAverage;
extern u32 f_AudioBarMaximum;
extern u32 f_AudioBarAverage;
extern u32 f_RspBarMaximum;
extern u32 f_RspBarAverage;
extern u32 f_RdpBarMaximum;
extern u32 f_RdpBarAverage;
extern u32 f_RdpCommandBarMaximum;
extern u32 f_RdpCommandBarAverage;
extern u32 f_RdpPipeBarMaximum;
extern u32 f_RdpPipeBarAverage;
extern u32 f_RdpTmemBarMaximum;
extern u32 f_RdpTmemBarAverage;
extern Mtx f_MeterProjectionMatrix;
extern Mtx f_MeterViewMatrix;
extern Mtx f_MeterIdentityMatrix;
extern Mtx f_MeterScaleMatrix;
extern Mtx f_MeterTranslationMatrix;
extern Gfx f_MeterDisplayLists[2][512];
extern char f_ReadoutText[];

/* Draws text made of digits and spaces into *gfxP at the pixel x, y */
static void DrawDigits(Gfx** gfxP, u16 x, u16 y, char* text) {
  Gfx* gfx = *gfxP;
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
  gDPLoadTextureBlock_4b(gfx++, (u32)f_DigitTexture & ~7, G_IM_FMT_IA, 64, 8, 0,
                         G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                         G_TX_NOLOD, G_TX_NOLOD);
  f_DigitDrawCount = (f_DigitDrawCount + 1) & 0x3FF;
  gDPPipeSync(gfx++);

  for (; (character = *text) != '\0'; text++, x += DIGIT_SPACING) {
    if (character != ' ') {
      gSPTextureRectangle(gfx++, x << 2, y << 2, (x + DIGIT_WIDTH) << 2,
                          (y + DIGIT_HEIGHT) << 2, G_TX_RENDERTILE,
                          ((character - '0') & 0xF) * DIGIT_WIDTH << 5, 0,
                          1 << 10, 1 << 10);
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
void DrawPerformanceMeter(u32 extraFrameCount, u32 y, u32 cpuStartTime,
                          u32 cpuEndTime, s32 taskFlags, s32 readoutMode) {
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
  Gfx* gfx;
  u32 viMode;
  u32 scale;

  gfx = f_MeterDisplayLists[f_MeterDisplayListIndex];
  viMode = osViGetCurrentMode();
  if (viMode < OS_VI_NTSC_HPN1) {
    scale = 1;
  } else if (viMode <= OS_VI_NTSC_HPF2) {
    y /= 2;
    scale = 2;
  }

  f_MeterViewport.vp.vscale[0] = f_MeterViewport.vp.vtrans[0] = 320 * 2 * scale;
  f_MeterViewport.vp.vscale[1] = f_MeterViewport.vp.vtrans[1] = 240 * 2 * scale;
  gSPViewport(gfx++, &f_MeterViewport);
  gSPDisplayList(gfx++, OS_K0_TO_PHYSICAL(f_InitRcpDisplayList));
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320 * scale,
                   osVirtualToPhysical(nuGfxCfb_ptr) & ~7);

  /* The projection's origin is at the bottom left, so y is flipped */
  posY = (240.0 - METER_TOP_Y) - y;
  if (posY == 0.0) {
    posY = 0.1f;
  }
  if (extraFrameCount > MAX_EXTRA_FRAME_COUNT) {
    extraFrameCount = MAX_EXTRA_FRAME_COUNT;
  }

  guOrtho(&f_MeterProjectionMatrix, 0.0f, (f32)(320 * scale), 0.0f,
          (f32)(240 * scale), -1.0f, 10.0f, 1.0f);
  guLookAt(&f_MeterViewMatrix, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
           0.0f);
  guScale(&f_MeterScaleMatrix, f_FrameScales[extraFrameCount] * (f32)scale,
          (f32)scale, 1.0f);
  /* Moves the meter 20 pixels right of the screen's left edge */
  guTranslate(&f_MeterTranslationMatrix, 20.0 / f_FrameScales[extraFrameCount],
              posY, 0.0f);
  gSPMatrix(gfx++, &f_MeterProjectionMatrix,
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(gfx++, &f_MeterViewMatrix,
            G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
  guMtxIdent(&f_MeterIdentityMatrix);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_MeterIdentityMatrix),
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_MeterScaleMatrix),
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_MeterTranslationMatrix),
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);

  gSPDisplayList(gfx++, OS_K0_TO_PHYSICAL(f_FrameDisplayList));
  f_FrameVertices[1].v.ob[0] = FRAME_WIDTH * (extraFrameCount + 1) + 2;
  f_FrameVertices[2].v.ob[0] = FRAME_WIDTH * (extraFrameCount + 1) + 2;
  for (i = 0; i < extraFrameCount + 1; i++) {
    vertexIndex = i * 4 + 4;
    gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 1, vertexIndex + 2, 0,
                  vertexIndex + 2, vertexIndex + 3, vertexIndex + 0, 0);
  }

  gSPVertex(gfx++, f_CpuBarVertices, 4, 0);
  startX = BAR_START_X;
  taskTime = cpuEndTime - cpuStartTime;
  barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
  cpuBarLength = barLength;
  if (f_CpuBarAverage == 0) {
    f_CpuBarAverage = barLength;
  } else {
    f_CpuBarAverage = (f_CpuBarAverage + barLength) >> 1;
  }
  if (f_CpuBarMaximum < barLength) {
    f_CpuBarMaximum = barLength;
  }
  f_CpuBarVertices[0].v.ob[0] = startX;
  f_CpuBarVertices[1].v.ob[0] = startX + barLength;
  f_CpuBarVertices[2].v.ob[0] = startX + barLength;
  f_CpuBarVertices[3].v.ob[0] = startX;
  gSP2Triangles(gfx++, 0, 2, 1, 0, 0, 3, 2, 0);

  gSPVertex(gfx++, f_AudioBarVertices, 16, 0);
  startX = BAR_START_X;
  for (i = 0; i < nuDebTaskPerfPtr->auTaskCnt; i++) {
    vertexIndex = i * 4;
    taskTime = nuDebTaskPerfPtr->auTaskTime[i].rspEnd -
               nuDebTaskPerfPtr->auTaskTime[i].rspStart;
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    audioBarLength = barLength;
    if (f_AudioBarAverage == 0) {
      f_AudioBarAverage = barLength;
    } else {
      f_AudioBarAverage = (f_AudioBarAverage + barLength) >> 1;
    }
    if (f_AudioBarMaximum < barLength) {
      f_AudioBarMaximum = barLength;
    }
    f_AudioBarVertices[vertexIndex + 0].v.ob[0] = startX;
    f_AudioBarVertices[vertexIndex + 1].v.ob[0] = startX + barLength;
    f_AudioBarVertices[vertexIndex + 2].v.ob[0] = startX + barLength;
    f_AudioBarVertices[vertexIndex + 3].v.ob[0] = startX;
    gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0,
                  vertexIndex + 0, vertexIndex + 3, vertexIndex + 2, 0);
    startX += barLength;
  }

  gSPVertex(gfx++, f_RspBarVertices, 32, 0);
  startX = BAR_START_X;
  for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
    vertexIndex = i * 4;
    taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].rspEnd -
               nuDebTaskPerfPtr->gfxTaskTime[i].rspStart;
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    rspBarLength = barLength;
    if (f_RspBarAverage == 0) {
      f_RspBarAverage = barLength;
    } else {
      f_RspBarAverage = (f_RspBarAverage + barLength) >> 1;
    }
    if (f_RspBarMaximum < barLength) {
      f_RspBarMaximum = barLength;
    }
    f_RspBarVertices[vertexIndex + 0].v.ob[0] = startX;
    f_RspBarVertices[vertexIndex + 1].v.ob[0] = startX + barLength;
    f_RspBarVertices[vertexIndex + 2].v.ob[0] = startX + barLength;
    f_RspBarVertices[vertexIndex + 3].v.ob[0] = startX;
    gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0,
                  vertexIndex + 0, vertexIndex + 3, vertexIndex + 2, 0);
    startX += barLength;
  }

  gSPVertex(gfx++, f_RdpBarVertices, 32, 0);
  startX = BAR_START_X;
  for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
    vertexIndex = i * 4;
    taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].rdpEnd -
               nuDebTaskPerfPtr->gfxTaskTime[i].rspStart;
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    rdpBarLength = barLength;
    if (f_RdpBarAverage == 0) {
      f_RdpBarAverage = barLength;
    } else {
      f_RdpBarAverage = (f_RdpBarAverage + barLength) >> 1;
    }
    if (f_RdpBarMaximum < barLength) {
      f_RdpBarMaximum = barLength;
    }
    f_RdpBarVertices[vertexIndex + 0].v.ob[0] = startX;
    f_RdpBarVertices[vertexIndex + 1].v.ob[0] = barLength;
    f_RdpBarVertices[vertexIndex + 2].v.ob[0] = barLength;
    f_RdpBarVertices[vertexIndex + 3].v.ob[0] = startX;
    gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0,
                  vertexIndex + 0, vertexIndex + 3, vertexIndex + 2, 0);
    startX += barLength;
  }

  gSPVertex(gfx++, f_RdpCommandBarVertices, 32, 0);
  startX = BAR_START_X;
  for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
    vertexIndex = i * 4;
    taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_CMD_CTR];
    taskTime = RDP_COUNT_TO_MICROSECONDS(taskTime);
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    rdpCommandBarLength = barLength;
    if (f_RdpCommandBarAverage == 0) {
      f_RdpCommandBarAverage = barLength;
    } else {
      f_RdpCommandBarAverage = (f_RdpCommandBarAverage + barLength) >> 1;
    }
    if (f_RdpCommandBarMaximum < barLength) {
      f_RdpCommandBarMaximum = barLength;
    }
    f_RdpCommandBarVertices[vertexIndex + 0].v.ob[0] = startX;
    f_RdpCommandBarVertices[vertexIndex + 1].v.ob[0] = barLength;
    f_RdpCommandBarVertices[vertexIndex + 2].v.ob[0] = barLength;
    f_RdpCommandBarVertices[vertexIndex + 3].v.ob[0] = startX;
    gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0,
                  vertexIndex + 0, vertexIndex + 3, vertexIndex + 2, 0);
    startX += barLength;
  }

  gSPVertex(gfx++, f_RdpPipeBarVertices, 32, 0);
  startX = BAR_START_X;
  for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
    vertexIndex = i * 4;
    taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_PIPE_CTR];
    taskTime = RDP_COUNT_TO_MICROSECONDS(taskTime);
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    rdpPipeBarLength = barLength;
    if (f_RdpPipeBarAverage == 0) {
      f_RdpPipeBarAverage = barLength;
    } else {
      f_RdpPipeBarAverage = (f_RdpPipeBarAverage + barLength) >> 1;
    }
    if (f_RdpPipeBarMaximum < barLength) {
      f_RdpPipeBarMaximum = barLength;
    }
    f_RdpPipeBarVertices[vertexIndex + 0].v.ob[0] = startX;
    f_RdpPipeBarVertices[vertexIndex + 1].v.ob[0] = barLength;
    f_RdpPipeBarVertices[vertexIndex + 2].v.ob[0] = barLength;
    f_RdpPipeBarVertices[vertexIndex + 3].v.ob[0] = startX;
    gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0,
                  vertexIndex + 0, vertexIndex + 3, vertexIndex + 2, 0);
    startX += barLength;
  }

  gSPVertex(gfx++, f_RdpTmemBarVertices, 32, 0);
  startX = BAR_START_X;
  for (i = 0; i < nuDebTaskPerfPtr->gfxTaskCnt; i++) {
    vertexIndex = i * 4;
    taskTime = nuDebTaskPerfPtr->gfxTaskTime[i].dpCnt[NU_DEB_DP_TMEM_CTR];
    taskTime = RDP_COUNT_TO_MICROSECONDS(taskTime);
    barLength = taskTime * FRAME_WIDTH / FRAME_MICROSECONDS;
    rdpTmemBarLength = barLength;
    if (f_RdpTmemBarAverage == 0) {
      f_RdpTmemBarAverage = barLength;
    } else {
      f_RdpTmemBarAverage = (f_RdpTmemBarAverage + barLength) >> 1;
    }
    if (f_RdpTmemBarMaximum < barLength) {
      f_RdpTmemBarMaximum = barLength;
    }
    f_RdpTmemBarVertices[vertexIndex + 0].v.ob[0] = startX;
    f_RdpTmemBarVertices[vertexIndex + 1].v.ob[0] = barLength;
    f_RdpTmemBarVertices[vertexIndex + 2].v.ob[0] = barLength;
    f_RdpTmemBarVertices[vertexIndex + 3].v.ob[0] = startX;
    gSP2Triangles(gfx++, vertexIndex + 0, vertexIndex + 2, vertexIndex + 1, 0,
                  vertexIndex + 0, vertexIndex + 3, vertexIndex + 2, 0);
    startX += barLength;
  }

  if (readoutMode & METER_READOUT_FREE_MEMORY) {
    sprintf(f_ReadoutText, "%06d", GetFreeMemorySize());
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
  } else if (readoutMode & METER_READOUT_CURRENT) {
    sprintf(f_ReadoutText, "%05d", cpuBarLength);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
  } else if (readoutMode & METER_READOUT_MAXIMUM) {
    sprintf(f_ReadoutText, "%05d", f_CpuBarMaximum);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
  } else if (readoutMode & METER_READOUT_AVERAGE) {
    sprintf(f_ReadoutText, "%05d", f_CpuBarAverage);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y, f_ReadoutText);
  }

  if (readoutMode & METER_READOUT_CURRENT) {
    sprintf(f_ReadoutText, "%05d", rspBarLength);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT,
               f_ReadoutText);
  } else if (readoutMode & METER_READOUT_MAXIMUM) {
    sprintf(f_ReadoutText, "%05d", f_RspBarMaximum);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT,
               f_ReadoutText);
  } else if (readoutMode & METER_READOUT_AVERAGE) {
    sprintf(f_ReadoutText, "%05d", f_RspBarAverage);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT,
               f_ReadoutText);
  }

  if (readoutMode & METER_READOUT_CURRENT) {
    sprintf(f_ReadoutText, "%05d", rdpBarLength);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT * 2,
               f_ReadoutText);
  } else if (readoutMode & METER_READOUT_MAXIMUM) {
    sprintf(f_ReadoutText, "%05d", f_RdpBarMaximum);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT * 2,
               f_ReadoutText);
  } else if (readoutMode & METER_READOUT_AVERAGE) {
    sprintf(f_ReadoutText, "%05d", f_RdpBarAverage);
    DrawDigits(&gfx, READOUT_X, y + READOUT_TOP_Y + READOUT_LINE_HEIGHT * 2,
               f_ReadoutText);
  }

  gDPFullSync(gfx++);
  gSPEndDisplayList(gfx++);
  nuGfxTaskStart(
      f_MeterDisplayLists[f_MeterDisplayListIndex],
      (s32)(gfx - f_MeterDisplayLists[f_MeterDisplayListIndex]) * sizeof(Gfx),
      NU_GFX_UCODE_F3DEX, taskFlags);
  f_MeterDisplayListIndex++;
  f_MeterDisplayListIndex &= 1;
}

/*
 * Prints the framebuffer count, the free memory and the largest free block,
 * and starts the display list as its own graphics task.
 */
void DrawMemoryMeter(u32 y, s32 taskFlags) {
  Gfx* gfx = f_MeterDisplayLists[f_MeterDisplayListIndex];

  gSPDisplayList(gfx++, OS_K0_TO_PHYSICAL(f_InitRcpDisplayList));
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                   osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
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
  gSPTextureRectangle(gfx++, 250 << 2, (y - 8) << 2, 302 << 2, (y + 20) << 2,
                      G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  sprintf(f_ReadoutText, "% 8d", nuGfxCfbNum);
  DrawDigits(&gfx, 252, y - 7, f_ReadoutText);
  sprintf(f_ReadoutText, "%08d", GetFreeMemorySize());
  DrawDigits(&gfx, 252, y + 2, f_ReadoutText);
  sprintf(f_ReadoutText, "%08d", FindLargestFreeMemorySize());
  DrawDigits(&gfx, 252, y + 10, f_ReadoutText);
  gDPFullSync(gfx++);
  gSPEndDisplayList(gfx++);
  nuGfxTaskStart(
      f_MeterDisplayLists[f_MeterDisplayListIndex],
      (s32)(gfx - f_MeterDisplayLists[f_MeterDisplayListIndex]) * sizeof(Gfx),
      NU_GFX_UCODE_F3DEX, taskFlags);
  f_MeterDisplayListIndex = (f_MeterDisplayListIndex + 1) & 1;
}
