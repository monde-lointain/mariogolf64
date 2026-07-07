#include "common.h"

extern u8* func_8005AF50(void);
extern void play_sound_effect(s32 arg0, s32 arg1, s32 arg2);
extern void func_80099490(void);
extern void func_8005DF54(u8* arg0, s32 arg1);
extern void func_8005C674(s32 arg0, s32 arg1);

void func_ovl8_801F5604(void);
void func_ovl8_801F56E4(void);

void func_ovl8_801F55A0(void) {
  u8* state = func_8005AF50();

  play_sound_effect(0x69, 0xD, 0x50);
  func_80099490();
  func_8005DF54(state, 0);
  func_ovl8_801F5604();
  func_ovl8_801F56E4();
  func_8005DF54(state, 1);
}

void func_ovl8_801F5604(void) {
  register u8* state asm("$8") = func_8005AF50();

  {
    register s32 row asm("$6") = 0;
    register s32 one asm("$10") = 1;
    register s32 col_limit asm("$9") = 0x12;
    register s32 row_limit asm("$11") = 0xE;
    register s32 offset asm("$7") = 0;
    s32 col;
    s32 inner_offset;
    u8* ptr;

    do {
      col = 0;
      inner_offset = offset;
      do {
        ptr = state + inner_offset + 0xAF1;
        if ((*ptr & 0x7F) == 0) {
          *ptr = one;
        }
        col += 1;
        inner_offset += 2;
      } while (col != col_limit);
      row += 1;
      offset += 0x24;
    } while (row != row_limit);
  }

  {
    register s32 fill_row asm("$6") = 0;
    register s32 one asm("$7") = 1;
    register s32 six asm("$5") = 6;
    register u8* row_base asm("$4") = state;
    u8* ptr;
    u8* end;

    do {
      ptr = row_base + 0x1320;
      end = (u8*)(six + (s32)ptr);
      do {
        *ptr++ = one;
      } while (ptr != end);
      fill_row += 1;
      row_base += 6;
    } while (fill_row != six);
  }

  {
    register s32 fill_row asm("$6") = 0;
    register s32 one asm("$5") = 1;
    register s32 eighteen asm("$9") = 0x12;
    register s32 six asm("$7") = 6;
    register u8* row_base asm("$2") = state;
    u8* ptr;
    u8* end;

    do {
      ptr = row_base + 0xA84;
      end = (u8*)(eighteen + (s32)ptr);
      do {
        *ptr++ = one;
      } while (ptr != end);
      fill_row += 1;
      row_base += 0x12;
    } while (fill_row != six);
  }

  func_8005C674(1, 1);
}

INCLUDE_ASM("asm/nonmatchings/overlay_8/func_ovl8_801F55A0",
            func_ovl8_801F56E4);
