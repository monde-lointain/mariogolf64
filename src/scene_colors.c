/*
 * Derives the shaded quads' colors from the scene's colors, and draws the
 * plane of two blended texture layers.
 */
#include "common.h"

#include "scene_colors.h"

#include "functions.h"
#include "game.h"
#include "hole_display.h"
#include "save.h"
#include "variables.h"

/* Each half of f_ShadedVertices is two quads; the second takes the derived
 * color */
#define QUAD_VERTEX_COUNT 4
#define SHADED_QUAD_START QUAD_VERTEX_COUNT

#define RGB_CHANNEL_COUNT 3

#define PLANE_TILE_SIZE 16

static Vtx f_TexturePlaneVertices[6] = {
    {{{0, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{4096, 0, 0}, 0, {0x3FFF, 0}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{0, 0, 4096}, 0, {0, 0x3FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{4096, 0, 4096}, 0, {0x3FFF, 0x3FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{0, 0, 8192}, 0, {0, 0x7FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{4096, 0, 8192}, 0, {0x3FFF, 0x7FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}}};
static Vtx f_ShadedVertices[2][2 * QUAD_VERTEX_COUNT] = {
    {{{{0, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{4096, 0, 0}, 0, {0x1000, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{0, 0, 8192}, 0, {0, 0x2000}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{4096, 0, 8192}, 0, {0x1000, 0x2000}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{0, 0, 0}, 0, {-0x2000, -0x4000}, {0xFF, 0xFF, 0xEF, 0x7F}}},
        {{{4096, 0, 0}, 0, {0x1FFF, -0x4000}, {0xFF, 0xFF, 0xEF, 0x7F}}},
        {{{0, 0, 8192}, 0, {-0x2000, 0x3FFF}, {0xFF, 0xFF, 0xEF, 0x7F}}},
        {{{4096, 0, 8192}, 0, {0x1FFF, 0x3FFF}, {0xFF, 0xFF, 0xEF, 0x7F}}}},
    {{{{0, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{4096, 0, 0}, 0, {0x1000, 0}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{0, 0, 8192}, 0, {0, 0x2000}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{4096, 0, 8192}, 0, {0x1000, 0x2000}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{0, 0, 0}, 0, {-0x2000, -0x4000}, {0x3F, 0x20, 0xEF, 0x00}}},
        {{{4096, 0, 0}, 0, {0x1FFF, -0x4000}, {0x3F, 0x20, 0xEF, 0x00}}},
        {{{0, 0, 8192}, 0, {-0x2000, 0x3FFF}, {0x3F, 0x20, 0xEF, 0x00}}},
        {{{4096, 0, 8192}, 0, {0x1FFF, 0x3FFF}, {0x3F, 0x20, 0xEF, 0x00}}}}};
s32 g_UnreadDebugSwitch = FALSE;
static s32 f_PlaneTextureScroll = 0;
/* Read by no routine */
static s32 D_800C42C8[2] UNUSED = {0};
static s32 D_800E1C10;
static s32 D_800E1C14;
static u8 D_800E1C18;
static u8 D_800E1C19;
static u8 D_800E1C1A;
static u8 D_800E1C1B;
static u8 D_800E1C1C;
static u8 D_800E1C1D;

/*
 * Sets the shaded quads of f_ShadedVertices and two color globals from
 * the colors of the g_SkyColors entry that g_HoleSetup.conditions selects
 */
void DeriveSceneColors(void) {
  SkyColors* entry;
  s32 vertexOffset;
  s32 channelIndex;

  D_800E1C10 = 0;
  D_800E1C14 = 0;

  /* This steps through the table a course of colors at a time, unlike
   * BlendSkyColors */
  entry = g_SkyColors[(g_HoleSetup.conditions.isRaining * 3) +
      g_HoleSetup.conditions.sceneColorIndex];
  for (vertexOffset = 0; vertexOffset != QUAD_VERTEX_COUNT; vertexOffset++) {
    for (channelIndex = 0; channelIndex != RGB_CHANNEL_COUNT; channelIndex++) {
      f_ShadedVertices[0][SHADED_QUAD_START + vertexOffset].v.cn[channelIndex] =
          (s32)((entry->unk4[channelIndex] + entry->unk7[channelIndex]) * 0.3f);
      f_ShadedVertices[1][SHADED_QUAD_START + vertexOffset].v.cn[channelIndex] =
          (s32)((entry->unk4[channelIndex] + entry->unk7[channelIndex]) * 0.3f);
    }
  }

  D_800E1C1B = (s32)((entry->unk4[0] + entry->unk7[0]) * 0.35f);
  D_800E1C1C = (s32)((entry->unk4[1] + entry->unk7[1]) * 0.35f);
  D_800E1C1D = (s32)((entry->unk4[2] + entry->unk7[2]) * 0.35f);
  D_800E1C18 = entry->unkA[0] * 0.8;
  D_800E1C19 = entry->unkA[1] * 0.8;
  D_800E1C1A = entry->unkA[2] * 0.8;
}

void DoNothing(void) {}

/*
 * Draws the flat strip of f_TexturePlaneVertices with the two blended
 * texture layers, moving the second layer on by a quarter texel each call
 */
void DrawTexturePlane(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_2CYCLE);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
  gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0,
      PRIMITIVE, PRIMITIVE, 0, COMBINED, 0, 0, 0, 0, COMBINED);
  gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xBF);
  gDPSetEnvColor(gfx++, 0x3F, 0x7F, 0x9F, 0xFF);
  SetSceneFog(&gfx);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
  gDPLoadTextureBlock(gfx++, TextureAddress(g_BlendedTextureScrollingLayer + 8),
      G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE,
      0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPLoadMultiBlock(gfx++, TextureAddress(g_BlendedTextureStillLayer + 8),
      SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE,
      DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
  gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
  gDPSetTileSize(
      gfx++, G_TX_RENDERTILE, 0, 0, PLANE_TILE_SIZE << 2, PLANE_TILE_SIZE << 2);
  f_PlaneTextureScroll++;
  gDPSetTileSize(gfx++, 1, f_PlaneTextureScroll, 0,
      f_PlaneTextureScroll + (PLANE_TILE_SIZE << 2), PLANE_TILE_SIZE << 2);
  gDPPipeSync(gfx++);
  gSPVertex(
      gfx++, f_TexturePlaneVertices, ARRAY_COUNT(f_TexturePlaneVertices), 0);
  gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
  gSP2Triangles(gfx++, 3, 2, 4, 0, 3, 4, 5, 0);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
