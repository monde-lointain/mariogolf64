#include "common.h"
#include "../main/rom_asset.h"

extern void func_ovl8_801F55A0(void);
extern s8 g_debug_mode_enabled;
extern void flag_set(s32 flag);
extern void func_8006F50C(void);
extern void set_clear_color(s32 red, s32 green, s32 blue);
extern s32 D_80105218;
extern s32 D_ovl8_801F5924;
extern u8 D_ovl8_801F5B30[];
extern u8 D_ovl8_80220740[];
extern u8 D_ovl8_8022BB50[];
extern void func_8005DC50(void);
extern void play_bgm_by_id(s32 id, s32 flags);

typedef struct Ovl8TextRecord {
  u8 *text;
  s32 cursor;
  void (*on_end)(void);
} Ovl8TextRecord;

extern Ovl8TextRecord D_ovl8_801F5928[4];

void func_ovl8_801F4A40(void) { func_ovl8_801F55A0(); }

void func_ovl8_801F4A5C(void) { g_debug_mode_enabled = 1; }

void func_ovl8_801F4A6C(void) {
  flag_set(0x44);
  flag_set(0x65);
}

void func_ovl8_801F4A90(void) {
  u32 i;

  i = 0;
  do {
    D_ovl8_801F5928[i].cursor = 0;
    i += 1;
  } while (i < 4U);
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4ABC);

void func_ovl8_801F4C84(void) {
  RomAssetLoad asset;
  s32 bgm_id;

  func_ovl8_801F4A90();
  func_8006F50C();
  func_8005DC50();
  if (D_80105218 == 0) {
    get_rom_asset_allocation_size(0x948, &asset);
    load_rom_asset_payload(D_ovl8_801F5B30, &asset);
    bgm_id = 5;
  } else {
    get_rom_asset_allocation_size(0x6F4, &asset);
    load_rom_asset_payload(D_ovl8_801F5B30, &asset);
    bgm_id = 0x2A;
  }
  play_bgm_by_id(bgm_id, 0x80);
  get_rom_asset_allocation_size(0x6F5, &asset);
  load_rom_asset_payload(D_ovl8_80220740, &asset);
  get_rom_asset_allocation_size(0x943, &asset);
  load_rom_asset_payload(D_ovl8_8022BB50, &asset);
  D_ovl8_801F5924 = 0;
}

void func_ovl8_801F4D48(void) {
  func_8006F50C();
  set_clear_color(0, 0, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4D74);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4F80);
