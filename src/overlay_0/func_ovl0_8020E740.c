#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"

extern void* heap3_alloc(u32 size_bytes);
extern void heap3_free(void** payload_ptr);
extern s32 D_ovl0_80224114;
extern s32 D_ovl0_80224120;
extern s32 D_ovl0_80224124;
extern void* D_ovl0_8022D650;

void func_ovl0_8020E740(void) {
  RomAssetLoad asset;

  D_ovl0_80224114 = -1;
  D_ovl0_80224124 = 1;
  D_ovl0_8022D650 = heap3_alloc(0x6270);
  D_ovl0_80224120 = -1;
}

void func_ovl0_8020E78C(void) {
  D_ovl0_80224124 = 0;
  heap3_free(&D_ovl0_8022D650);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020E7B4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020EBBC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020ED54);

s32 func_ovl0_8020EEEC(s32* arg0) { return *arg0 = *arg0; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020EEF8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020FAE8);

extern Mtx D_227CA0[];
extern Mtx D_ovl0_80227CA0[];
extern Vtx D_ovl0_802243C8[];
extern s32 D_ovl0_802243B4;
extern s32 D_ovl0_802243B8;
extern s32 D_ovl0_802243C0;
extern f64 D_ovl0_802252A8;
extern f64 D_ovl0_802252B0;
extern u32 sky_panel_bank_index;
void convert_and_pack_floats_to_fixed(f32 mf[4][4], Mtx* m);
void emit_glyph_sprite_dl(Gfx** dl, s32 tag);
void func_80071608(Gfx** dl);
void func_80071CE4(Gfx** dl, s32 i);
void func_80075E48(Gfx** gfxp);
void mtx_from_rts(f32 matrix[4][4], f32 rotation[3], f32 translation[3], f32 scale[3]);

void func_ovl0_8020FC10(Gfx** glistp) {
    f32 scale[3];
    f32 pos[3];
    f32 rot[3];
    f32 model[4][4];
    f32 view[4][4];
    Gfx* gfx = *glistp;
    f32 temp_ft2;
    f32 var_ft0;
    f32 var_ft1;

    if (D_ovl0_802243B8 != -1) {
        if (D_ovl0_802243B4 == 0) {
            if (D_ovl0_802243B8 < 0xA) {
                D_ovl0_802243B8 = D_ovl0_802243B8 + 1;
            }
        } else {
            if (D_ovl0_802243B8 > 0) {
                D_ovl0_802243B8 = D_ovl0_802243B8 - 1;
            }
        }
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0x9F);
        gDPPipeSync(gfx++);

        temp_ft2 = (f32) D_ovl0_802243B8;
        var_ft1 = (f32) ((f64) ((temp_ft2 * 18.0f) + 180.0f) * D_ovl0_802252A8);
        if (D_ovl0_802252B0 < (f64) var_ft1) {
            var_ft1 = 6.2831855f;
        }
        var_ft0 = temp_ft2 * 0.1f;
        if (var_ft0 > 1.0f) {
            var_ft0 = 1.0f;
        }
        scale[0] = var_ft0;
        scale[1] = var_ft0;
        scale[2] = var_ft0;
        rot[0] = 0;
        rot[1] = 0;
        rot[2] = -var_ft1;
        pos[0] = (f32) -D_ovl0_802243C0;
        pos[1] = 20.0f;
        pos[2] = -300.0f;
        mtx_from_rts(model, rot, pos, scale);
        guTranslateF(view, (f32) D_ovl0_802243C0, -20.0f, 0.0f);
        guMtxCatF(view, model, model);
        convert_and_pack_floats_to_fixed(model, &D_ovl0_80227CA0[sky_panel_bank_index]);
        gSPMatrix(gfx++, &D_227CA0[sky_panel_bank_index], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(gfx++, D_ovl0_802243C8, 4, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        if (D_ovl0_802243B8 == 0xA) {
            func_80071608(&gfx);
            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xF0, 0xFF);
            func_80071CE4(&gfx, 0);
        } else {
            func_80075E48(&gfx);
            emit_glyph_sprite_dl(&gfx, 0);
        }
    }
    *glistp = gfx;
}

extern s32 D_ovl0_80224410;

s32 func_ovl0_8020FFD8(void) { return D_ovl0_80224410 == -1; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020FFEC);

extern s32 D_ovl0_80224418;

s32 func_ovl0_8021035C(void) { return D_ovl0_80224418 >= 0xC; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80210370);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_802104BC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80210F40);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8021169C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80211D60);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80212590);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_802129C0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80212CE0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213038);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213634);

extern s32 D_ovl0_802246F8;

s32 func_ovl0_80213C64(void) { return D_ovl0_802246F8 != -1; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213C78);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213D70);

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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80214250);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80216130);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_802161D4);
