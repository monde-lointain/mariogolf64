#include "common.h"

#include "asset.h"
#include "math_util.h"
#include "screen.h"
#include "variables.h"

#define CURSOR_BOX_ASSET 0x6EF
#define CURSOR_BOX_CORNER_COUNT 4
/* Half the size of the box the cursor frames, whose top-left corner the callers
 * give */
#define CURSOR_BOX_HALF_SIZE 24

extern Mtx f_CursorBoxProjection;
extern Mtx f_CursorBoxTranslations[2]; /* One per draw buffer */

void* f_CursorBoxImage = NULL;
Vtx f_CursorBoxVertices[] = {
    {{{-24, 24, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-24, -24, 0}, 0, {0, 0xC00}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{24, -24, 0}, 0, {0xC00, 0xC00}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{24, 24, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
f32 f_CursorCornerPhases[] = {0.0f, 1.5707964f, 3.1415927f, 4.712389f};

/* Reads the cursor box image into the given buffer and sizes the quad to it */
void LoadCursorBox(void* image) {
  EncodedAsset asset;
  /* s32 sizes are there to match */
  s32 width;
  s32 height;

  f_CursorBoxImage = image;
  OpenEncodedAsset(CURSOR_BOX_ASSET, &asset);
  ReadEncodedAsset(f_CursorBoxImage, &asset);
  width = (((u8*)f_CursorBoxImage)[2] << 8) | ((u8*)f_CursorBoxImage)[3];
  height = (((u8*)f_CursorBoxImage)[4] << 8) | ((u8*)f_CursorBoxImage)[5];
  f_CursorBoxVertices[0].v.tc[0] = 0;
  f_CursorBoxVertices[1].v.tc[0] = 0;
  f_CursorBoxVertices[0].v.tc[1] = 0;
  f_CursorBoxVertices[3].v.tc[1] = 0;
  f_CursorBoxVertices[0].v.ob[0] = -width / 2;
  f_CursorBoxVertices[1].v.ob[0] = -width / 2;
  f_CursorBoxVertices[2].v.ob[0] = width / 2;
  f_CursorBoxVertices[3].v.ob[0] = width / 2;
  f_CursorBoxVertices[0].v.ob[1] = height / 2;
  f_CursorBoxVertices[1].v.ob[1] = -height / 2;
  f_CursorBoxVertices[2].v.ob[1] = -height / 2;
  f_CursorBoxVertices[3].v.ob[1] = height / 2;
  f_CursorBoxVertices[2].v.tc[0] = width << 6;
  f_CursorBoxVertices[3].v.tc[0] = width << 6;
  f_CursorBoxVertices[1].v.tc[1] = height << 6;
  f_CursorBoxVertices[2].v.tc[1] = height << 6;
}

/*
 * Draws the cursor box with its top-left corner at the given screen position.
 * Each corner's red pulses with the sine of its own phase, which the four
 * corners start a quarter turn apart. It does not match: the original keeps the
 * green and blue in the loop rather than hoisting them, which leaves left in a
 * saved register and the frame 8 bytes smaller.
 */
#ifdef NON_MATCHING
void DrawCursorBox(Gfx** displayListHead, s32 left, s32 top) {
  Gfx* displayList = *displayListHead;
  s32 corner;
  s32 red;
  s32 width;
  s32 height;

  width = (((u8*)f_CursorBoxImage)[2] << 8) | ((u8*)f_CursorBoxImage)[3];
  height = (((u8*)f_CursorBoxImage)[4] << 8) | ((u8*)f_CursorBoxImage)[5];
  for (corner = 0; corner != CURSOR_BOX_CORNER_COUNT; corner++) {
    f_CursorCornerPhases[corner] += DEGREES_TO_RADIANS(15);
    red = (s32)(sinf(f_CursorCornerPhases[corner]) * 127.0f) + 127;
    if (red > 255) {
      red = 255;
    }
    if (red < 0) {
      red = 0;
    }
    f_CursorBoxVertices[corner].v.cn[0] = red;
    f_CursorBoxVertices[corner].v.cn[1] = 255;
    f_CursorBoxVertices[corner].v.cn[2] = 32;
    f_CursorBoxVertices[corner].v.cn[3] = 191;
  }
  guOrtho(&f_CursorBoxProjection, -SCREEN_WIDTH / 2, SCREEN_WIDTH / 2,
      -SCREEN_HEIGHT / 2, SCREEN_HEIGHT / 2, 10.0f, 2000.0f, 1.0f);
  guTranslate(&f_CursorBoxTranslations[g_DrawBufferIndex],
      (f32)(left + CURSOR_BOX_HALF_SIZE - (SCREEN_WIDTH / 2)) - 0.5f,
      (f32)((SCREEN_HEIGHT / 2) - CURSOR_BOX_HALF_SIZE - top) + 0.5f, -100.0f);
  gDPPipeSync(displayList++);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_NONE);
  gDPLoadTextureBlock_4b(displayList++,
      ((u32)f_CursorBoxImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_IA, width,
      height, 0, 0, 0, 0, 0, 0, 0);
  gDPPipeSync(displayList++);
  gDPPipeSync(displayList++);
  gDPSetCycleType(displayList++, G_CYC_1CYCLE);
  gDPPipeSync(displayList++);
  gDPPipeSync(displayList++);
  gDPSetTexturePersp(displayList++, G_TP_PERSP);
  gDPPipeSync(displayList++);
  gDPSetTextureFilter(displayList++, G_TF_POINT);
  gDPPipeSync(displayList++);
  gDPSetRenderMode(displayList++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineLERP(displayList++, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0, 0, 0, 0,
      SHADE, SHADE, 0, TEXEL0, 0);
  gSPClearGeometryMode(displayList++, 0xFFFFFFFF);
  gSPSetGeometryMode(displayList++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(displayList++);
  gSPMatrix(displayList++, OS_K0_TO_PHYSICAL(&f_CursorBoxProjection),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(displayList++,
      OS_K0_TO_PHYSICAL(&f_CursorBoxTranslations[g_DrawBufferIndex]),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gDPPipeSync(displayList++);
  gSPVertex(displayList++, f_CursorBoxVertices, CURSOR_BOX_CORNER_COUNT, 0);
  gSP2Triangles(displayList++, 0, 1, 2, 0, 0, 2, 3, 0);
  gDPPipeSync(displayList++);
  *displayListHead = displayList;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_13/cursor_box", DrawCursorBox);
#endif
