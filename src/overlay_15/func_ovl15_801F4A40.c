#include "common.h"
#include "../main/session.h"

typedef struct {
  /* 0x00 */ u8 unknown_00[0x30];
  /* 0x30 */ u8 checksum[2];
  /* 0x32 */ u8 payload[0x1CE];
} GbPakSaveRecord; /* 0x200 */

extern s32 D_ovl15_801FBB78;
extern void func_ovl15_801F7664(Gfx** dl, s32* arg1);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F4A40);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F4B20);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F4E70);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F4F50);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F54F8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5570);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5644);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5AF0);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5EA8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5F1C);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F6388);

void func_ovl15_801F6624(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F662C);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F66EC);

void func_ovl15_801F67F4(void) {
  D_800FEC00.unknown_00 = 0;
  D_800FEC00.unknown_04 = 0;
  D_800FEC00.unknown_08 = 0;
  D_800FEC00.unknown_8C = 0;
}

void func_ovl15_801F6818(void) {
  D_800FEC00.unknown_34 = 0;
  D_800FEC00.unknown_38 = 0;
  D_800FEC00.unknown_8C = 0;
}

void func_ovl15_801F6834(void) {
  D_800FEC00.unknown_34 = 0;
  D_800FEC00.unknown_04 = 0;
  D_800FEC00.unknown_38 = 0;
  D_800FEC00.unknown_8C = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F6858);

s32 func_ovl15_801F686C(GbPakSaveRecord *record) {
  s32 sum;
  u8 *cursor;

  sum = 0;
  cursor = record->payload;
  do {
    sum += *cursor;
    cursor++;
  } while (cursor != (u8 *)(record + 1));
  return sum;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F6890);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F68FC);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F69B8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F6E94);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7074);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F71F8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7664);

void func_ovl15_801F77D4(Gfx** dl, s32* arg1) {
    Gfx* saved = *dl;

    func_ovl15_801F7664(&saved, arg1);
    *dl = saved;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F780C);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7ED4);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F8110);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F8BC8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F8C60);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F91E4);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F93DC);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F9688);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F9CEC);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F9D80);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA0BC);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA320);

/* Return the overlay 15 state word: 0 idle, 1 ramping up, 2 ramping down, 3 active. */
s32 func_ovl15_801FA35C(void) {
    return D_ovl15_801FBB78;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA368);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA720);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA844);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA990);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FB348);
