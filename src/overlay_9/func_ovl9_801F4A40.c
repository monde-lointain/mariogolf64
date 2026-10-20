#include "common.h"
#include "../main/rom_asset.h"

extern void bgm_request_stop(void);
extern u8 D_ovl9_801F57D0[];
extern u8 D_ovl9_801F69E0[];
extern u8 D_ovl9_801F8D30[];
extern s32 gameplay_viewport_x;
extern s32 gameplay_viewport_y;
extern s32 gameplay_viewport_w;
extern s32 gameplay_viewport_h;
extern s32 view_camera_eye_x;
extern s32 view_camera_eye_z;
extern s32 D_801B54F0;
extern s32 D_801B54F8;
extern s32 D_801B54FC;
extern f32 D_801B5500;
extern s32 D_801B5518;
extern s32 D_801B551C;
extern s32 D_801B5520;
extern s32 D_801B5524;
extern f32 D_801B5528;
extern s32 D_801B552C;
extern f32 D_801B5530;
extern s32 func_8006F1F0(void);
extern s32 func_8006F2F4(void);
extern void func_8006F2E8(void);
extern void func_8006F24C(s32 arg0);
extern void func_8006ADF8(s32 arg);
extern void func_80069FBC(void);
extern void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags,
                          u8 red, u8 green, u8 blue, u8 alpha);
extern s32 D_800B6808;
extern s32 D_800BB020;
extern s32 D_800BB024;
extern s32 D_800BB02C;
extern s32 D_800BB030;
extern s32 D_80105218;
extern s32 D_ovl9_801F577C;
extern s32 g_terrain_enable_gate;

/* Overlay 9 init hook, called through OverlayDescriptor.initialize after DMA and bss clear. */
void func_ovl9_801F4A40(void) {
  RomAssetLoad asset;

  get_rom_asset_allocation_size(0x702, &asset);
  load_rom_asset_payload(&D_ovl9_801F57D0, &asset);
  get_rom_asset_allocation_size(0x6F6, &asset);
  load_rom_asset_payload(&D_ovl9_801F69E0, &asset);
  get_rom_asset_allocation_size(0x97F, &asset);
  load_rom_asset_payload(&D_ovl9_801F8D30, &asset);
  bgm_request_stop();
  gameplay_viewport_x = 8;
  gameplay_viewport_y = 8;
  gameplay_viewport_w = 0x130;
  gameplay_viewport_h = 0xE0;
  D_801B5518 = 0xE0;
  D_801B551C = 0xE0;
  D_801B5520 = 0x140;
  D_801B5524 = 0xF0;
  view_camera_eye_x = 0;
  D_801B54F0 = 0;
  view_camera_eye_z = 0;
  D_801B54F8 = 0;
  D_801B54FC = 0;
  D_801B552C = 1;
  D_801B5500 = -100.0f;
  D_801B5528 = 1.0f;
  D_801B5530 = 45.0f;
}

void func_ovl9_801F4B68(void) {
  D_800B6808 = 2;
  if ((func_8006F1F0() != 0) && (func_8006F2F4() != 0)) {
    func_8006F2E8();
    D_800BB020 = 0x19;
    D_800BB02C = 0xC;
    D_800BB024 = 0x1D;
    D_800BB030 = 0x1B;
    func_8006F24C(0);
    func_8006ADF8(2);
  } else {
    if ((D_ovl9_801F577C != 0) && (g_terrain_enable_gate == 1) && (D_80105218 != 0)) {
      D_800BB020 = 0x19;
      D_800BB02C = 0xC;
      D_800BB024 = 0x13;
      D_800BB030 = 3;
    } else {
      func_80069FBC();
      D_800BB020 = 0x19;
      D_800BB02C = 0xC;
      D_800BB024 = 0x15;
      D_800BB030 = 4;
    }
  }
  func_800719A0(0xA0, 0x72, -1, 1, 0, 0, 0, 0, 0, 0);
  func_800719A0(0xA0, 0x72, -1, 1, 1, 0, 0, 0, 0, 0);
  func_800719A0(0xA0, 0x72, -1, 1, 2, 0, 0, 0, 0, 0);
  func_800719A0(0xA0, 0x72, -1, 1, 3, 0, 0, 0, 0, 0);
  func_800719A0(0xA0, 0x72, -1, 1, 4, 0, 0, 0, 0, 0);
  func_800719A0(0xA0, 0x72, -1, 1, 5, 0, 0, 0, 0, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_9/func_ovl9_801F4A40", func_ovl9_801F4DA0);

INCLUDE_ASM("asm/nonmatchings/overlay_9/func_ovl9_801F4A40", func_ovl9_801F4FF4);
