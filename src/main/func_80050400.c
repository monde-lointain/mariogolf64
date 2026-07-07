#include "common.h"

typedef struct {
  /* 0x0 */ u32 start;
  /* 0x4 */ u32 size;
  /* 0x8 */ u32 pos;
  /* 0xC */ u32 end;
} RomLoadSlot; /* 0x10 */

extern RomLoadSlot D_800DE8A0[2];

void func_80050400(void) {
  s32 i;
  u8* slot;

  i = 0;
  slot = (u8*)D_800DE8A0;
  do {
    *(s32*)slot = 0;
    i += 1;
    slot += 0x10;
  } while (i != 2);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80050400", func_80050428);

INCLUDE_ASM("asm/nonmatchings/main/func_80050400", func_800504E8);

INCLUDE_ASM("asm/nonmatchings/main/func_80050400", func_80050504);

INCLUDE_ASM("asm/nonmatchings/main/func_80050400", func_80050588);

INCLUDE_ASM("asm/nonmatchings/main/func_80050400", func_80050598);
