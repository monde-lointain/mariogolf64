#include "common.h"

typedef struct {
  /* 0x0 */ u32 start;
  /* 0x4 */ u32 size;
  /* 0x8 */ u32 pos;
  /* 0xC */ u32 end;
} RomLoadSlot; /* 0x10 */

extern s32 D_8012D3A8;

extern const char D_800CCB9C[];

extern void osSyncPrintf(const char*, ...);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);

void func_800505A0(void* dst, u32 size, RomLoadSlot* slot) {
  if (slot->end < slot->start + size) {
    size = slot->end - slot->start;
    osSyncPrintf(D_800CCB9C, D_8012D3A8);
  }

  nuPiReadRom(slot->start, dst, (size + 1) & ~1);
  slot->start += size;
}

INCLUDE_ASM("asm/nonmatchings/main/func_800505A0", func_8005062C);

INCLUDE_ASM("asm/nonmatchings/main/func_800505A0", func_800506D4);
