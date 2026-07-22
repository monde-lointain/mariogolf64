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
extern s16 D_800C54EA[];
extern s16 D_800C54F2[];
extern s16 D_800C56AA[];
extern s16 D_800C56B2[];
extern void func_8003E004(void);
extern void per_hole_skybox_palette_load(void);

void init_sky_panels(void) {
  s32 i;

  sky_panel_bank_index = 0;
  for (i = 0; i != 14; i++) {
    s32 off = i * 0x10;
    *(s16*)((u8*)D_800C54F2 + off) =
        (*(s16*)((u8*)D_800C54EA + off) / 10 + 0x10) << 5;
    *(s16*)((u8*)D_800C56B2 + off) =
        (*(s16*)((u8*)D_800C56AA + off) / 10 + 0x10) << 5;
  }
  func_8003E004();
  per_hole_skybox_palette_load();
}

/* init_sky_pool_and_world_state: CARRY (S256, near-match, body 100% RE'd).
 * Sky-dome Vtx-grid builder (4x5x6 + block-2/3 s-texcoord overwrite) + tint
 * fill
 * + guOrtho/2x guPerspective + 14 world-state zeros. TERMINAL = grid-builder
 * biv-regalloc + loop-bound-hoist (ROM re-materializes `li v0,5`/`li v0,4`
 * inline; my build hoists the bounds; 93 asm-differ rows). Grid-builder wall
 * class (S241 analog carried).
 * docs/wip/init_sky_pool_and_world_state.near-match.md;
 * [[grid-builder-CE88-regalloc-levers]].
 */
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

