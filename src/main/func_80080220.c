#include "common.h"

extern u32 sky_panel_bank_index;
extern void func_8003E4B4(void);
extern void* heap3_alloc(s32 size);
extern void func_80026400(u8* block, s32 size);
extern s32 D_800E2190[];
extern s32 D_800E21A8[];
extern s32 D_800C59E0;
extern u8* D_800C5470;

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080220);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220",
            init_sky_pool_and_world_state);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080564);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008060C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080688);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008085C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080C4C);

void func_80080DCC(void) {
  s32 unused[8];
  if (D_800C59E0 == 0) {
    D_800C5470 = heap3_alloc(0x7118);
    func_80026400(D_800C5470, 0x7118);
    D_800C59E0 = 1;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080E14);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080E7C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", load_course_scenery_assets);

void func_80081550(void) { func_8003E4B4(); }

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008156C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80081C90);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80081D4C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80081EF8);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_800824E4);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", draw_terrain_aim_grid);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80083A48);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80083AC8);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_800842C0);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80084468);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80084EBC);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_800852A8);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008534C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80085F98);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008658C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008679C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220",
            emit_ball_offscreen_indicator);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_800874D8);

void func_80087BAC(void) {
  s32 i;
  for (i = 0; i != 5; i++) {
    D_800E2190[i] = 0;
    D_800E21A8[i] = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80087BE4);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80087CB0);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_800880A0);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80088890);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80088A90);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80088BDC);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80089094);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008C520);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008C658);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008C6B0);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008CD30);

void toggle_sky_panel_bank_index(void) { sky_panel_bank_index ^= 1; }
