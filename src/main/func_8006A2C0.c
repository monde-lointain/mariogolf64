#include "common.h"

extern s32 D_800C4010;
extern s32 D_800C4014;
extern f32 D_800C401C;
extern s32 D_800C4020;
extern s32 D_800C4060;
extern s32 D_800C4064;
extern s32 D_800C4068;
extern s32 D_800C4124;
extern s32 D_800C4144;
extern s32 D_800FF4B0;
extern s32 D_800FF4B4;
extern s32 D_800FF4B8;
extern void* D_800E1C04;
extern void* D_800E1C08;
extern void* D_800E1C0C;

extern void* heap3_alloc(u32 need);
extern void heap3_free(void** payload_ptr);
extern u32 func_8005062C(u16 index, void* out);
extern void func_800506D4(void* data, void* slot);
void func_8006B54C(void);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A2C0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A4A0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A548);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A5E4);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A84C);

void func_8006ACD8(void) {
  u8 sp10[0x20];

  D_800E1C04 = heap3_alloc(func_8005062C(0x63F, sp10));
  func_800506D4(D_800E1C04, sp10);
  func_8006B54C();
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006AD1C);

void func_8006AD88(void) { D_800C4020 = 0; }

s32 func_8006AD94(void) { return D_800C4020; }

s32 func_8006ADA0(void) {
  if (D_800C4010 == 0) {
    return (s32)D_800C401C;
  }
  if (D_800C4014 == 1) {
    return 0;
  }
  if (D_800C4014 == 3) {
    D_800C4010 = 0;
    return 20;
  }
  return 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006ADF8);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006AEA4);

void func_8006B54C(void) { D_800C4068 = -1; }

s32 func_8006B55C(void) { return D_800C4064 < 2; }

void func_8006B56C(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
  D_800C4068 = 0;
  D_800C4060 = arg3;
  D_800C4064 = arg3;
  D_800FF4B0 = arg0;
  D_800FF4B4 = arg1;
  D_800FF4B8 = arg2;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006B5A0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006B5D8);

void func_8006B980(void) {
  u8 sp10[0x20];

  D_800E1C08 = heap3_alloc(func_8005062C(0x70B, sp10));
  func_800506D4(D_800E1C08, sp10);
  D_800E1C0C = heap3_alloc(func_8005062C(0x70C, sp10));
  func_800506D4(D_800E1C0C, sp10);
}

void func_8006B9E4(void) {
  heap3_free(&D_800E1C08);
  heap3_free(&D_800E1C0C);
}

s32 func_8006BA10(void) { return !~D_800C4124; }

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006BA24);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006BA94);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006BC80);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006C3E8);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006C450);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006C484);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006C8CC);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006C918);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006CD50);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006CE88);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D058);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D164);

s32 func_8006D1FC(void) { return D_800C4144; }

void func_8006D208(s32 arg0) { D_800C4144 = arg0; }

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D214);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D38C);

void func_8006D4DC(void) { D_800C4144 = -1; }

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D4EC);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D6D0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DDCC);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DE44);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DEB4);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DF84);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DFF0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006E210);