/* func_80080688: CARRY (S255 stretch, near-match — body fully RE'd, NOT
 * banked). Club/shot setup glue: func_8007E2B0/func_8007E234(2); if
 * putter_mode_flag==1 play_sound_effect(0,9,0x64)+D_80106190=100.0f else
 * update_object_group_by_id(9); func_8002646C; func_80051FCC;
 * idx=func_800520DC(); func_8005062C(D_800C54CC[idx]+idx, buf);
 * func_800506D4(D_800E21C0,&buf); func_80052070; idx=func_80051FCC();
 * terrain=D_800C1434[idx*210]; switch(terrain)
 * {3->func_8005062C(0x565,buf);4->0x566;5->0x567} shared &buf; func_800506D4(
 * D_800E2180,&buf); if(D_800C59E0==2)func_80080C4C(); then D_801B608C dispatch
 * (==9/==3 set D_800C59E4=1 + func_802418B8(0/1) once; else func_8008C520()).
 * TERMINAL RESIDUAL: the `idx*210` index multiply. ROM synthesizes it as an
 * ADD-only two-chain (idx*10 + idx*200, idx held in both s0 and v0); gcc-2.7.2
 * synth_mult here emits the SHORTER subtract-based form (`sll v0,3; subu; subu;
 * sll`) in EVERY source spelling (idx*210, idx*10+idx*200, byte-offset cast) —
 * it folds/re-synthesizes to the subu form, 5 instrs shorter, cascading the
 * tail
 * + a few delay-slot fills. Compiler-internal multiply synthesis, not source-
 * leverable; percent below the 0.97 permuter gate and
 * instruction-count-changing (permuter cannot add the missing words).
 * Escalation = gcc-2.7.2 synth_mult source dive (why the ROM avoided the
 * subtract for *210 — likely an rtx_cost / config difference) OR corpus-mine a
 * matched *210 struct-stride sibling.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80080688);

extern void func_801FD624(void);
extern void func_800789C8(void);
extern void func_8020D6F0(void);
extern void func_80216EDC(void);
extern void func_8020E740(void);
extern void func_8021E5F4(void);
extern s32 func_80051FCC(void);
extern void func_80077C18(s32 arg0);
extern s32 D_800C59D8;
extern s32 D_800C59DC;
extern void* D_801B55DC;
extern void* D_800E2188;
extern void* D_80132360;
extern void* D_800FED0C;
extern void* D_800FF4F0;
extern void* D_8012D428;
extern void* D_801B7EE0;
extern void* D_800E2140;
extern void* D_800E2144[];
extern void* D_800E2138;
extern void* D_800E2158;
extern void* D_800E214C[];
extern void* D_800E2184;
extern void* D_800E2164;
extern void* D_800E2168;
extern void* D_800E216C;
extern void* D_800E2170;
extern void* D_800E2174;
extern void* D_800E217C;
extern void* D_800E2178;
extern void* D_80105214;

void func_8008085C(void) {
  RomLoadSlot slot[2];
  u32 size;

  D_800C59E0 = 2;
  func_801FD624();
  func_800789C8();
  func_8020D6F0();
  func_80216EDC();
  func_8020E740();
  func_8021E5F4();

  size = func_8005062C(0x541, slot);
  D_801B55DC = heap3_alloc(size);
  func_800506D4(D_801B55DC, slot);

  size = func_8005062C(0x5A4, slot);
  D_800E2188 = heap3_alloc(size);
  func_800506D4(D_800E2188, slot);

  size = func_8005062C(0x5E3, slot);
  D_80132360 = heap3_alloc(size);
  func_800506D4(D_80132360, slot);

  size = func_8005062C(0x972, slot);
  D_800FED0C = heap3_alloc(size);
  func_800506D4(D_800FED0C, slot);

  size = func_8005062C(0x93C, slot);
  D_800FF4F0 = heap3_alloc(size);
  func_800506D4(D_800FF4F0, slot);

  size = func_8005062C(0x4DB, slot);
  D_8012D428 = heap3_alloc(size);
  func_800506D4(D_8012D428, slot);

  size = func_8005062C(0x4C3, slot);
  D_801B7EE0 = heap3_alloc(size);
  func_800506D4(D_801B7EE0, slot);

  D_800C59D8 = -1;
  size = func_8005062C(0x4E3, slot);
  D_800E2140 = heap3_alloc(size);
  func_800506D4(D_800E2140, slot);

  D_800E2144[0] = heap3_alloc(0x128);
  D_800E2144[1] = heap3_alloc(0x128);
  D_800C59DC = -1;
  D_800E2138 = heap3_alloc(0x1208);

  size = func_8005062C(0x648, slot);
  D_800E2158 = heap3_alloc(size);
  func_800506D4(D_800E2158, slot);

  size = func_8005062C(0x563, slot);
  D_800E214C[0] = heap3_alloc(size);
  func_800506D4(D_800E214C[0], slot);

  size = func_8005062C(0x564, slot);
  D_800E214C[1] = heap3_alloc(size);
  func_800506D4(D_800E214C[1], slot);

  size = func_8005062C(0x562, slot);
  D_800E214C[2] = heap3_alloc(size);
  func_800506D4(D_800E214C[2], slot);

  size = func_8005062C(0x57C, slot);
  D_800E2184 = heap3_alloc(size);
  func_800506D4(D_800E2184, slot);

  size = func_8005062C(0x97B, slot);
  D_800E2164 = heap3_alloc(size);
  func_800506D4(D_800E2164, slot);

  size = func_8005062C(0x725, slot);
  D_800E2168 = heap3_alloc(size);
  func_800506D4(D_800E2168, slot);

  size = func_8005062C(0x568, slot);
  D_800E216C = heap3_alloc(size);
  func_800506D4(D_800E216C, slot);

  size = func_8005062C(0x56B, slot);
  D_800E2170 = heap3_alloc(size);
  func_800506D4(D_800E2170, slot);

  size = func_8005062C(0x56A, slot);
  D_800E2174 = heap3_alloc(size);
  func_800506D4(D_800E2174, slot);

  size = func_8005062C(0x569, slot);
  D_800E217C = heap3_alloc(size);
  func_800506D4(D_800E217C, slot);

  size = func_8005062C(0x723, slot);
  D_800E2178 = heap3_alloc(size);
  func_800506D4(D_800E2178, slot);

  size = func_8005062C(0x724, slot);
  D_80105214 = heap3_alloc(size);
  func_800506D4(D_80105214, slot);

  func_80077C18(func_80051FCC());
}

extern void func_80078910(void);
extern void func_8020D784(void);
extern void func_80216F8C(void);
extern void func_8020E78C(void);
extern void func_8021E6B8(void);
extern void func_80077DEC(void);
extern void* D_801B55DC;
extern void* D_800E2188;
extern void* D_80132360;
extern void* D_800FED0C;
extern void* D_800FF4F0;
extern void* D_8012D428;
extern void* D_801B7EE0;
extern void* D_800E2140;
extern void* D_800E2144[];
extern void* D_800E2138;
extern void* D_800E2158;
extern void* D_800E214C[];
extern void* D_800E2184;
extern void* D_800E2164;
extern void* D_800E2168;
extern void* D_800E216C;
extern void* D_800E2170;
extern void* D_800E2174;
extern void* D_800E217C;
extern void* D_800E2178;
extern void* D_80105214;

void func_80080C4C(void) {
  if (D_800C59E0 == 2) {
    D_800C59E0 = 1;
    func_80078910();
    func_8020D784();
    func_80216F8C();
    func_8020E78C();
    func_8021E6B8();
    heap3_free(&D_801B55DC);
    heap3_free(&D_800E2188);
    heap3_free(&D_80132360);
    heap3_free(&D_800FED0C);
    heap3_free(&D_800FF4F0);
    heap3_free(&D_8012D428);
    heap3_free(&D_801B7EE0);
    heap3_free(&D_800E2140);
    heap3_free(&D_800E2144[0]);
    heap3_free(&D_800E2144[1]);
    heap3_free(&D_800E2138);
    heap3_free(&D_800E2158);
    heap3_free(&D_800E214C[0]);
    heap3_free(&D_800E214C[1]);
    heap3_free(&D_800E214C[2]);
    heap3_free(&D_800E2184);
    heap3_free(&D_800E2164);
    heap3_free(&D_800E2168);
    heap3_free(&D_800E216C);
    heap3_free(&D_800E2170);
    heap3_free(&D_800E2174);
    heap3_free(&D_800E217C);
    heap3_free(&D_800E2178);
    heap3_free(&D_80105214);
    func_80077DEC();
  }
}

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

extern void* course_panorama_ptr;
extern void* sky_cloud_texture_ptr;
extern void* D_800FBE0C;
extern void* D_800E21C0;
extern void* D_800E2180;
extern void* D_800E2160;
extern void* D_800FE3D4;
extern void* D_800FC89C;
extern void* D_80132CEC;
extern u16 D_800C54CC;
extern s16 D_801B7F30;
extern s16 D_801B7F32;
extern s16 D_801B7F34;
extern s16 D_801B7F36;
extern s32 D_800E21BC;
extern void func_8004C860(void* buf, s32 w, s32 h, s32 fmt);
extern void func_8006ACD8(void);
extern void func_8003E400(void);
extern void func_80078D94(void);

void load_course_scenery_assets(void) {
  RomLoadSlot slot_b;
  RomLoadSlot slot[2];
  u32 size;
  s32 i;

  size = func_8005062C(0x4BA, slot);
  course_panorama_ptr = heap3_alloc(size);
  func_800506D4(course_panorama_ptr, slot);

  size = func_8005062C(0x4D4, slot);
  D_800FBE0C = heap3_alloc(size);
  func_800506D4(D_800FBE0C, slot);

  size = func_8005062C(D_800C54CC, slot);
  D_800E21C0 = heap3_alloc(size);
  func_800506D4(D_800E21C0, slot);

  size = func_8005062C(0x567, slot);
  D_800E2180 = heap3_alloc(size);
  func_800506D4(D_800E2180, slot);

  size = func_8005062C(0x722, slot);
  D_800E2160 = heap3_alloc(size);
  func_800506D4(D_800E2160, slot);

  D_800FE3D4 = heap3_alloc(0x1000);
  func_8004C860(D_800FE3D4, 0x40, 0x40, 0x3E);

  for (i = 0; i != 5; i++) {
    D_800E2190[i] = 0;
    D_800E21A8[i] = 0;
  }

  D_801B7F30 = 8;
  D_801B7F32 = 8;
  D_801B7F34 = 0x138;
  D_800E21BC = 0;
  D_801B7F36 = 0xE8;

  sky_cloud_texture_ptr = heap3_alloc(0x1000);

  size = func_8005062C(0x4D7, slot);
  D_800FC89C = heap3_alloc(size);
  func_800506D4(D_800FC89C, slot);

  size = func_8005062C(0x4D9, slot);
  D_80132CEC = heap3_alloc(size);
  func_800506D4(D_80132CEC, slot);

  for (i = 0; i != 4; i++) {
    size = func_8005062C(D_800C547C[i] + 9, slot);
    D_801B55E0[i] = heap3_alloc(size);
    func_800506D4(D_801B55E0[i], slot);
    func_800504E8(D_800C54A0[i], &slot_b);
    size = func_80050598(&slot_b);
    D_801321C8[i] = heap3_alloc(size);
    func_800505A0(D_801321C8[i], size, &slot_b);
  }

  func_8006ACD8();
  func_8003E400();
  func_80078D94();
}

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

typedef struct {
  /* 0x0 */ u32 id;
} TerrainAttrEntry;

