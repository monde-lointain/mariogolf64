#include "common.h"

typedef struct {
  /* 0x0 */ u32 start;
  /* 0x4 */ u32 size;
  /* 0x8 */ u32 pos;
  /* 0xC */ u32 end;
} RomLoadSlot; /* 0x10 */

extern RomLoadSlot D_800DE8A0[2];

extern void osSyncPrintf(const char*, ...);
extern const char D_800CCB70[];
extern const char D_800CCB7C[];

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

void func_80050428(s32, RomLoadSlot*);

void func_800504E8(s32 index, RomLoadSlot* slot) { func_80050428(index, slot); }

RomLoadSlot* func_80050504(s32 index) {
  s32 count;
  u8* slot_ptr;

  count = 0;
  slot_ptr = (u8*)D_800DE8A0;
  while (1) {
    RomLoadSlot* slot = (RomLoadSlot*)slot_ptr;
    count += 1;
    if (slot->start == 0) {
      func_80050428(index, slot);
      return slot;
    }
    if (count == 2) {
      break;
    }
    slot_ptr += 0x10;
  }

  osSyncPrintf(D_800CCB70, index);
  osSyncPrintf(D_800CCB7C, index);
  return NULL;
}

void func_80050588(RomLoadSlot* slot) {
  slot->end = 0;
  slot->pos = 0;
  slot->start = 0;
}

u32 func_80050598(RomLoadSlot* slot) { return slot->size; }
