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
extern s32 effect_spawn_pos[];
extern s32 D_800C5AA8;
extern f32 D_800E3370;
extern f32 D_800E3374;
extern f32 D_800E3378;
extern s32 flag_is_set(s32 flag);
extern void check_and_print_grid(char* str, s32 col, s32 row);
extern u16 debug_menu_pad_buttons_c;
extern s32 D_800C5AD0;
extern char D_80105118[];
extern char D_800D1B70[];

typedef struct {
  /* 0x0 */ u32 start;
  /* 0x4 */ u32 size;
  /* 0x8 */ u32 pos;
  /* 0xC */ u32 end;
} RomLoadSlot; /* 0x10 */

extern u32 func_8005062C(u16 index, void* out);
extern void func_800506D4(void* data, RomLoadSlot* slot);
extern void func_800504E8(s32 index, RomLoadSlot* slot);
extern u32 func_80050598(RomLoadSlot* slot);
extern void func_800505A0(void* dst, u32 size, RomLoadSlot* slot);
extern u16 D_800C547C[];
extern s16 D_800C54A0[];
extern void* D_801B55E0[];
extern void* D_801321C8[];
INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080220);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220",
            init_sky_pool_and_world_state);

void func_80080564(s32 arg0, s32 arg1, s32 arg2) {
  RomLoadSlot buf_a[2];
  RomLoadSlot buf_b;
  u32 size;

  func_8005062C(D_800C547C[arg1] + 9, buf_a);
  func_800506D4(D_801B55E0[arg0], &buf_a[0]);
  func_800504E8(D_800C54A0[arg1] + arg2, &buf_b);
  size = func_80050598(&buf_b);
  func_800505A0(D_801321C8[arg0], size, &buf_b);
}

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

/* func_80081C90: CARRY (S253) #base-register-vs-displacement /
 * #indexed-vs-pointer terminal loop variant. Two 14-iter RMW loops over the
 * fixed global array D_800C54F2 (u16 field, 0x10 stride, bank base =
 * sky_panel_bank_index*448): loop1 adds wind_magnitude/8192 to each; if the
 * bank's first field (signed) >= 0x4001, loop2 subtracts 0x2000 from each. ROM
 * keeps INDEXED addressing (v1 = pure byte offset, re-materializes
 * %hi(D_800C54F2)+v1 with a %lo displacement per access, TWICE per iteration
 * for the load+store); gcc-2.7.2 loop.c strength-reduction folds base+offset
 * into ONE walking pointer (0(v1)) in every source spelling (byte-offset cast,
 * array-index, counter-index, do-while, for). No pointer-giv (S235
 * func_8006F24C DEST_REG) recipe applies because this ROM uses no pointer giv
 * at all. Permuter-unreachable (addressing-mode + strength-reduction decision).
 * docs/wip/func_80081C90.near-match.md.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80081C90);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80081D4C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80081EF8);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_800824E4);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", draw_terrain_aim_grid);

void func_80083A48(void) {
  s32 i;
  s32 off;

  D_800C5AA8 = 0;
  for (i = 0; i != 128; i++) {
    off = i * 0xC;
    *(f32*)((u8*)&D_800E3370 + off) = effect_spawn_pos[0] * (1.0f / 1024.0f);
    *(f32*)((u8*)&D_800E3374 + off) = effect_spawn_pos[1] * (1.0f / 1024.0f);
    *(f32*)((u8*)&D_800E3378 + off) = effect_spawn_pos[2] * (1.0f / 1024.0f);
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80083AC8);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_800842C0);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80084468);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80084EBC);

void func_800852A8(void) {
  u16 buttons;

  if (flag_is_set(0x6E)) {
    buttons = debug_menu_pad_buttons_c;
    if (buttons & 0x8) {
      D_800C5AD0 -= 1;
    }
    if (buttons & 0x4) {
      D_800C5AD0 += 1;
    }
    sprintf(D_80105118, D_800D1B70, D_800C5AD0);
    check_and_print_grid(D_80105118, 0x14, 0x6);
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008534C);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80085F98);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_8008658C);

extern f32 D_800C5DF0;
extern s32 D_800C5DF4;
extern s8 D_8010623F;
extern char D_801B71D0[];
extern s32 func_80213C78(char* record);
extern void func_80050DA0(s32, s32, s32, s32, s32);

void func_8008679C(s32 arg) {
  if (arg == -1) {
    D_800C5DF0 = -1.0f;
    return;
  }
  if (arg == 0) {
    s32 result = func_80213C78(&D_801B71D0[D_8010623F * 0xB8]);
    if ((result != 0xA) & (result != 2)) {
      func_80050DA0(0x70, 0xE, 0x40, 0x1E, 0x7F);
    } else {
      func_80050DA0(0x71, 0xE, 0x60, 0x1E, 0x7F);
    }
  }
  D_800C5DF4 = arg;
  D_800C5DF0 = 0.0f;
}

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

extern f32 sqrtf(f32);

void func_80087BE4(s32 x, s32 y) {
  f32 dist;

  if (x == 0 && y == 0) {
    dist = sqrtf((f32)(x * x + y * y));
  } else {
    dist = 0.0f;
  }
  if (56.0f < dist) {
    f32 scale = 56.0f / dist;
    x = (s32)(x * scale);
    y = (s32)(y * scale);
  }
  D_800E2190[0] = x / 4;
  D_800E21A8[0] = -y / 4;
}

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
