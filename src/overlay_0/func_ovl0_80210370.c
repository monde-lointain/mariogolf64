#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"
#include "overlay_0.h"

extern s32 D_ovl0_80224114;
extern s32 D_ovl0_80224120;
extern s32 D_ovl0_80224124;
extern void func_800772B0(void);
extern s32 D_ovl0_80224848;
extern s32 D_ovl0_8022484C;
extern s32 D_ovl0_80224408;
extern s32 D_ovl0_80224410;
extern s32 D_ovl0_80224414;

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80210370);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_802104BC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80210F40);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_8021169C);

extern void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan);
extern Mtx D_1B5638[];
extern Mtx D_227D20[];
extern s32 D_800C0E60;
extern Gfx D_ovl0_80224490[];
extern Vtx D_ovl0_80224538[];
extern f64 D_ovl0_80225388;
extern f64 D_ovl0_80225390;
extern f64 D_ovl0_80225398;
extern f64 D_ovl0_802253A0;
extern Mtx D_ovl0_80227D20[];

void func_ovl0_80211D60(Gfx **gfxp) {
    f32 rot[3];
    f32 trans[3];
    f32 scale[3];
    f32 mf[4][4];
    Gfx *gfx = *gfxp;
    s32 alpha = 255;
    s32 i;
    s32 n;
    s32 h;

    D_ovl0_80224414 ^= 1;
    if (D_ovl0_80224408 == 1) {
        func_80050DA0(0x49, D_800C0E60, 0x3F, 0x1E, 0x7F);
    }
    if (D_ovl0_80224408 == 0x1E) {
        func_80050DA0(0x4A, D_800C0E60, 0x3F, 0x1E, 0x7F);
    }
    scale[0] = 1.0f;
    scale[1] = 1.0f;
    scale[2] = 1.0f;
    if (D_ovl0_80224408 < 6) {
        rot[2] = 0.0f;
        rot[0] = 3.1415927f;
        rot[1] = D_ovl0_80225390 - (f32)D_ovl0_80224408 * D_ovl0_80225388;
    } else if (D_ovl0_80224408 < 0x12) {
        rot[1] = 0.0f;
        rot[2] = 0.0f;
        rot[0] = D_ovl0_802253A0 - (f32)(D_ovl0_80224408 - 6) * D_ovl0_80225398;
    } else if (D_ovl0_80224408 < 0x18) {
        rot[1] = 0.0f;
        rot[2] = 0.0f;
        rot[0] = D_ovl0_802253A0 - (f32)(D_ovl0_80224408 - 6) * D_ovl0_80225398;
    } else if (D_ovl0_80224408 < 0x1E) {
        rot[1] = 0.0f;
        rot[2] = 0.0f;
        rot[0] = D_ovl0_802253A0 - (f32)(D_ovl0_80224408 - 0x12) * D_ovl0_80225398;
    } else {
        if (D_ovl0_80224408 >= 0x2A) {
            D_ovl0_80224410 = -1;
            return;
        }
        n = D_ovl0_80224408 - 0x1E;
        alpha -= n * 32;
        if (alpha < 0) {
            alpha = 0;
        }
        rot[0] = rot[1] = rot[2] = 0.0f;
        scale[0] = scale[1] = scale[2] = n * 2.0f + 1.0f;
    }
    trans[0] = 0.0f;
    trans[1] = 0.0f;
    trans[2] = -300.0f;
    mtx_from_rts(mf, rot, trans, scale);
    convert_and_pack_floats_to_fixed(mf, &D_ovl0_80227D20[D_ovl0_80224414]);

    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 255, alpha);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, ENVIRONMENT, 0, TEXEL0, 0, 0, 0, 0, TEXEL0, ENVIRONMENT, 0, TEXEL0, 0);
    gSPDisplayList(gfx++, D_ovl0_80224490);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    if (alpha == 255) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    } else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }
    if (D_ovl0_80224408 < 0x18) {
        gDPLoadTLUT_pal256(gfx++, (u32)&D_ovl0_8022D650->render_a.palette_008 & ~7);
    } else {
        gDPLoadTLUT_pal256(gfx++, (u32)&D_ovl0_8022D650->render_b.palette_2510 & ~7);
    }
    gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &D_227D20[D_ovl0_80224414], G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, D_ovl0_80224538, 12, 0);
    for (i = 0; i != 5; i++) {
        if (D_ovl0_80224408 < 0x18) {
            h = (i != 4) ? 12 : 8;
            gDPLoadTextureTile(gfx++, (u32)&D_ovl0_8022D650->render_a.tiles_208[i] & ~7, G_IM_FMT_CI, G_IM_SIZ_8b, 160, h, 0, 0, 159, h - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_MIRROR, 0, 0, 0, 0);
            gSPModifyVertex(gfx++, i * 2, G_MWO_POINT_ST, 0);
            gSPModifyVertex(gfx++, i * 2 + 1, G_MWO_POINT_ST, 0x13FF0000);
            gSPModifyVertex(gfx++, i * 2 + 2, G_MWO_POINT_ST, h << 5);
            gSPModifyVertex(gfx++, i * 2 + 3, G_MWO_POINT_ST, (h << 5) + 0x13FF0000);
        } else {
            h = (i != 4) ? 14 : 8;
            gDPLoadTextureTile(gfx++, (u32)&D_ovl0_8022D650->render_b.tiles_2710[i] & ~7, G_IM_FMT_CI, G_IM_SIZ_8b, 144, h, 0, 0, 143, h - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_MIRROR, 0, 0, 0, 0);
            gSPModifyVertex(gfx++, i * 2, G_MWO_POINT_ST, 0);
            gSPModifyVertex(gfx++, i * 2 + 1, G_MWO_POINT_ST, 0x11FF0000);
            gSPModifyVertex(gfx++, i * 2 + 2, G_MWO_POINT_ST, h << 5);
            gSPModifyVertex(gfx++, i * 2 + 3, G_MWO_POINT_ST, (h << 5) + 0x11FF0000);
        }
        gSP2Triangles(gfx++, i * 2, i * 2 + 2, i * 2 + 3, 0, i * 2, i * 2 + 3, i * 2 + 1, 0);
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80212590);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_802129C0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80212CE0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80213038);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80213634);

