#include "common.h"

extern s32 D_ovl14_802457E8;
extern s32 D_ovl14_802457EC;
extern void heap3_free(void** payload_address);
extern void play_bgm_by_id(s32 id, s32 flags);
extern s32 D_ovl14_802457E0;
extern s32 D_ovl14_80245860;
extern void* D_ovl14_80245AB0;

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80241800);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_8024186C);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80241E54);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80242050);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80242294);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80243698);

void func_ovl14_80243880(void) {
    D_ovl14_802457E8 = 0;
    D_ovl14_802457EC = 5;
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80243898);

void func_ovl14_802439B0(void) {
    D_ovl14_802457E8 = 0;
    D_ovl14_802457EC = 5;
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_802439C8);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80243E28);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80244134);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80245260);

void func_ovl14_802452E4(void) {
    heap3_free(&D_ovl14_80245AB0);
    D_ovl14_802457E0 = 0;
    play_bgm_by_id(D_ovl14_80245860, 0x11);
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80245320);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80245598);
