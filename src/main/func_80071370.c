#include "common.h"

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071370);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800715A0);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071608);

extern s32 D_8012F524;

/* func_800718C4: inits field 0x14 (D_8012F524) of all 12 elements of the
 * 0x2C-stride struct array at D_8012F510 to -1. Structure fully solved:
 *   for (i = 0; i != 12; i++) *(s32*)((u8*)&D_8012F524 + i * 0x2C) = -1;
 * Carried INCLUDE_ASM: #local-alloc-qty-permutation. The counter and the
 * strength-reduced byte-offset giv allocate to v0/v1 OPPOSITE the ROM (ROM
 * counter=v1/off=v0; build counter=v0/off=v1) and the giv-init `move ,zero`
 * schedules before the hoisted `li -1`/`li 0xc` invariants instead of after.
 * 3 source forms (dual-IV explicit off, for(off=0) split, single-IV i*0x2C)
 * all yield the identical v0/v1 swap; no source lever flips local-alloc's
 * allocno order. Documented no-lever class (permuter 0 cracks). */
INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800718C4);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800718F4);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071924);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071954);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800719A0);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071B34);

extern s32 D_8012F51C;

void func_80071C74(s32 idx) { *(s32*)((u8*)&D_8012F51C + idx * 0x2C) = 0xFF; }

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071C9C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071CE4);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80072A08);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800734F0);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800738BC);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007399C);

s32 func_80073BF0(u8* str) {
  s32 len = 0;

  if (str[0] != 0) {
    do {
      str++;
      len++;
    } while (str[0] != 0);
  }
  return len;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80073C14);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80073F24);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074230);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074500);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800747B0);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074840);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800748D0);

void func_80074960(void) {}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074968);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074CA8);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074D0C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074E5C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074EFC);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80075010);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007512C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007515C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800754BC);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007580C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80075E48);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800760CC);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80076138);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007624C);
