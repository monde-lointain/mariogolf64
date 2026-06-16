#include "common.h"

extern u32 __osSpGetStatus(void);

OSYieldResult osSpTaskYielded(OSTask *tp) {
  u32 status = __osSpGetStatus();
  u32 bit = status >> 8;
  bit &= 1;

  if (status & 0x80) {
    *(u32 *)((u8 *)tp + 4) = (*(u32 *)((u8 *)tp + 4) | bit) & ~2;
  }
  return bit;
}
