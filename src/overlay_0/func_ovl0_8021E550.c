#include "common.h"

extern s32 flag_is_set(s32 flag);
extern void check_and_print_grid(char* str, s32 col, s32 row);
extern char D_80105118[];
extern s32 D_ovl0_80224BD0;
extern char D_ovl0_80225690[];
extern u16 debug_menu_pad_buttons_c;

void func_ovl0_8021E550(void) {
    u16 buttons;

    if (flag_is_set(0x6E) != 0) {
        buttons = debug_menu_pad_buttons_c;
        if (buttons & 0x800) {
            D_ovl0_80224BD0 -= 1;
        }
        if (buttons & 0x400) {
            D_ovl0_80224BD0 += 1;
        }
        sprintf(D_80105118, D_ovl0_80225690, D_ovl0_80224BD0);
        check_and_print_grid(D_80105118, 0x14, 5);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021E5F4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021E6B8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021E710);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021E724);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021EA48);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021EA58);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021FC20);
