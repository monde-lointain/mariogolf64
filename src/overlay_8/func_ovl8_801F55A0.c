#include "common.h"

extern u8* func_8005AF50(void);
extern void func_8005DF54(u8* save, s32 arg1);
extern void play_sound_effect(s32 arg0, s32 arg1, s32 arg2);
extern s32 func_80099490(void);

void func_ovl8_801F5604(void);
void func_ovl8_801F56E4(void);

void func_ovl8_801F55A0(void) {
  u8* save = func_8005AF50();
  play_sound_effect(0x69, 0xD, 0x50);
  func_80099490();
  func_8005DF54(save, 0);
  func_ovl8_801F5604();
  func_ovl8_801F56E4();
  func_8005DF54(save, 1);
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F55A0",
            func_ovl8_801F5604);

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F55A0",
            func_ovl8_801F56E4);
