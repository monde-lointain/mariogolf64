#include "common.h"

extern Gfx D_ovl3_80232300[];
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s32 D_800C2004;
extern s32 current_game_mode;

Gfx* func_ovl3_801F4A40(void) { return D_ovl3_80232300; }

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F4A4C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F4C34);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F4C78);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F6A38);

void func_ovl3_801F6B20(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F6B28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F70F0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F75EC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F7660);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F7CD0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F7D28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F8BAC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F93D8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9BCC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9C58);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9E38);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9FF8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FA1B8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FA20C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FADBC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FAF08);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FAF78);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FAFEC);

void func_ovl3_801FB11C(void) {
  if (D_800C2004 & 0x4000) {
    play_sound_effect(0x5F, 0xF, 0x7F);
    current_game_mode = 2;
  }
}

void func_ovl3_801FB15C(void) {
  if (D_800C2004 & 0x8000) {
    play_sound_effect(0x5D, 0xF, 0x50);
    current_game_mode = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FB198);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FB54C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FC60C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FC754);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FCB74);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FCEF8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FD1B4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FD4F0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FD7B0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FDA40);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FE4D0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802004A0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80200A40);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80200EF0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80201574);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80202000);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802047F0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80204FF0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80205038);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802058B8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80206180);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80206A2C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80206A68);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80207D08);

void func_ovl3_80207E18(void) {
}

void func_ovl3_80207E20(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80207E28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802081E8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_8020879C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80208F8C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80209430);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80209478);
