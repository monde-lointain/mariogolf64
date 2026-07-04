#include "common.h"

extern u8 flag;
extern s8 D_800B67C0;
extern s32 D_800BFEE8;
extern u8 D_800DAF60[];
extern u8 D_800DB410[];
extern s32 D_800DC6D0;

void print_string_at_grid(char* str, s32 col, s32 row);
void check_and_print_grid(char* str, s32 col, s32 row);

void print_string_at_grid(char* str, s32 col, s32 row) {
  u8 f = flag;
  u8* dst = &D_800DAF60[row * 40 + col];
  u8* end = &D_800DAF60[0x4B0];
  s32 c;
  while (1) {
    c = *str++;
    if (c == 0) {
      break;
    }
    if (dst >= D_800DAF60) {
      if (dst < end) {
        *dst = f | c;
      }
      dst++;
    } else {
      dst++;
    }
  }
  flag = 0;
}

void check_and_print_grid(char* str, s32 col, s32 row) {
  if (D_800B67C0 != 0) {
    print_string_at_grid(str, col, row);
  }
}

void func_8004DA4C(u32 num, s32 col, s32 row) {
  char buf[9];
  char* p = &buf[8];
  *p = 0;
  col += 8;
  do {
    s32 nib = num & 0xF;
    s32 d = nib + 0x30;
    if (d >= 0x3A) {
      d = nib + 0x37;
    }
    *--p = d;
    num >>= 4;
    col--;
  } while (num != 0);
  check_and_print_grid(p, col, row);
}

void convert_and_print_hex(u32 num, s32 col, s32 row) {
  char buf[9];
  char* p = &buf[8];
  char* stop;
  *p = 0;
  col += 8;
  stop = &buf[0];
  do {
    s32 nib = num & 0xF;
    s32 d = nib + 0x30;
    if (d >= 0x3A) {
      d = nib + 0x37;
    }
    *--p = d;
    col--;
    num >>= 4;
  } while (p != stop);
  check_and_print_grid(p, col, row);
}

void func_8004DAF4(u8* str) {
  u8 ch;
  s32 c;
  while (1) {
    ch = *str++;
    c = (s8)ch;
    if (c == 0) {
      break;
    }
    if (c == '\r' || c == '\n') {
      s32 n = 40;
      D_800DC6D0 = ((D_800DC6D0 / 40) * 40 + 40) % 4800;
      do {
        s32 pos = D_800DC6D0;
        D_800DC6D0 = pos + 1;
        D_800DB410[pos] = ' ';
        n--;
      } while (n != 0);
      D_800DC6D0 -= 40;
    } else {
      s32 pos = D_800DC6D0;
      D_800DC6D0 = pos + 1;
      D_800DB410[pos] = ch;
    }
    D_800DC6D0 %= 4800;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/print_string_at_grid", func_8004DC44);
