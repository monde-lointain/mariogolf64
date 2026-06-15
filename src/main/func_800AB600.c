#include "common.h"

extern u32 __osSpGetStatus(void);

u32 func_800AB600(void *arg0) {
  u32 status = __osSpGetStatus();
  u32 bit = status >> 8;
  bit &= 1;

  if (status & 0x80) {
    *(u32 *)((u8 *)arg0 + 4) = (*(u32 *)((u8 *)arg0 + 4) | bit) & ~2;
  }
  return bit;
}
