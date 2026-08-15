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

/* Marks every course/character grid cell in the save block as unlocked, then
 * floors the 0x15CC counter at 0xBB8.
 *
 * `set` keeps the stored 1 in a register born with the other loop constants
 * (loop.c hoists it in emission order, so a bare literal is born last).
 * The empty asm does two jobs and emits no bytes: it separates the two sets of
 * `p`, so loop.c cannot take them as one consecutive movable group and hoist
 * `off + ret` into the preheader (the ROM recomputes it per column), and its
 * `off` operand adds the reference that puts `off` ahead of `base` in
 * global.c allocno_compare (6250 vs 6363 without it). */
void func_ovl8_801F56E4(void) {
  u8* ret = func_8005AF50();
  s32 i = 0, set = 1, n = 6;
  s32 rowoff = 0;
  s32 off;
  u8* base = ret;
  do {
    s32 j = 0;
    s32 coloff;
    u32 flag;
    off = rowoff;
    coloff = 0;
    flag = (u32)base + 0x1320;
    do {
      u32 p;
      u32 addr;
      u32 end;
      *(u8*)flag = set;
      p = off + (u32)ret;
      __asm__ __volatile__("" : : "r"(off));
      p += 0x1344;
      addr = coloff + p;
      end = addr + 0xD;
      do {
        *(u8*)addr = set;
        addr++;
      } while (addr != end);
      coloff += 0x12;
      j++;
      flag++;
    } while (j != n);
    rowoff += 0x6C;
    i++;
    base += 6;
  } while (i != n);
  if (*(s32*)(ret + 0x15CC) < 0xBB8) {
    *(s32*)(ret + 0x15CC) = 0xBB8;
  }
}
