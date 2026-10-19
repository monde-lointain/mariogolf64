#include "common.h"

extern void func_ovl8_801F55A0(void);
extern s8 g_debug_mode_enabled;

void func_ovl8_801F4A40(void) { func_ovl8_801F55A0(); }

void func_ovl8_801F4A5C(void) { g_debug_mode_enabled = 1; }

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4A6C);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4A90);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4ABC);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4C84);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4D48);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4D74);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F4A40", func_ovl8_801F4F80);
