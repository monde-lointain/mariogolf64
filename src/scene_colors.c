#include "common.h"
#include "variables.h"

/* Each half of g_ShadedVertices is two quads; the second takes the derived color */
#define QUAD_VERTEX_COUNT 4
#define SHADED_QUAD_START QUAD_VERTEX_COUNT

#define RGB_CHANNEL_COUNT 3

#define PLANE_TILE_SIZE 16

typedef struct Unk100 {
    /* 0x00 */ u8 unk0[4];
    /* 0x04 */ u8 unk4[3];
    /* 0x07 */ u8 unk7[3];
    /* 0x0A */ u8 unkA[3];
    /* 0x0D */ u8 unkD[0x100 - 0xD];
} Unk100; /* size = 0x100 */

extern Vtx g_TexturePlaneVertices[6];
extern Vtx g_ShadedVertices[2][2 * QUAD_VERTEX_COUNT];
extern s32 sPlaneTextureScroll;
extern Unk100 D_800C6014[];
extern s32 D_800E1C10;
extern s32 D_800E1C14;
extern u8 D_800E1C18;
extern u8 D_800E1C19;
extern u8 D_800E1C1A;
extern u8 D_800E1C1B;
extern u8 D_800E1C1C;
extern u8 D_800E1C1D;

void func_800625FC(Gfx **displayListHead);

/*
 * Sets the shaded quads of g_ShadedVertices and two color globals from
 * the colors of the D_800C6014 entry that g_HoleConditions selects
 */
void DeriveSceneColors(void) {
    Unk100 *entry;
    s32 vertexOffset;
    s32 channelIndex;

    D_800E1C10 = 0;
    D_800E1C14 = 0;
    entry = &D_800C6014[g_HoleConditions.isRaining * 3 + g_HoleConditions.sceneColorIndex];
    for (vertexOffset = 0; vertexOffset != QUAD_VERTEX_COUNT; vertexOffset++) {
        for (channelIndex = 0; channelIndex != RGB_CHANNEL_COUNT; channelIndex++) {
            g_ShadedVertices[0][SHADED_QUAD_START + vertexOffset].v.cn[channelIndex] =
                (s32)((entry->unk4[channelIndex] + entry->unk7[channelIndex]) * 0.3f);
            g_ShadedVertices[1][SHADED_QUAD_START + vertexOffset].v.cn[channelIndex] =
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

void DoNothing(void) {
}

/*
 * Draws the flat strip of g_TexturePlaneVertices with the two blended
 * texture layers, moving the second layer on by a quarter texel each call
 */
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
    gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, COMBINED, 0,
                      0, 0, 0, COMBINED);
    gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xBF);
    gDPSetEnvColor(gfx++, 0x3F, 0x7F, 0x9F, 0xFF);
    func_800625FC(&gfx);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gDPLoadTextureBlock(gfx++, (u32)(g_BlendedTextureScrollingLayer + 8) & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                        DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(gfx++, (u32)(g_BlendedTextureStillLayer + 8) & ~7, SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA,
                      G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, PLANE_TILE_SIZE << 2, PLANE_TILE_SIZE << 2);
    sPlaneTextureScroll++;
    gDPSetTileSize(gfx++, 1, sPlaneTextureScroll, 0, sPlaneTextureScroll + (PLANE_TILE_SIZE << 2),
                   PLANE_TILE_SIZE << 2);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, g_TexturePlaneVertices, ARRAY_COUNT(g_TexturePlaneVertices), 0);
    gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
    gSP2Triangles(gfx++, 3, 2, 4, 0, 3, 4, 5, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
