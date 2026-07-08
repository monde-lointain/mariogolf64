#include "common.h"

extern s8 D_800BA9A8;
extern s8 D_800BA9A9;
extern void* D_800DAD00;
extern void* D_800DAD04;
extern void* D_800DAD08;
extern void* wind_dl_buffer_addr;

extern void heap3_free(void** payload_ptr);

INCLUDE_ASM("asm/nonmatchings/main/func_8003E400", func_8003E400);

void func_8003E4B4(void) {
  heap3_free(&D_800DAD00);
  heap3_free(&D_800DAD04);
  heap3_free(&D_800DAD08);
  heap3_free(&wind_dl_buffer_addr);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8003E400",
            build_pin_or_cup_matrix_for_dad10);

void func_8003E628(void) { D_800BA9A8 = 1; }

void func_8003E638(void) { D_800BA9A9 = 1; }

INCLUDE_ASM("asm/nonmatchings/main/func_8003E400", func_8003E648);
