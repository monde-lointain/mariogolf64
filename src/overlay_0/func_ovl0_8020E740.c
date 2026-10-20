#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"
#include "overlay_0.h"

extern void* heap3_alloc(u32 size_bytes);
extern void heap3_free(void** payload_ptr);
extern s32 D_ovl0_80224114;
extern s32 D_ovl0_80224120;
extern s32 D_ovl0_80224124;
extern void func_800772B0(void);
extern s32 D_ovl0_80224848;
extern s32 D_ovl0_8022484C;
extern s32 D_ovl0_80224408;
extern s32 D_ovl0_8022440C;
extern s32 D_ovl0_80224414;
extern s32 D_ovl0_80224418;
extern u16 D_ovl0_8022441C;
extern u16 D_ovl0_80224450;
extern u16 D_ovl0_80224474;
extern f32 D_ovl0_8022BC60;
extern f32 D_ovl0_8022BC64;
extern f32 D_ovl0_8022BC68;
const char D_ovl0_802252B8[] = "MOVIE %d \n";

void func_ovl0_8020E740(void) {
  RomAssetLoad asset;

  D_ovl0_80224114 = -1;
  D_ovl0_80224124 = 1;
  D_ovl0_8022D650 = heap3_alloc(0x6270);
  D_ovl0_80224120 = -1;
}

void func_ovl0_8020E78C(void) {
  D_ovl0_80224124 = 0;
  heap3_free((void**)&D_ovl0_8022D650);
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

void func_ovl0_8020FFEC(s32 event) {
  RomAssetLoad asset;
  RomLoadSlot slot;
  u32 size;

  osSyncPrintf(D_ovl0_802252B8, event);
  if (D_ovl0_80224124 != 1) {
    return;
  }
  D_ovl0_80224410 = event;
  if (event == -1) {
    return;
  }
  D_ovl0_80224120 = -1;
  D_ovl0_80224114 = -1;
  D_ovl0_80224408 = 0;
  D_ovl0_8022440C = 0;
  D_ovl0_80224414 = 0;
  D_ovl0_80224418 = D_ovl0_80224124;

  switch (event) {
    case -1:
      break;
    case 0: {
      u16* asset_id = &D_ovl0_8022441C;

      open_rom_asset_slot(0x92C, &slot);
      size = get_rom_load_slot_size(&slot);
      read_rom_load_slot(D_ovl0_8022D650, size, &slot);
      get_rom_asset_allocation_size(*asset_id, &asset);
      load_rom_asset_payload(D_ovl0_8022D650->event0.asset_200, &asset);
      get_rom_asset_allocation_size(*asset_id, &asset);
      load_rom_asset_payload(D_ovl0_8022D650->event0.asset_2880, &asset);
      break;
    }
    case 1:
      get_rom_asset_allocation_size(0x64E, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      get_rom_asset_allocation_size(0x64E, &asset);
      load_rom_asset_payload(D_ovl0_8022D650->event1.asset_1008, &asset);
      D_ovl0_8022BC60 = 384.0f;
      D_ovl0_8022BC64 = -64.0f;
      D_ovl0_8022BC68 = 0.7f;
      break;
    case 2:
    case 3:
    case 4:
    case 16:
      open_rom_asset_slot(0x914, &slot);
      size = get_rom_load_slot_size(&slot);
      read_rom_load_slot(D_ovl0_8022D650->event_slot4208.slot_4208, size, &slot);
      get_rom_asset_allocation_size(D_ovl0_80224450, &asset);
      load_rom_asset_payload(D_ovl0_8022D650->event_slot4208.asset_4408, &asset);
      break;
    case 5:
      open_rom_asset_slot(0x91F, &slot);
      size = get_rom_load_slot_size(&slot);
      read_rom_load_slot(D_ovl0_8022D650->event_slot4208.slot_4208, size, &slot);
      get_rom_asset_allocation_size(D_ovl0_80224474, &asset);
      load_rom_asset_payload(D_ovl0_8022D650->event_slot4208.asset_4408, &asset);
      break;
    case 14:
      get_rom_asset_allocation_size(0x55E, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 12:
      get_rom_asset_allocation_size(0x55C, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 13:
      get_rom_asset_allocation_size(0x55B, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 6:
      get_rom_asset_allocation_size(0x556, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 7:
      get_rom_asset_allocation_size(0x557, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 8:
      get_rom_asset_allocation_size(0x558, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 9:
      get_rom_asset_allocation_size(0x559, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 11:
      get_rom_asset_allocation_size(0x55A, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 10:
      get_rom_asset_allocation_size(0x55D, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      break;
    case 15:
      get_rom_asset_allocation_size(0x976, &asset);
      load_rom_asset_payload(D_ovl0_8022D650, &asset);
      get_rom_asset_allocation_size(0x977, &asset);
      load_rom_asset_payload(D_ovl0_8022D650->event15.asset_2508, &asset);
      break;
  }
}

s32 func_ovl0_8021035C(void) { return D_ovl0_80224418 >= 0xC; }
