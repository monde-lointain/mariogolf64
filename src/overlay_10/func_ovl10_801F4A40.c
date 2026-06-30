#include "common.h"

extern u16 D_800FBDC4;
extern s32 D_800BB020;
extern s32 D_800BB024;
extern s32 D_800BB02C;
extern s32 D_800BB030;

extern void func_8009226C(s32 arg0);
extern void play_sound_effect(s32 arg0, s32 arg1, s32 arg2);
extern void func_800719A0(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4,
                          s32 arg5, s32 arg6, s32 arg7, s32 arg8, s32 arg9);

void func_ovl10_801F4A40(void) {
  if (D_800FBDC4 & 0x9000) {
    func_8009226C(8);
    D_800BB020 = 0x19;
    D_800BB024 = 0x1B;
    D_800BB02C = 0xD;
    D_800BB030 = 0xC;
    play_sound_effect(0x5D, 0xF, 0x7F);
    func_800719A0(0xA0, 0x72, -1, 1, 0, 0, 0, 0, 0, 0);
  }
}

s32 func_ovl10_801F4AD8(s32* arg0) { return *arg0 = *arg0; }
