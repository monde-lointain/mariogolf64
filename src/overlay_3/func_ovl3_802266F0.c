#include "common.h"

extern u8* func_8005AF50(void);
extern u8* func_8005AF74(void);

extern s32 D_ovl3_80230E88;
extern f32 D_ovl3_80230E8C;
extern f32 D_ovl3_80230E90;
extern s32 D_ovl3_80230E9C;
extern s32 D_ovl3_80230EA0;

void func_ovl3_802266F0(void) {
    D_ovl3_80230E88 = 0;
    D_ovl3_80230E8C = 0.0f;
    D_ovl3_80230E90 = 0.0f;
    D_ovl3_80230E9C = 0;
    D_ovl3_80230EA0 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022671C);

void func_ovl3_80226754(void) {
    D_ovl3_80230E88 = 2;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226764);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226788);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802267B0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226904);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226958);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802269AC);

void func_ovl3_80226A34(void) {
  func_8005AF50();
  func_8005AF74();
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226A58);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022747C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802287EC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80228A80);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80228B60);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80228C00);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80229278);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802296C4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80229CB0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022A090);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022A43C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022A8F4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022ADE8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B360);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B46C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B5E0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B668);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B75C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B7E0);

extern s8 D_80105DBC[];

s32 func_ovl3_8022BDB0(void) {
    return ~D_80105DBC[4] == 0;
}

s8 func_ovl3_8022BDC4(void) {
    return D_80105DBC[4];
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022BDD0);

void func_ovl3_8022BDF8(void) {
}
