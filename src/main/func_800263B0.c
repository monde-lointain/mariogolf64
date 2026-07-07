#include "common.h"

extern char D_800CA0D0[];
extern u8 g_sprite_decoded_cache[];
extern s8 D_800B6564;

void heap_init(s32 slot);

void func_800263B0(void) {
  s32 offset;

  osSyncPrintf(D_800CA0D0);
  offset = 0;
  do {
    *(s32*)&g_sprite_decoded_cache[offset] = 0;
    offset += 0x10;
  } while ((u32)offset < 0x2E0);
  heap_init(1);
  D_800B6564 = 0;
}