extern s32 D_ovl0_802246F8;

s32 func_ovl0_80213C64(void) { return D_ovl0_802246F8 != -1; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80213C78);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80213D70);

extern u8* D_8012D3C0;

s32 func_ovl0_80213C78(PlayerState* player);

void func_ovl0_8021419C(PlayerState* player) {
  RomAssetLoad asset;

  switch (func_ovl0_80213C78(player)) {
    case 2:
    case 10:
      get_rom_asset_allocation_size(0x6A7, &asset);
      load_rom_asset_payload(D_8012D3C0 + 0x970, &asset);
      get_rom_asset_allocation_size(0x6A8, &asset);
      load_rom_asset_payload(D_8012D3C0 + 0x1578, &asset);
      break;
    case 3:
      get_rom_asset_allocation_size(0x6A6, &asset);
      load_rom_asset_payload(D_8012D3C0 + 0x970, &asset);
      break;
    case 0:
      get_rom_asset_allocation_size(0x6BE, &asset);
      load_rom_asset_payload(D_8012D3C0 + 0x970, &asset);
      break;
    case 1:
      get_rom_asset_allocation_size(0x6AE, &asset);
      load_rom_asset_payload(D_8012D3C0 + 0x970, &asset);
      break;
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_80214250);

void func_ovl0_80216130(s32 arg0) {
  RomAssetLoad asset;

  if (D_ovl0_80224124 == 1) {
    if (arg0 == -1) {
      D_ovl0_80224848 = -1;
      return;
    }
    D_ovl0_80224848 = 0;
    D_ovl0_8022484C = 0x6CF;
    get_rom_asset_allocation_size(0x6CF, &asset);
    load_rom_asset_payload(D_ovl0_8022D650, &asset);
    get_rom_asset_allocation_size(0x6AF, &asset);
    load_rom_asset_payload(D_ovl0_8022D650->setup.asset_808, &asset);
    D_ovl0_80224120 = -1;
    D_ovl0_80224114 = -1;
    func_800772B0();
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80210370", func_ovl0_802161D4);
