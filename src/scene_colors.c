#include "scene_colors.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "c_language.h"
#include "compiler.h"
#include "game.h"
#include "gbi_helpers.h"
#include "hole_setup.h"
#include "play.h"
#include "sky_and_hud_animations.h"

#define SHADED_QUAD_START QUAD_VERTEX_COUNT

#define PLANE_TILE_SIZE 16

static Vtx f_TexturePlaneVertices[6] = {
    {{{0, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{4096, 0, 0}, 0, {0x3FFF, 0}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{0, 0, 4096}, 0, {0, 0x3FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{4096, 0, 4096}, 0, {0x3FFF, 0x3FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{0, 0, 8192}, 0, {0, 0x7FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}},
    {{{4096, 0, 8192}, 0, {0x3FFF, 0x7FFF}, {0xFF, 0xFF, 0xFF, 0xBF}}},
};
static Vtx f_ShadedVertices[2][2 * QUAD_VERTEX_COUNT] = {
    {
        {{{0, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{4096, 0, 0}, 0, {0x1000, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{0, 0, 8192}, 0, {0, 0x2000}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{4096, 0, 8192}, 0, {0x1000, 0x2000}, {0xFF, 0xFF, 0xFF, 0x7F}}},
        {{{0, 0, 0}, 0, {-0x2000, -0x4000}, {0xFF, 0xFF, 0xEF, 0x7F}}},
        {{{4096, 0, 0}, 0, {0x1FFF, -0x4000}, {0xFF, 0xFF, 0xEF, 0x7F}}},
        {{{0, 0, 8192}, 0, {-0x2000, 0x3FFF}, {0xFF, 0xFF, 0xEF, 0x7F}}},
        {{{4096, 0, 8192}, 0, {0x1FFF, 0x3FFF}, {0xFF, 0xFF, 0xEF, 0x7F}}},
    },
    {
        {{{0, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{4096, 0, 0}, 0, {0x1000, 0}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{0, 0, 8192}, 0, {0, 0x2000}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{4096, 0, 8192}, 0, {0x1000, 0x2000}, {0x00, 0x00, 0xFF, 0x00}}},
        {{{0, 0, 0}, 0, {-0x2000, -0x4000}, {0x3F, 0x20, 0xEF, 0x00}}},
        {{{4096, 0, 0}, 0, {0x1FFF, -0x4000}, {0x3F, 0x20, 0xEF, 0x00}}},
        {{{0, 0, 8192}, 0, {-0x2000, 0x3FFF}, {0x3F, 0x20, 0xEF, 0x00}}},
        {{{4096, 0, 8192}, 0, {0x1FFF, 0x3FFF}, {0x3F, 0x20, 0xEF, 0x00}}},
    },
};
Boolean g_UnreadDebugSwitch = FALSE;
static s32 f_PlaneTextureScroll = 0;
static s32 f_UnusedWordsAfterPlaneScroll[2] UNUSED = {0};
static s32 f_UnusedFirstClearedWord;
static s32 f_UnusedSecondClearedWord;
static u8 f_UnusedDimmedRed;
static u8 f_UnusedDimmedGreen;
static u8 f_UnusedDimmedBlue;
static u8 f_UnusedBlendedRed;
static u8 f_UnusedBlendedGreen;
static u8 f_UnusedBlendedBlue;

void DeriveSceneColors(void) {
    SkyColors *colors;
    s32 vertexOffset;
    s32 channelIndex;

    f_UnusedFirstClearedWord = 0;
    f_UnusedSecondClearedWord = 0;

    colors = g_SkyColors
        [(g_HoleSetup.conditions.isRaining * 3) + g_HoleSetup.conditions.sceneColorIndex];
    for (vertexOffset = 0; vertexOffset != QUAD_VERTEX_COUNT; vertexOffset++) {
        for (channelIndex = 0; channelIndex != RGB_CHANNEL_COUNT; channelIndex++) {
            f_ShadedVertices[0][SHADED_QUAD_START + vertexOffset].v.cn[channelIndex] =
                (s32)((colors->topColor[channelIndex] + colors->horizonColor[channelIndex]) * 0.3f);
            f_ShadedVertices[1][SHADED_QUAD_START + vertexOffset].v.cn[channelIndex] =
                (s32)((colors->topColor[channelIndex] + colors->horizonColor[channelIndex]) * 0.3f);
        }
    }

    f_UnusedBlendedRed =
        (s32)((colors->topColor[RED_CHANNEL] + colors->horizonColor[RED_CHANNEL]) * 0.35f);
    f_UnusedBlendedGreen =
        (s32)((colors->topColor[GREEN_CHANNEL] + colors->horizonColor[GREEN_CHANNEL]) * 0.35f);
    f_UnusedBlendedBlue =
        (s32)((colors->topColor[BLUE_CHANNEL] + colors->horizonColor[BLUE_CHANNEL]) * 0.35f);
    f_UnusedDimmedRed = colors->noiseColor[RED_CHANNEL] * 0.8;
    f_UnusedDimmedGreen = colors->noiseColor[GREEN_CHANNEL] * 0.8;
    f_UnusedDimmedBlue = colors->noiseColor[BLUE_CHANNEL] * 0.8;
}

void DoNothing(void) {
}

void DrawTexturePlane(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

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
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        TEXEL1,
        PRIMITIVE_ALPHA,
        TEXEL1,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        COMBINED,
        0,
        0,
        0,
        0,
        COMBINED
    );
    gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xBF);
    gDPSetEnvColor(gfx++, 0x3F, 0x7F, 0x9F, OPAQUE_ALPHA);
    SetSceneFog(&gfx);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureScrollingLayer)),
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureStillLayer)),
        SECOND_TEXTURE_TMEM,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPTexture(gfx++, TEXTURE_SCALE_FULL, TEXTURE_SCALE_FULL, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        0,
        PLANE_TILE_SIZE << G_TEXTURE_IMAGE_FRAC,
        PLANE_TILE_SIZE << G_TEXTURE_IMAGE_FRAC
    );
    f_PlaneTextureScroll++;
    gDPSetTileSize(
        gfx++,
        1,
        f_PlaneTextureScroll,
        0,
        f_PlaneTextureScroll + (PLANE_TILE_SIZE << G_TEXTURE_IMAGE_FRAC),
        PLANE_TILE_SIZE << G_TEXTURE_IMAGE_FRAC
    );
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_TexturePlaneVertices, ARRAY_COUNT(f_TexturePlaneVertices), 0);
    gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
    gSP2Triangles(gfx++, 3, 2, 4, 0, 3, 4, 5, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
