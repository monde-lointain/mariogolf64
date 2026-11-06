/*
 * Loads the cursor box's image and draws it around a point.
 */
#include "cursor_box.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "compiler.h"
#include "game.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "include_asm.h"
#include "math_util.h"
#include "screen.h"

#define CURSOR_BOX_CORNER_COUNT 4
/* Half the size of the box the cursor frames, whose top-left corner the callers
 * give */
#define CURSOR_BOX_HALF_SIZE 24

static void* f_CursorBoxImage = NULL;
static Vtx f_CursorBoxVertices[] = {
    {{{-24, 24, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-24, -24, 0}, 0, {0, 0xC00}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{24, -24, 0}, 0, {0xC00, 0xC00}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{24, 24, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
static f32 f_CursorCornerPhases[] = {0.0f, 1.5707964f, 3.1415927f, 4.712389f};

/* Read by no routine */
static u8 f_UnusedBufferAfterCornerPhases[0x40] UNUSED;
static Mtx f_CursorBoxProjection;
static Mtx f_CursorBoxTranslations[DRAW_BUFFER_COUNT];

/* Reads the cursor box image into the given buffer and sizes the quad to it */
void LoadCursorBox(void* image) {
  EncodedAsset asset;

  /*
   * s32 sizes are there to match: u16 sizes give 12 words differing (none in
   * shape), u32 48 (4) and s16 68 (14). Reading the sizes as the header's
   * halfwords, rather than a byte at a time, gives 10 (10).
   */
  s32 width;
  s32 height;

  f_CursorBoxImage = image;
  OpenEncodedAsset(ASSET_CURSOR_BOX, &asset);
  ReadEncodedAsset(f_CursorBoxImage, &asset);
  width = ReadImageWidth(f_CursorBoxImage);
  height = ReadImageHeight(f_CursorBoxImage);
  f_CursorBoxVertices[0].v.tc[TEXTURE_AXIS_S] = 0;
  f_CursorBoxVertices[1].v.tc[TEXTURE_AXIS_S] = 0;
  f_CursorBoxVertices[0].v.tc[TEXTURE_AXIS_T] = 0;
  f_CursorBoxVertices[3].v.tc[TEXTURE_AXIS_T] = 0;
  f_CursorBoxVertices[0].v.ob[AXIS_X] = -width / 2;
  f_CursorBoxVertices[1].v.ob[AXIS_X] = -width / 2;
  f_CursorBoxVertices[2].v.ob[AXIS_X] = width / 2;
  f_CursorBoxVertices[3].v.ob[AXIS_X] = width / 2;
  f_CursorBoxVertices[0].v.ob[AXIS_Y] = height / 2;
  f_CursorBoxVertices[1].v.ob[AXIS_Y] = -height / 2;
  f_CursorBoxVertices[2].v.ob[AXIS_Y] = -height / 2;
  f_CursorBoxVertices[3].v.ob[AXIS_Y] = height / 2;
  f_CursorBoxVertices[2].v.tc[TEXTURE_AXIS_S] = HalfScaleTexelCoordinate(width);
  f_CursorBoxVertices[3].v.tc[TEXTURE_AXIS_S] = HalfScaleTexelCoordinate(width);
  f_CursorBoxVertices[1].v.tc[TEXTURE_AXIS_T] =
      HalfScaleTexelCoordinate(height);
  f_CursorBoxVertices[2].v.tc[TEXTURE_AXIS_T] =
      HalfScaleTexelCoordinate(height);
}

/*
 * Draws the cursor box with its top-left corner at the given screen position.
 * Each corner's red pulses with the sine of its own phase, which the four
 * corners start a quarter turn apart. Doesn't match: the original keeps the
 * green and blue in the loop rather than hoisting them, which leaves left in a
 * saved register and the frame 8 bytes smaller. It also keeps corner as a
 * counter rather than turning it into an offset. A u32 corner ties; an s16 or
 * u8 corner, a < test or dropping the barrier give 72-176, and storing the
 * alpha first 44. The original sets the green before the red's clamp and stores
 * it after it. loop.c keeps a variable's set in the loop when the set may not
 * run in every pass, past a label or jump, and is used in another block. Green
 * and blue variables are hoisted wherever they are set (42 words, 10 in shape):
 * set before the clamp, loop.c's dump treats them as always set, and set after
 * it they are used in their own block. A static inline function stepping the
 * phase, in place of the block, gives 173 words (11 in shape), as with no
 * barrier. So does a pointer to the corner's phase, set inside or outside the
 * block or with the step outside it. A pointer to the projection matrix changes
 * nothing. Naming the translation's y as boxY saves 2 words and 2 in shape, and
 * naming x too gains nothing more. The register oracle, forcing 6 pseudos'
 * registers, leaves 12 words at best, so the code differs too. 42 words differ,
 * 10 in shape.
 */
#ifdef NON_MATCHING
void DrawCursorBox(Gfx** displayListHead, s32 left, s32 top) {
  Gfx* displayList = *displayListHead;
  s32 corner;
  s32 red;
  s32 width;
  s32 height;
  s32 boxY;

  width = ReadImageWidth(f_CursorBoxImage);
  height = ReadImageHeight(f_CursorBoxImage);
  for (corner = 0; corner != CURSOR_BOX_CORNER_COUNT; corner++) {
    /* In its own block, to match; without it 173 words differ, 11 in shape,
     * against 42 (10) */
    IN_OWN_BLOCK(f_CursorCornerPhases[corner] += DegreesToRadians(15));
    red = (s32)(sinf(f_CursorCornerPhases[corner]) * 127.0f) + 127;
    if (red > COLOR_CHANNEL_MAX) {
      red = COLOR_CHANNEL_MAX;
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
  boxY = (SCREEN_HEIGHT / 2) - CURSOR_BOX_HALF_SIZE - top;
  guTranslate(&f_CursorBoxTranslations[g_DrawBufferIndex],
      (f32)(left + CURSOR_BOX_HALF_SIZE - (SCREEN_WIDTH / 2)) - 0.5f,
      (f32)boxY + 0.5f, -100.0f);
  gDPPipeSync(displayList++);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_NONE);
  gDPLoadTextureBlock_4b(displayList++,
      TextureAddress(ImageBody(f_CursorBoxImage)), G_IM_FMT_IA, width, height,
      0, 0, 0, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
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
  gSPClearGeometryMode(displayList++, ALL_GEOMETRY_MODES);
  gSPSetGeometryMode(displayList++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(displayList++);
  gSPMatrix(displayList++, PhysicalAddress(&f_CursorBoxProjection),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(displayList++,
      PhysicalAddress(&f_CursorBoxTranslations[g_DrawBufferIndex]),
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
