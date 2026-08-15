#include "common.h"

extern u8* func_8005AF50(void);
extern void func_8005DF54(u8* save, s32 arg1);
extern s32 func_80099490(void);

void func_ovl8_801F5780(void) {
  u8* save = func_8005AF50();
  s32 i;
  u8* row;
  func_80099490();
  func_8005DF54(save, 0);
  i = 0;
  {
    s32 set = 1, ncol = 3, nrow = 6;
    row = save;
    do {
      s32 j = 0;
      u8* p = row;
      do {
        *(s32*)(p + 0xEC8) = set;
        j++;
        p += 4;
      } while (j != ncol);
      i++;
      row += 0x74;
    } while (i != nrow);
  }
  func_8005DF54(save, 1);
}

extern void play_sound_effect(s32 arg0, s32 arg1, s32 arg2);
extern void func_ovl8_801F55A0(void);
extern s32 D_800C2000;
extern s32 D_ovl8_801F5960[];
extern s32 D_ovl8_801F5994[];
extern s32 D_ovl8_801F59C8;

/* Cheat-code watcher: appends the frame's button word to a 13-entry log and
 * compares the log against the fixed pattern; a full match plays the unlock
 * chime and runs the save-reset routine, any mismatch clears the log. */
void func_ovl8_801F57F8(void) {
  s32 in = D_800C2000;
  s32 i = 0;
  s32 n;

  if (in <= 0) {
    return;
  }
  n = D_ovl8_801F59C8;
  if (n < 0xD) {
    D_ovl8_801F5994[n] = in;
    D_ovl8_801F59C8 = n + 1;
  }
  if (D_ovl8_801F59C8 != 0) {
    s32 cnt = D_ovl8_801F59C8;
    s32* logged = D_ovl8_801F5994;
    s32* want = D_ovl8_801F5960;
  compare:
    if (*want != *logged) {
      goto compared;
    }
    logged++;
    i++;
    want++;
    if (i != cnt) {
      goto compare;
    }
  compared:
    if (i != D_ovl8_801F59C8) {
      i = 0;
      goto reset;
    }
    i = 0;
  }
  if (D_ovl8_801F59C8 != 0xD) {
    return;
  }
  i = 0;
  D_ovl8_801F59C8 = 0;
  do {
    D_ovl8_801F5994[i] = -1;
    i++;
  } while (i != 0xD);
  play_sound_effect(0x69, 0xD, 0x50);
  func_ovl8_801F55A0();
  return;

reset:
  i = 0;
  D_ovl8_801F59C8 = 0;
  do {
    D_ovl8_801F5994[i] = -1;
    i++;
  } while (i != 0xD);
}
