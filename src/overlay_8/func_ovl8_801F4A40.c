#include "common.h"

extern void func_ovl8_801F55A0(void);
extern s8 g_debug_mode_enabled;
extern void flag_set(s32 flag);
extern void func_8006F50C(void);
extern void set_clear_color(s32 red, s32 green, s32 blue);

void func_ovl8_801F4A40(void) { func_ovl8_801F55A0(); }

void func_ovl8_801F4A5C(void) { g_debug_mode_enabled = 1; }

void func_ovl8_801F4A6C(void) {
  flag_set(0x44);
  flag_set(0x65);
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4A90);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4ABC);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4C84);

void func_ovl8_801F4D48(void) {
  func_8006F50C();
  set_clear_color(0, 0, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4D74);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4F80);
