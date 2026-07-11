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

extern u8 D_8010CFA8;
extern u8 D_8010CFA9;
extern u8 D_8010CFAA;

void func_800718F4(u8 r, u8 g, u8 b, s32 idx) {
  (&D_8010CFA8)[idx * 3] = r;
  (&D_8010CFA9)[idx * 3] = g;
  (&D_8010CFAA)[idx * 3] = b;
}

/* func_80071924: 2D setter D_8012F52C[idx][j] = val (field 0x1C of the 0x2C
 * struct array at D_8012F510, viewed as s32[][11]). Structure fully solved:
 *   *(s32*)((u8*)&D_8012F52C + idx * 0x2C + j * 4) = val;
 * Carried INCLUDE_ASM: #base-register-vs-displacement. ROM materializes the
 * base (lui + addiu = full &D_8012F52C) into a reg and stores 0(reg); gcc-2.7.2
 * always FOLDS the constant %lo into the sw displacement (sw val,-0xad4(at)),
 * one instr shorter. 3 source forms (flat ptr+2 index, s32[][11] 2D array,
 * explicit row-pointer intermediate) all fold identically; no source lever
 * blocks the %lo fold. Permuter does not flip this class. */
INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071924);

extern u8 D_801321B0;
extern u8 D_801321C3;

void func_80071954(u8* src) {
  u8* dst = &D_801321B0;
  u8* end = src + 0x13;

  do {
    u8 c = *src;
    *dst = c;
    if ((u8)(c - 0x61) < 0x1A) {
      *dst = c + 0x80;
    }
    if (*src == 0) {
      break;
    }
    src++;
    dst++;
  } while (src != end);
  D_801321C3 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800719A0);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071B34);

extern s32 D_8012F51C;

void func_80071C74(s32 idx) { *(s32*)((u8*)&D_8012F51C + idx * 0x2C) = 0xFF; }

typedef struct {
  u8 pad0[0xC];
  s32 unk_0C;
  s32 unk_10;
  s32 unk_14;
  u8 pad18[0x14];
} Elem2F510; /* stride 0x2C */

extern Elem2F510 D_8012F510[];
extern u16* D_800E1C48;

s32 func_80071C9C(s32 idx) {
  Elem2F510* e = &D_8012F510[idx];
  s32 t = e->unk_14;
  u16* base = D_800E1C48;
  s32 v = e->unk_0C;

  return v >= (s32)base[t * 2 + 3];
}

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

extern u8 D_800FF573;

/* func_8007512C: inits byte field 0x?? (D_800FF573) of all 300 elements of a
 * 0x48-stride array to -1. Structure fully solved:
 *   for (i = 0; i != 300; i++) *(s8*)((u8*)&D_800FF573 + i * 0x48) = -1;
 * Carried INCLUDE_ASM: #base-register-vs-displacement + a value-immediate diff.
 * (1) ROM re-materializes lui %hi(D_800FF573) INSIDE the loop each iter with a
 * running byte-offset and folds %lo into the sb; gcc-2.7.2 strength-reduces to
 * a hoisted running pointer (base materialized once above the loop). (2) ROM
 * `li a1,-1` for the stored byte; gcc emits `li a1,0xff` (truncates the -1
 * constant to the byte value at compile time) regardless of s8/u8 pointer type.
 * Both are no-source-lever gcc codegen choices; permuter does not flip them. */
INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007512C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007515C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800754BC);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007580C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80075E48);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800760CC);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80076138);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007624C);
