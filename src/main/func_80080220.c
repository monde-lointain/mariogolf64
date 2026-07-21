#include "common.h"

extern u32 sky_panel_bank_index;
extern void func_8003E4B4(void);
extern void* heap3_alloc(s32 size);
extern void func_80026400(u8* block, s32 size);
extern s32 D_800E2190[];
extern s32 D_800E21A8[];
extern s32 D_800C59E0;
extern u8* D_800C5470;
extern s32 D_800C5ED8;
extern void* D_800E3970;
extern void* D_800E3974;
extern void heap3_free(void** payload_ptr);
extern void func_800263B0(void);
extern void func_8007E2B0(void);
extern void func_80216B74(s32 arg0);
extern void func_80080C4C(void);
extern u8 D_801061B8[];
extern s32 D_801B608C;
extern s32 D_800C59E4;
extern void func_80241CE8(void);
extern void func_8008085C(void);
extern void func_8008C658(void);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080220);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220",
            init_sky_pool_and_world_state);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080564);

void func_8008060C(void) {
  s32 state = D_801B608C;
  if (state == 9) {
    goto reset_state;
  }
  if (state != 3) {
    goto cleanup_temp;
  }
reset_state:
  if (D_800C59E4 == 1) {
    D_800C59E4 = 0;
    func_80241CE8();
  }
  goto check_init;
cleanup_temp:
  func_8008C658();
check_init:
  if (D_800C59E0 == 1) {
    func_8008085C();
  }
}

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

void func_80080E14(void) {
  if (D_800C59E0 != 0) {
    func_800263B0();
    heap3_free((void**)&D_800C5470);
    func_8007E2B0();
    func_80216B74(1);
    func_80080C4C();
    D_800C59E0 = 0;
    memset(D_801061B8, 0, 0x8C);
  }
}

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

void func_8008C658(void) {
  if (D_800C5ED8 & 1) {
    heap3_free(&D_800E3970);
  }
  if (D_800C5ED8 & 2) {
    heap3_free(&D_800E3974);
  }
  D_800C5ED8 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008C6B0);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008CD30);

void toggle_sky_panel_bank_index(void) { sky_panel_bank_index ^= 1; }
