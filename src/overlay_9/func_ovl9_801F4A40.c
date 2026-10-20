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

INCLUDE_ASM("asm/nonmatchings/overlay_9/func_ovl9_801F4A40", func_ovl9_801F4B68);

INCLUDE_ASM("asm/nonmatchings/overlay_9/func_ovl9_801F4A40", func_ovl9_801F4DA0);

INCLUDE_ASM("asm/nonmatchings/overlay_9/func_ovl9_801F4A40", func_ovl9_801F4FF4);