extern TerrainAttrEntry* get_table_entry(u32 idx);
extern void func_8007515C(s32, s32, s32, s32, s32, s32);
extern void func_8020FFEC(s32);
extern void func_800326FC(s32, s32, s32, s32);
extern void func_8004C510(s32);
extern s32 D_800C5A10;
extern s32 D_800C5A14;
extern s8 D_80104B6B;
extern s32 D_801B7F70;
extern s32 D_801B7F78;
extern s32 D_801B7F7C;
extern char D_800D1AD0[];

void func_80081D4C(s32 arg) {
  s32 state;

  D_800C5A14 = 0;
  if (arg == -1) {
    D_800C5A10 = arg;
    func_8007515C(0, -1, 0, 0, 0, 0);
    func_8007515C(1, -1, 0, 0, 0, 0);
    func_8020FFEC(-1);
    return;
  }
  state = D_800C5A10;
  if (state != -1) {
    return;
  }
  D_800C5A10 = arg;
  switch (get_table_entry(D_80104B6B)->id) {
    case 0xD:
      D_801B7F70 = 1;
      D_801B7F78 = 0x4D;
      D_800C5A10 = state;
      D_801B7F7C = 0x65;
      func_800326FC(0xA0, 0x80, 0x80, 0x80);
      func_8004C510(0x3C);
      func_8020FFEC(0);
      osSyncPrintf(D_800D1AD0);
      break;
    case 0xA:
    case 0x15:
      D_800C5A10 = 2;
      func_8004C510(0x3C);
      break;
    case 0xB:
      func_8004C510(0x28);
      break;
    default:
      D_801B7F70 = 1;
      D_801B7F78 = 0x4D;
      D_801B7F7C = 0x4D;
      func_8004C510(0x28);
      func_800326FC(0xA0, 0xA0, 0x40, 0x38);
      break;
  }
}

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

