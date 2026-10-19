#include "common.h"

extern void func_ovl8_801F55A0(void);
extern s8 g_debug_mode_enabled;
extern void flag_set(s32 flag);
extern void func_8006F50C(void);
extern void set_clear_color(s32 red, s32 green, s32 blue);

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

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4C84);

void func_ovl8_801F4D48(void) {
  func_8006F50C();
  set_clear_color(0, 0, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4D74);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4F80);
