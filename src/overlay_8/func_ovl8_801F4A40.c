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
extern void func_ovl8_801F4ABC(void);
extern s32 func_8006B55C(void);
extern void func_8006B56C(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern void func_8006B5A0(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern void func_8009226C(s32 arg0);
extern s32 func_800959F8(void);
extern void play_sound_effect(s32 arg0, s32 arg1, s32 arg2);
extern u8 D_800FBDC4[];
extern s32 D_800B6808;
extern s32 D_800BB020;
extern s32 D_800BB024;
extern s32 D_800BB02C;
extern s32 D_800BB030;
extern s32 D_ovl8_801F5920;

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

void func_ovl8_801F4D74(void) {
  s32 r;
  s32 g;
  s32 b;
  s32 i;
  s32 pressed;
  s32 controller_count;
  s32 offset;
  s32 loaded;

  if (D_80105218 != 0) {
    func_ovl8_801F4ABC();
    if (D_ovl8_801F5920 == 0) {
      r = 0;
      if (D_800B6808 == 1) {
        r = 0xFF;
        g = 0xFF;
        b = 0xFF;
      } else {
        g = 0;
        b = 0;
      }
      func_8006B5A0(r, g, b, 8);
      D_800B6808 = 3;
    }
    switch (D_ovl8_801F5924) {
    case 0:
      if ((func_8006B55C() != 0) && (func_800959F8() != 0)) {
        pressed = 0;
        i = 0;
        if (D_80105218 != 0) {
          controller_count = D_80105218;
          offset = 0;
          do {
            loaded = *(u16 *)(D_800FBDC4 + offset);
            offset += 0xE;
            i += 1;
            pressed |= loaded;
          } while (i != controller_count);
        }
        if (pressed & 0x9000) {
          func_8009226C(0xA);
          D_800BB020 = 0x19;
          D_800BB024 = 0xE;
          D_800BB02C = 4;
          D_800BB030 = 2;
          D_ovl8_801F5920 = 0;
          D_ovl8_801F5924 = 1;
          play_sound_effect(0x5D, 0xF, 0x7F);
        } else if (((pressed & 0x4000) && (g_debug_mode_enabled != 0)) || (D_ovl8_801F5920 == 0x276)) {
          D_ovl8_801F5924 = 2;
          func_8006B56C(0, 0, 0, 0x10);
        }
      }
      break;
    case 2:
      if (func_8006B55C() != 0) {
        D_800BB020 = 0x19;
        D_800BB024 = 0x1B;
        D_800BB02C = 4;
        D_800BB030 = 0xC;
        D_ovl8_801F5920 = 0;
      }
      break;
    }
    if (D_ovl8_801F5920 < 0x384) {
      D_ovl8_801F5920 += 1;
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4F80);
