#include "common.h"

typedef struct {
  /* 0x0 */ u32 start;
  /* 0x4 */ u32 size;
  /* 0x8 */ u32 pos;
  /* 0xC */ u32 end;
} RomLoadSlot; /* 0x10 */

extern s32 D_8012D3A8;

extern const char D_800CCB9C[];
extern const char D_800CCBAC[];

extern s32 flag_is_set(s32 flag);
extern void osSyncPrintf(const char*, ...);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);
extern void func_800504E8(s32 index, RomLoadSlot* slot);
extern u32 func_80050598(RomLoadSlot* slot);
extern void func_80050710(void* data, RomLoadSlot* slot);
extern void func_80050914(void* data, RomLoadSlot* slot);

void func_800505A0(void* dst, u32 size, RomLoadSlot* slot) {
  if (slot->end < slot->start + size) {
    size = slot->end - slot->start;
    osSyncPrintf(D_800CCB9C, D_8012D3A8);
  }

  nuPiReadRom(slot->start, dst, (size + 1) & ~1);
  slot->start += size;
}

u32 func_8005062C(u16 index, void* out) {
  u8 scratch_storage[0x80];
  u8* scratch = (u8*)(((u32)scratch_storage + 0xF) & ~0xF);
  RomLoadSlot* slot;
  u32 value;

  if (flag_is_set(0x49)) {
    osSyncPrintf(D_800CCBAC, index);
  }

  slot = (RomLoadSlot*)((u8*)out + 0x10);
  func_800504E8(index, slot);
  ((RomLoadSlot*)out)->end = func_80050598(slot);
  func_800505A0(scratch, 4, slot);
  ((RomLoadSlot*)out)->pos = scratch[0];

  value = *(u32*)scratch & 0xFFFFFF;
  *(u32*)scratch = value;
  return value;
}

void func_800506D4(void* data, RomLoadSlot* slot) {
  if (slot->pos == 1) {
    func_80050710(data, slot);
  } else {
    func_80050914(data, slot);
  }
}
