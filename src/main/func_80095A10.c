#include "common.h"

extern s32 D_800E4C5C;
extern u8 D_800E4CC8;
extern s32 D_800E4C98;
extern s32 D_800E4C9C;
extern s32 D_800E4CA0;
extern u8 D_800C7464[];

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095A10);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095A68);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095C10);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095D10);

void func_80095D64(void) { D_800E4C5C = 0; }

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095D70);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095DE0);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_8009676C);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800967F4);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80096C04);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80096F44);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80097218);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800974D8);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800977E0);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80097A08);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80097C18);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80097E30);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_8009806C);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098310);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800985B4);

void func_8009874C(void) { D_800E4CC8 = 0; }

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098758);

void func_800989C4(s32* arg0) {
  s32 v;
  v = D_800E4C98;
  arg0[0] = v;
  v = D_800E4C9C;
  arg0[1] = v;
  v = D_800E4CA0;
  arg0[2] = v;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800989EC);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098C6C);

s32 func_80098CA0(s32 arg0) {
  u16 temp;
  s32 result = 0;
  s32 i = 7;

  do {
    temp = (result & 0xFF) << 1;
    result = temp;
    if (arg0 & 1) {
      result = temp | 1;
    }
    arg0 = (u32)(arg0 & 0xFF) >> 1;
    i--;
  } while (i >= 0);

  return result & 0xFF;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098CD8);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098D70);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098E48);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800990D0);

u8* func_80099454(s32 arg0) {
  u8 index = arg0 & 0xFF;

  if (index >= 0xC0) {
    return &D_800C7464[0];
  }
  return &D_800C7464[index * 3];
}
