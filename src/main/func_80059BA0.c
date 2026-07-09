#include "common.h"

extern u8 D_801323A0[];
extern s8 D_80105DC2;
extern s32 D_80105DC4;
extern s32 D_800C2B28[];
extern s32 D_800FE334;
extern u8 D_801052E8;
extern u8 D_801B9410;
extern u8 D_800C2BD4;
extern s32 current_game_mode;
extern u8 D_8012F720[];
extern s32 D_801B6098;
extern s32 scenario_mode_id;
extern u8 D_8012D400[];
extern u32 D_8012F724;
extern s32 D_80105DCC;
extern s32 D_80105DC8;

extern u8 rumble_disable_flag;
extern u8 D_800FE3EC;
extern u8 D_800FE402;
extern u8 D_800FE418;
extern u8 D_800FE42E;
extern s8 D_800C1FF4;
extern s8 D_800C1FF5;
extern s8 D_800C1FF6;
extern s8 D_800C1FF7;
extern s32 D_800C2B3C;

u8* func_8005AF74(void);
void func_8005DF54(u8*, s32);
s32 func_80099490(void);
void func_80029A6C(s32);
void func_8005D9A0(void);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059BA0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059BC0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059FAC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005A2AC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005A580);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005ACF8);

u8* func_8005AF50(void) {
  func_8005AF74();
  return D_8012F720;
}

u8* func_8005AF74(void) { return D_801323A0; }

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005AF80);

void func_8005B03C(void) {
  u8* temp = func_8005AF50();
  func_80099490();
  func_8005DF54(temp, 0);
}

s32 func_8005B070(s32 arg0) {
  s32 i = 5;
  s32* p = &D_800C2B3C;
  while (i != 0) {
    if (arg0 >= *p) {
      break;
    }
    i--;
    p--;
  }
  return i + 1;
}

s32 func_8005B0A0(s32 arg0) { return D_800C2B28[arg0]; }

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B0B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B150);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B28C);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B314);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B7BC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005BC10);

void func_8005C018(u32 arg0) {
  u32* p = &D_8012F724;
  if (*p < arg0) {
    *p = arg0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C038);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C458);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C4B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C510);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C5B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C614);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C674);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CA48);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CEE0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CF78);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D0D8);

void func_8005D1E4(void) {
  u8* p = func_8005AF74();
  p[0x2E] = D_801B6098;
  p[0x2F] = scenario_mode_id;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D218);

s32 func_8005D23C(void) { return D_80105DC2; }

s32 func_8005D248(void) { return D_80105DC4 != 0; }

s32 func_8005D258(void) {
  s32 x = D_80105DC8;
  return (x & (~x >> 31)) + 0x16F;
}

s32 func_8005D274(void) {
  s32 v = func_8005D23C();
  return (v & (~v >> 31)) + 0x1EB;
}

s32 func_8005D2A0(void) { return (D_80105DCC > 0) ? D_80105DCC : 1; }

void func_8005D2B8(u8* arg0) {
  s32 i = 0;
  do {
    *arg0++ = D_8012D400[i];
  } while (++i != 3);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D2E4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D308);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D334);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D3B8);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D9A0);

void func_8005DAD4(void) {
  D_800FE334 = 0;
  D_801052E8 = 1;
  D_801B9410 = 0;
  D_800C2BD4 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DAFC);

void func_8005DC50(void) {
  func_8005D9A0();
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  D_800FE3EC = 0;
  D_800FE402 = 0;
  D_800FE418 = 0;
  D_800FE42E = 0;
  D_800C1FF4 = -1;
  D_800C1FF5 = -1;
  D_800C1FF6 = -1;
  D_800C1FF7 = -1;
  current_game_mode = 0;
}

void func_8005DCDC(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0x17;
}

void func_8005DD10(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 5;
}

void func_8005DD44(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0x12;
}

void func_8005DD78(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0xD;
}

void func_8005DDAC(void) {
  D_800FE334 = 0;
  D_801052E8 = 1;
  D_800C2BD4 = 0;
  D_801B9410 = 1;
  rumble_disable_flag = 0;
  func_80029A6C(0x1E);
  current_game_mode = 0xC;
}

s8 func_8005DE00(void) { return ((s8*)func_8005AF50())[0x28]; }

s8 func_8005DE20(void) { return ((s8*)func_8005AF50())[0x29]; }

s8 func_8005DE40(void) { return ((s8*)func_8005AF50())[0x2B]; }

void func_8005DE60(void) { func_8005DF54(func_8005AF50(), 1); }

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE88);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DF54);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DFE8);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005E180);