/* func_800842C0: CARRY (S255) chain-USED GCC nested function, NOT a raw-DL-word
 * regalloc wall. A texrect glyph-string emitter (col,row,char* str): classifies
 * each char (space skip / 'm' special / digits) and emits a G_TEXRECT +
 * RDPHALF1/2 + 0x04000400 DL block per glyph. The glist pointer is a LOCAL of
 * the caller func_80084468, reached through the GCC static chain ($v0): child
 * spills incoming $v0 (`sw v0,0(sp)`), keeps it in t0, and does `lw a1,0(t0)` =
 * *(chain) = parent glistp, advancing it in place. Parent func_80084468:582
 * `addiu $v0,$sp,0x10` sets chain=&(glistp local at sp+0x10), maintains the
 * glistp there, reloads it after each `jal func_800842C0` (:611). Real out-of-
 * line jal (not inlined), so NOT an orphan -> no volatile byte-repro. Bank only
 * by writing it nested inside the decompiled func_80084468 (2644B FP DL
 * builder, its own slice). docs/wip/func_800842C0.near-match.md;
 * docs/hazards.md#nested-function-static-chain-spill;
 * memory [[nested-function-static-chain-spill]].
 */
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

extern Gfx* glistp;
extern u16 sky_panel_persp_norm;
extern Mtx D_800E2050;
extern Mtx D_E2050;
extern Mtx D_FE2F0;
extern void mtx_from_rts(f32 mf[4][4], f32 rotate[3], f32 translate[3],
                         f32 scale[3]);
