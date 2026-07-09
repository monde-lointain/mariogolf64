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

extern u8 D_E473F0[];
extern s32 D_8012D3A8;

/* Read the size/offset directory entry for `index` from the D_E473F0 ROM table
 * (8 bytes/entry) into 16-byte-aligned DMA scratch, then fill the load slot:
 * start=pos=base+offset, size, end=start+size. Records index to D_8012D3A8.
 * `size_val` caches the size read so the D_8012D3A8 store schedules between the
 * end and size stores (matches ROM). */
void func_80050428(s32 index, RomLoadSlot* slot) {
  u8 offset_buf[0x40];
  u8 size_buf[0x40];
  u32 rom_offset = index * 8;
  u8* offset_aligned = (u8*)(((u32)(&offset_buf[0xF])) & (~0xF));
  u8* size_aligned = (u8*)(((u32)(&size_buf[0xF])) & (~0xF));
  u32 size_val;

  nuPiReadRom((u32)(D_E473F0 + rom_offset), size_aligned, 4);
  nuPiReadRom((u32)(rom_offset + (D_E473F0 + 4)), offset_aligned, 4);
  slot->pos = (u32)(D_E473F0 + (*((u32*)offset_aligned)));
  slot->start = slot->pos;
  slot->end = slot->pos + (*((u32*)size_aligned));
  size_val = *((u32*)size_aligned);
  D_8012D3A8 = index;
  slot->size = size_val;
}

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
