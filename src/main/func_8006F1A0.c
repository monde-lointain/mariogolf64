#include "common.h"

extern s32 D_800FE4A8;
extern s32 D_800C42D4;
extern u8 D_800FF1E8[];
extern u8 D_800FF1E9[];
extern s32 D_800FF210[];
extern s32 D_800FF21C[];
extern s32 D_800FF220[];
extern s32 D_800FE450[];
extern s32 D_800FE4AC;
extern s32 D_800FE4B0;
extern s32 D_800FE4B4;
extern s32 D_800FE4B8;
extern s32 D_800FEC00;
extern char D_800D1540[];

void func_8006F1A0(s32 arg0, s32 arg1) {
  s32 off = arg0 * 140;
  *(s32*)((u8*)D_800FF210 + off) += arg1;
  if (*(s32*)((u8*)D_800FF210 + off) >= 10000) {
    *(s32*)((u8*)D_800FF210 + off) = 9999;
  }
}

s32 func_8006F1F0(void) { return D_800FE4A8; }

s32 func_8006F1FC(s32 arg0) { return D_800FF1E8[arg0 * 140] == 1; }

u8* func_8006F228(s32 arg0) { return &D_800FF1E9[arg0 * 140]; }

void func_8006F24C(s32 arg0) {
  s32 i;
  s32 bound;
  s32* fe = D_800FE450;
  s32 val;
  s32* p;

  D_800FE4B0 = arg0;
  D_800FE4AC = 0;
  if (arg0 == 0) {
    i = 0;
    val = -1;
    bound = 4;
    p = fe + 471;
    do {
      s32 off = i * 140;
      *(u8*)((u8*)D_800FF1E8 + off) = 0;
      *(s32*)((u8*)D_800FF21C + off) = val;
      *(s32*)((u8*)D_800FF220 + off) = val;
      *p++ = val;
      i++;
    } while (i != bound);
    D_800FE4B4 = 0;
    D_800FEC00 = 0;
    D_800FE4B8 = 0;
  } else {
    D_800FE4B4 = 0;
    D_800FEC00 = 0;
  }
}

void func_8006F2E8(void) { D_800C42D4 = 0; }

s32 func_8006F2F4(void) { return D_800C42D4; }

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F300);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F404);

void func_8006F4F0(void) { nuContRmbForceStop(); }

void func_8006F50C(void) {
  nuContRmbForceStop();
  osSyncPrintf(D_800D1540);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F534);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F5E0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F734);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006FE88);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_800708B4);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_80070BCC);
