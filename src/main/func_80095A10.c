#include "common.h"

extern s32 D_800E4C5C;
extern u8 D_800E4CC8;
extern s32 D_800E4C98;
extern s32 D_800E4C9C;
extern s32 D_800E4CA0;
extern u8 D_800C7464[];
extern s32 D_800E4C50;
extern s32 D_800E4C54;
extern s32 D_800E4C58;
extern u8 D_800E4C90;
extern u8 D_800E4C91;
extern s32 D_800C73A0;
extern s32 D_800C73E0;
extern u8* D_800C73F0[];
extern Light D_800C73B0[3];

extern s32 func_80056494(s32 arg0, s32 arg1);
extern void play_sound_effect(s32 sfx, s32 arg1, s32 arg2);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095A10);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095A68);

s32 func_80095C10(Gfx** gdlp, Lights3* src) {
  Gfx* gdl = *gdlp;
  if (D_800E4C5C != 0) {
    D_800C73B0[0] = src->l[0];
    D_800C73B0[1] = src->l[1];
    gSPNumLights(gdl++, NUMLIGHTS_3);
    gSPLight(gdl++, &D_800C73B0[0], 1);
    gSPLight(gdl++, &D_800C73B0[1], 2);
    gSPLight(gdl++, &D_800C73B0[2], 3);
    gSPLight(gdl++, (Light*)((u8*)D_800C73B0 - 8), 4);
    *gdlp = gdl;
    return 1;
  }
  *gdlp = gdl;
  return 0;
}

void func_80095D10(s32 arg0, s32 arg1, s32 arg2) {
  D_800E4C50 = arg1;
  D_800E4C5C = 0;
  D_800C73A0 = 0;
  D_800E4C90 = 0;
  D_800E4C91 = 0;
  D_800E4C54 = arg2;
  D_800E4C58 = func_80056494(arg0, arg2);
}

void func_80095D64(void) { D_800E4C5C = 0; }

void func_80095D70(s32 arg0, s32 arg1) {
  if (arg1 == -1) {
    return;
  }
  if (D_800C73A0 - 1 != arg0) {
    return;
  }
  play_sound_effect(arg1, D_800C73E0, 0x7F);
  D_800C73E0 = D_800C73E0 + 1;
  if (D_800C73E0 >= 8) {
    D_800C73E0 = 4;
  }
}

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

s32 func_80098D70(u8* dst, u8* src, s32 count) {
  s32 offset;
  s32 i;
  for (i = 0; i < count; i++) {
    s32 k;
    s32 matched;
    for (k = 0; k < 28; k++) {
      u8* p = D_800C73F0[k];
      matched = 0;
      if (*p != 0) {
        u8 c = src[i];
        while (*p != 0) {
          /* Empty block is codegen-load-bearing: the basic-block boundary
             breaks the CSE extended-basic-block so `*p` reloads at the loop
             top each iteration (matches ROM; permuter-confirmed). */
          if (1) {
          }
          if (*p == c) {
            matched = 1;
            break;
          }
          p++;
        }
      }
      if (matched) {
        break;
      }
    }
    if (k >= 28) {
      return -1;
    }
    offset = i - 28;
    dst[i] = (k - offset) % 28;
  }
  return 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098E48);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800990D0);

u8* func_80099454(s32 arg0) {
  u8 index = arg0 & 0xFF;

  if (index >= 0xC0) {
    return &D_800C7464[0];
  }
  return &D_800C7464[index * 3];
}
