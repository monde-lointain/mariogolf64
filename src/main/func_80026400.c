#include "common.h"

#include "heap.h"

extern char D_800CA0E0[];
extern char D_800CA0F0[];
extern char D_800CA118[];
extern char D_800CA134[];
extern char D_800CA14C[];
extern char D_800CA164[];
extern HeapBlock* D_800B6568;
extern void* D_800B656C;
extern s8 D_800B6564;

extern void* heap3_alloc(s32 size);
extern void heap3_free(void** payload_ptr);

void func_800263B0(void);

/* Register the region [block, block+size) as slot-1 heap and mark slot-1 ready.
 * CARRY (S207): structurally-complete delay-slot-fill/regalloc near-match.
 * TARGET saves `size` across func_800263B0 (`move s0,a1`) and computes `end`
 * late in the 263b0 delay slot (block->s1); my build's delay-slot filler pulls
 * `block+size` into osSyncPrintf's delay slot (block->s0, end->s1), consuming
 * a1 directly. No clean C trigger (int-reuse / pointer-arith / late-decl /
 * asm-memory-barrier all hoist the add). Body was: void func_80026400(u8*
 * block, s32 size) { void* end; osSyncPrintf(D_800CA0E0); func_800263B0(); end
 * = block + size; D_800B6568 = (HeapBlock*)block; D_800B656C = end;
 *     heap_add_region(1, (HeapBlock*)block, end); D_800B6564 = 1;
 *   } */
INCLUDE_ASM("asm/nonmatchings/main/func_80026400", func_80026400);

/* Re-register the previously recorded slot-1 region and mark slot-1 ready. */
void func_8002646C(void) {
  osSyncPrintf(D_800CA0F0);
  func_800263B0();
  heap_add_region(1, D_800B6568, D_800B656C);
  D_800B6564 = 1;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80026400",
            update_scenery_cylinder_transforms);

INCLUDE_ASM("asm/nonmatchings/main/func_80026400",
            project_sort_scenery_cylinders);

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", emit_scenery_billboard);

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", draw_scenery_opaque_pass);

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", draw_scenery_alpha_pass);

/* Duplicate `str` (length rounded up to a word, capped at 0xFF) into a fresh
 * slot-3 buffer with a 4-word header, then release both. Returns a status code:
 * 1 if `tag` is 0, 2 if `str` is empty, 3 if the rounded length is too large,
 * 0 on success. */
s32 func_80028110(char* str, s32 tag) {
  s32 len;
  void* hdr;
  void* buf;

  if (tag == 0) {
    osSyncPrintf(D_800CA118);
    return 1;
  }
  len = strlen(str);
  if (len == 0) {
    osSyncPrintf(D_800CA134);
    return 2;
  }
  len = (len + 3) & ~3;
  if ((u32)len >= 0x100) {
    osSyncPrintf(D_800CA14C);
    return 3;
  }
  hdr = heap3_alloc(4);
  buf = heap3_alloc(len);
  memcpy(buf, str, len);
  *(s32*)hdr = 0xA;
  *(s32*)hdr = len;
  *(s32*)hdr = tag;
  heap3_free(&hdr);
  heap3_free(&buf);
  return 0;
}

/* Sibling of func_80028110 with a read-back check: duplicate `str` into a
 * slot-3 buffer with a header marked 0xB, and if the stored length reads back
 * as zero clear `*out` and log. Returns 2 if `str` is empty, 3 if too large,
 * else 1. */
s32 func_80028204(char* str, s32* out) {
  s32 len;
  void* hdr;
  void* buf;

  len = strlen(str);
  if (len == 0) {
    osSyncPrintf(D_800CA134);
    return 2;
  }
  len = (len + 3) & ~3;
  if ((u32)len >= 0x100) {
    osSyncPrintf(D_800CA14C);
    return 3;
  }
  hdr = heap3_alloc(4);
  buf = heap3_alloc(len);
  memcpy(buf, str, len);
  *(s32*)hdr = 0xB;
  *(s32*)hdr = len;
  if (*(s32*)hdr == 0) {
    *out = 0;
    osSyncPrintf(D_800CA164);
  }
  heap3_free(&hdr);
  heap3_free(&buf);
  return 1;
}