extern void convert_and_pack_floats_to_fixed(f32 mf[4][4], Mtx* m);
extern void func_80085F98(s32 arg0);

void func_8008658C(void) {
  f32 translate[3];
  f32 scale[3];
  f32 rotate[3];
  f32 mf[4][4];

  translate[0] = -240.0f;
  translate[1] = 180.0f;
  translate[2] = -500.0f;
  scale[0] = 0.1f;
  scale[1] = 0.1f;
  scale[2] = 0.1f;
  rotate[0] = 0.0f;
  rotate[1] = 0.0f;
  rotate[2] = 0.0f;
  mtx_from_rts(mf, rotate, translate, scale);
  convert_and_pack_floats_to_fixed(mf, &D_800E2050);

  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetCycleType(glistp++, G_CYC_1CYCLE);
  gSPLoadGeometryMode(glistp++, 0);
  gSPSetGeometryMode(glistp++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
  gDPPipeSync(glistp++);
  gDPSetRenderMode(glistp++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
  gSPPerspNormalize(glistp++, sky_panel_persp_norm);
  gDPPipeSync(glistp++);
  gDPSetTexturePersp(glistp++, G_TP_PERSP);
  gSPMatrix(glistp++, &D_FE2F0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(glistp++, &D_E2050, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

  func_80085F98(0);

  gDPPipeSync(glistp++);
}

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

/* func_80088A90: CARRY (S254) glyph/units-string DL emitter, S243 raw-DL-word
 * wall class. Renders a numeric string (digits + 'f'/'m'/'y' unit glyphs, '.'
 * decimal, ' ' skip) into a hand-rolled 6-word-per-char G_TEXRECT block
 * (0xE4/0xE1/0xF1 command words), advancing x by a per-glyph width and mapping
 * each char to a font tile offset ('f'->0x60 w0x10, '.'->0x70 w4, 'm'->0x50
 * w8, 'y'->0x58 w8, digit->(c-0x30)*8 w8). Body is structurally COMPLETE: the
 * per-char emit block (sll/andi/sll12/or/sw pairs, dl+=0x18, cursor+=8) is
 * byte-identical in isolation. TERMINAL divergence is pervasive regalloc +
 * reorg branch-likely: (1) ROM keeps the char MASKED in a separate reg
 * (`andi v1,t0,0xFF` redundant on the already-lbu'd byte) while the raw load
 * stays in t0 for the `bnez` loop test; my build collapses both into one reg,
 * cascading the whole allocation (ROM dl=t1/cursor=a2/word_lo=t3; mine
 * dl=t2/cursor=t1/word_lo=a2). (2) ROM classifies with `beq`/`beql`
 * branch-likely TOWARD physically-later handler labels (space-skip + 'm'/'.'
 * set their width/advance in the annulled delay slot, reorg.c optimize_skip); a
 * structured if-else emits `bne`-past + nop, unreproducible from faithful C.
 * Kin to func_80071370.c's 73F24/74230/74500 (all carried). Permuter-class
 * (regalloc + reorg coin). Near-match C in
 * docs/wip/func_80088A90.near-match.md.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80088A90);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80088BDC);

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80089094);

extern s32 scenario_mode_id[];
extern s32 D_801B6098;
extern s8 D_801B60BA;
extern s8 D_801B60BB;
extern s32 D_801B60A0;
extern s32 func_80052100(s32 mode, s32 arg1);
extern s32 func_80052168(s32 mode, s32 arg1);
extern s32 func_8005244C(s32 arg0, s32 arg1);

void func_8008C520(void) {
  RomLoadSlot slot[2];
  u32 size;
  s32 id;

  D_800C5ED8 = 0;
  if (D_801B60BA != -1 && func_80052100(scenario_mode_id[0], D_801B6098) != 0) {
    id = 0x554;
    goto alloc1;
  }
  if (D_801B60BB != -1 && func_80052168(scenario_mode_id[0], D_801B6098) != 0) {
    id = 0x555;
    goto alloc1;
  }
  goto slot2;
alloc1:
  size = func_8005062C(id, slot);
  D_800E3970 = heap3_alloc(size);
  func_800506D4(D_800E3970, slot);
  D_800C5ED8 |= 1;
slot2:
  if ((D_801B608C == 2) | (D_801B608C == 4)) {
    if (D_801B60A0 != 0) {
      id = 0x8FF;
    } else {
      s32 status = func_8005244C(0, 1);
      if ((u32)(status - 1) >= 2) {
        return;
      }
      id = 0x8FC;
    }
    size = func_8005062C(id, slot);
    D_800E3974 = heap3_alloc(size);
    func_800506D4(D_800E3974, slot);
    D_800C5ED8 |= 2;
  }
}

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

extern s8 D_801061CE;
extern s8 D_800FC872;
extern s8 D_800FC86D;
extern s8 rumble_disable_flag;
extern s32 D_801B60B0;
extern char D_800D1C84[];
extern char D_800D1CD0[];
extern char D_800D1CD4[];
extern char D_800C1420[];
extern s32 D_800C2AD8[];
extern s32 D_800C2B00[];
extern s32 func_80242418(void);
extern void func_80242424(Gfx** chain);
extern void func_80242D40(Gfx** chain);
extern void func_800734F0(Gfx** chain, s32 arg1);
extern void func_8007624C(Gfx** chain, s32 x, s32 y, char* str, u32 fg, u32 bg,
                          s32 arg6);

void func_8008CD30(Gfx** chain) {
  Gfx* dl = *chain;
  s32 mode;
  s32 total_secs;
  s32 secs;
  s32 mins;
  s32 centi;

  if (D_801061CE == 1) {
    mode = D_801B608C;
    if (mode == 0xB) {
      if (D_800FC872 != 0 && func_80242418() != 0) {
        func_80242424(&dl);
      }
    } else if (mode != 9) {
      if (mode == 3) {
        if (D_800FC86D != 0) {
          func_80242D40(&dl);
        }
      } else if (mode == 7 && rumble_disable_flag == 0) {
        total_secs = D_801B60B0 / 30;
        centi = (s32)(D_801B60B0 * 3.3333334f) % 100;
        secs = total_secs % 60;
        mins = total_secs / 60;
        func_800734F0(&dl, 0);
        sprintf(D_80105118, D_800D1C84, mins, secs, centi);
        func_8007624C(&dl, 0xA8, 0xB0, D_80105118, 0xFFFF0000, 0x30FF0000,
                      0x80);
      } else {
        func_800734F0(&dl, 0);
        gDPSetPrimColor(dl++, 0, 0, 255, 0, 0, 255);
        gDPSetEnvColor(dl++, 255, 255, 0, 255);
        sprintf(D_80105118, D_800D1CD0, &D_800C1420[func_80051FCC() * 0xC8]);
        sprintf(D_80105118, D_800D1CD0, &D_800C1420[func_80051FCC() * 0xC8]);
        func_8007624C(&dl, 0xA0, 0x20, D_80105118,
                      D_800C2AD8[scenario_mode_id[0]],
                      D_800C2B00[scenario_mode_id[0]], 0x10);
        sprintf(D_80105118, D_800D1CD4, D_801B6098 + 1);
        func_8007624C(&dl, 0xA0, 0x30, D_80105118,
                      D_800C2AD8[scenario_mode_id[0]],
                      D_800C2B00[scenario_mode_id[0]], 0x10);
        gDPPipeSync(dl++);
        gDPPipeSync(dl++);
        gDPSetCycleType(dl++, G_CYC_1CYCLE);
      }
    }
  }
  *chain = dl;
}

void toggle_sky_panel_bank_index(void) { sky_panel_bank_index ^= 1; }
