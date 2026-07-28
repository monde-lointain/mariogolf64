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

/* init_sky_pool_and_world_state: CARRY (S256, rebuilt S258 to the EXACT
 * instruction count). Sky-dome Vtx-grid builder (4x5x6, then a block-2/3
 * s-texcoord overwrite) + tint fill + guOrtho/2x guPerspective + 14 world-state
 * zeros.
 *
 * S256 carried it at 93 asm-differ rows; it now builds at 161/161 instrs with
 * the instruction shapes aligned 1:1. The rebuild is about WHICH loops
 * gcc-2.7.2 is allowed to discover: loop.c only processes loops that emit
 * NOTE_INSN_LOOP_BEG, which for/while/do-while do and a goto loop does not.
 *  - the two OUTER grid loops must be GOTO loops: the ROM re-materialises
 *    `li v0,5` / `li v0,4` inline at the exit tests and does not
 * strength-reduce the block base (any structured spelling hoists both and folds
 * the base into a walking pointer, the S256 failure);
 *  - the INNER col loop must be STRUCTURED: the ROM has the two pointer givs
 *    (`a3` and `a3+0xF`) that only loop.c's SR produces (a goto here is 4
 * short);
 *  - every constant the ROM keeps in a register across a goto loop must be a
 *    local variable (0xB7 / 6 / 0xC00 / 0x1E / the pool base), one per loop and
 *    assigned where the ROM materialises it, since a goto loop de-hoists
 * literals;
 *  - `(Vtx*)(block_off + (u32)pool)` puts the integer operand first, matching
 * the ROM's `addu a3,t6,s0`. RESIDUAL: a systematic local-alloc register
 * permutation of the grid-1 locals (same register set, same quantity order,
 * different priority ranking) plus one 2-instruction swap where loop.c places
 * the giv init after the s/x inits. Permuter plateaued 615 -> 545 in 60k
 * iterations. Full reconstruction + the register mapping table in
 * docs/wip/init_sky_pool_and_world_state.near-match.md.
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
extern void* D_800E2158[];
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
  D_800E2158[0] = heap3_alloc(size);
  func_800506D4(D_800E2158[0], slot);

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
extern void* D_800E2158[];
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
    heap3_free(&D_800E2158[0]);
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
extern void build_radial_falloff_texture(void* buf, s32 w, s32 h, s32 fmt);
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
  build_radial_falloff_texture(D_800FE3D4, 0x40, 0x40, 0x3E);

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

extern s32 wind_magnitude;

/* Scrolls all 14 sky panels of the active bank by the wind, then wraps.
 * The goto loops are load-bearing: gcc-2.7.2's loop.c only strength-reduces
 * loops it discovers through NOTE_INSN_LOOP_BEG, which a structured for/while
 * emits and a goto loop does not. Every structured spelling folds the two
 * `%hi(D_800C54F2)+off` re-materialisations per iteration into one walking
 * pointer, which is 4 instrs/iteration SHORT of the ROM. The two separate
 * `countN` bound variables are load-bearing too: one shared bound spans both
 * loops, which raises its allocno priority and swaps it with `delta`. */
void scroll_sky_panels_by_wind(void) {
  s32 delta = wind_magnitude / 8192;
  s32 count1;
  s32 count2;
  s32 off;
  s32 i;

  off = sky_panel_bank_index * 448;
  i = 0;
  count1 = 14;
loop1:
  *(u16*)((u8*)D_800C54F2 + off) += delta;
  i++;
  off += 0x10;
  if (i != count1) {
    goto loop1;
  }

  off = sky_panel_bank_index * 448;
  if (*(s16*)((u8*)D_800C54F2 + off) < 0x4001) {
    return;
  }
  i = 0;
  count2 = 14;
loop2:
  *(u16*)((u8*)D_800C54F2 + off) -= 0x2000;
  i++;
  off += 0x10;
  if (i != count2) {
    goto loop2;
  }
}

typedef struct {
  /* 0x0 */ u32 id;
  /* 0x4 */ char* name_ptr;
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

extern s32 camera_position_x;
extern s32 camera_position_z;
extern s8 g_terrain_vtx_xform_mode;
extern s32 D_80104AE8[];
extern Vtx D_800C5A18[];
extern char D_800D1AE0[];
extern char D_800D1AF0[];
extern const f64 D_800D1B00; /* pi / 180 */
extern const f64 D_800D1B08; /* 2 * pi */
extern Mtx D_800E1DD0[];
/* Same matrix bank as D_800E1DD0, addressed through segment 0 for the RSP. */
extern Mtx D_E1DD0[];
/* The scene's projection matrix, referenced through segment 0. */
extern Mtx D_1B5638[];

extern s32 func_80052250(void);
extern s32 func_8003DFD0(s32 x, s32 z);
extern void func_80071608(Gfx** chain);
extern void func_8007580C(Gfx** chain, s32 pass);
extern void func_80075E48(Gfx** chain);
extern void func_80074840(s32* chain, s32 x, s32 y, u8* str);
extern void mtx_from_rts(f32 mf[4][4], f32 rotate[3], f32 translate[3],
                         f32 scale[3]);
extern void convert_and_pack_floats_to_fixed(f32 mf[4][4], Mtx* m);
extern void func_800747B0(Gfx** chain, s16 x, s16 y, char* str);

/**
 * Emits the rest-distance panel: the readout of how far the ball still lies
 * from the hole, over a quad that spins and scales into place.
 *
 * The distance is formatted in metres or in yards depending on the unit mode,
 * with the terrain name drawn beside it once the panel has finished animating.
 * Its opening frames show the aiming overlays instead, which the panel then
 * replaces. The animation timer drives both the quad's rotation and its scale,
 * and it stops advancing at 30, leaving the panel at rest.
 */
void emit_rest_distance_panel_dl(Gfx** gfxp) {
  f32 scale[3];
  f32 pos[3];
  f32 rot[3];
  f32 model[4][4];
  f32 view[4][4];
  Gfx* gfx = *gfxp;
  TerrainAttrEntry* ent;
  s32 dist;
  f32 ang;
  f32 alpha;

  if (D_800C5A10 != -1) {
    ent = get_table_entry(D_80104B6B);
    if (D_800C5A14 == 1) {
      func_8007515C(0, 0xC, 0, 0, 8, (s32)ent->name_ptr);
    }
    if (D_800C5A10 == 1 && func_80052250() == 0) {
      dist = func_8003DFD0(effect_spawn_pos[0] - camera_position_x,
                           D_80104AE8[0] - camera_position_z);
      if (g_terrain_vtx_xform_mode != 0 || ent->id == 0xB) {
        s32 metres = (s32)((f32)(dist * 300) * 7.119878e-05f);

        sprintf(D_80105118, D_800D1AE0, metres / 100, metres % 100, 0xFB);
      } else {
        s32 yards = (s32)((f32)(dist * 100) * 7.119878e-05f);

        sprintf(D_80105118, D_800D1AF0, yards / 100, yards % 100, 0xF9);
      }
      if (D_800C5A14 == 1) {
        func_8007515C(1, 0xA, 0, 0, -8, (s32)D_80105118);
      }
    }

    if (D_800C5A14 < 30) {
      D_800C5A14++;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0x9F);
    gDPPipeSync(gfx++);

    ang = ((f32)D_800C5A14 * 18.0f + 180.0f) * D_800D1B00;
    if (ang > D_800D1B08) {
      ang = 6.2831855f;
    }
    alpha = (f32)D_800C5A14 * 0.1f;
    if (alpha > 1.0f) {
      alpha = 1.0f;
    }

    rot[0] = 0.0f;
    rot[1] = 0.0f;
    rot[2] = -ang;
    pos[0] = -72.0f;
    pos[1] = 32.0f;
    pos[2] = -100.0f;
    scale[0] = alpha;
    scale[1] = alpha;
    scale[2] = alpha;
    mtx_from_rts(model, rot, pos, scale);
    guTranslateF(view, 72.0f, -20.0f, 0.0f);
    guMtxCatF(view, model, model);
    convert_and_pack_floats_to_fixed(model, &D_800E1DD0[sky_panel_bank_index]);

    gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &D_E1DD0[sky_panel_bank_index],
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, D_800C5A18, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);

    if (D_800C5A14 < 10) {
      func_80075E48(&gfx);
      func_8007580C(&gfx, 0);
      if (D_800C5A10 == 1) {
        func_8007580C(&gfx, 1);
      }
    } else {
      func_80071608(&gfx);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
      func_800747B0(&gfx, 0xA0, 0x60, ent->name_ptr);
      if (D_800C5A10 == 1 && func_80052250() == 0) {
        func_80074840((s32*)&gfx, 0xA0, 0x70, (u8*)D_80105118);
      }
    }
  }

  *gfxp = gfx;
}

u32 pack_shade_ramp_rgba(s32 shade, s32 alpha) {
  s32 r;
  s32 g;
  s32 b;

  b = 0xFF;
  if (shade == 0) {
    g = 0xFF;
    r = 0xFF;
  } else if (shade > 0) {
    /* The subtrahend must pass through `b`: with `b = r - shade` written
     * directly, cse sees `r` and `b` holding the same 0xFF and canonicalises
     * the minuend to `b`'s register (`subu a2,a2,a0`), where the ROM re-reads
     * `r`
     * (`subu a2,v0,a0`). Staging `shade` in `b` first clears that equivalence.
     */
    r = 0xFF;
    b = shade;
    b = r - b;
    g = b;
  } else {
    g = shade + 0xFF;
    r = g;
  }

  return (r << 24) | (g << 16) | (b << 8) | (alpha & 0xFF);
}

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

/* Trail mode: 0 off, 1 record-and-draw, anything else draw-only. */
extern s32 D_800C5AAC;
/* Master enable for the effect. */
extern s8 D_801061CF;
/* Suppresses the whole draw when it reads 1. */
extern s32 D_800BE69C;
/* View scale: biases the sampled height and the projected depth. */
extern s32 D_800BA700;
/* 128 * {s32 x, s32 y, s32 z} of projected screen positions. The three fields
 * are spelled as three symbols, not one array: with one symbol loop.c combines
 * the (i+1) accesses into one address giv, where the ROM keeps a %hi/%lo per
 * field. See the header comment. */
extern s32 D_800E2D70;
extern s32 D_800E2D74;
extern s32 D_800E2D78;
/* The 4-vertex quad the trail re-points per segment. */
extern Vtx D_800C5A60[];
/* The identity matrix this emitter installs, and its segment-0 alias. */
extern Mtx D_800E1E90[];
extern Mtx D_E1E90[];
extern s32 project_point_to_screen(f32, f32, f32, s32*);
extern f32 sqrtf(f32);

/**
 * Emits the ball trail: a fading white ribbon through the last 128 recorded
 * ball positions.
 *
 * In mode 1 the current ball position is first appended to a 128-entry ring of
 * world-space samples (the raw position scaled by 1/1024, with the height
 * pulled down by 0.27 * the view scale). Every sample is then projected to
 * screen space, walking BACKWARDS from the newest so that segment 0 is the
 * freshest; a sample the projector rejects is marked with -1 and skipped.
 *
 * Each surviving pair of adjacent samples becomes one quad, two triangles wide,
 * built by re-pointing the same four vertices through gSPModifyVertex rather
 * than by uploading new geometry: G_MWO_POINT_RGBA sets a white colour whose
 * alpha fades from 0x40 down by one step every four segments, and
 * G_MWO_POINT_XYSCREEN offsets the two endpoints by +-4 screen units along the
 * segment normal. Depth comes from G_ZS_PRIM plus a per-segment
 * gDPSetPrimDepth, so the ribbon z-sorts against the world without geometry.
 *
 * `proj` is the caller's matrix bank; the emitter restores the caller's
 * projection from `proj[7]` and pops its own modelview on the way out.
 *
 * Notes on the shape of this body:
 * - The first two loop guards repeat the five increments instead of jumping to
 *   the shared tail. That is not style: it gives the `off2` biv three increment
 *   insns, and loop.c:3804 subtracts `add_cost * bl->biv_count` from each giv's
 *   benefit, which drives the three combined (i+1) address givs to benefit <= 0
 *   and leaves all seven accesses as the ROM's %hi/addu/%lo. With one increment
 *   site they reduce to pointers and the body is 12 insns short. The third
 *   guard must NOT repeat them, or its tail fails to cross-jump.
 * - The RGBA word is spelled inline in each gSPModifyVertex argument. gcc
 *   evaluates the macro's `val` after the w0 store, so the `i / 4` division's
 *   bgez lands BETWEEN the first command's two stores; that is what forces the
 *   ROM to hold the first slot's address in its own register.
 * - `dx`/`dy` are reused for the scaled deltas rather than given new temps, so
 *   the two `mul.s` write in place, and the truncations stay inline in the
 *   XYSCREEN arguments where the ROM computes them.
 * - `i = 0;` runs BEFORE the three pointer inits. Both are 21-ref allocnos and
 *   global.c:587 ranks them by live length; the ROM order needs `py` to be the
 *   shorter of the pair.
 * - `none` and the hoisted `h` exist to place the -1 constant and the
 *   D_800BA700 load at the ROM's scheduling positions.
 * - `slot2` is the last four bytes of the function. The 12 preamble commands
 *   outrun the register file, so reload spills three of the gbi macros' `_g`
 *   temporaries, and reload1.c:657 hands out stack slots in ASCENDING pseudo
 *   number. Expansion order numbers the second pipesync's `_g` below the cycle
 *   type's, which puts them at 0x3C/0x44; the ROM has them the other way round.
 *   Naming the third command's packet as a declared local gives it a low
 *   (pre-macro) pseudo number, so it takes 0x3C and the pipesync takes 0x44.
 *   Nothing else in the body changes.
 */
void emit_ball_trail_dl(Gfx** gfxp, Mtx* proj) {
  s32 screen[6];
  Gfx* gfx = *gfxp;
  /* See the header comment: this exists for its pseudo number. */
  Gfx* slot2;
  s32 mode = D_800C5AAC;
  /* Loop 2 (project): z-field cursor, source/destination byte offsets. */
  s32* zp;
  /* Main loop (emit): the three field cursors for sample i. */
  s32* px;
  s32* py;
  s32* pz;
  s32 off;
  s32 srcoff;
  s32 dstoff;
  /* Byte offset of sample i + 1. Deliberately NOT strength-reduced. */
  s32 off2;
  s32 none;
  s32 h;
  s32 idx;
  s32 i;
  f32 dy;
  f32 dx;
  f32 scale;

  if (mode == 0) {
    return;
  }
  if (D_801061CF == 0) {
    return;
  }

  if (mode == 1) {
    D_800C5AA8++;
    if (D_800C5AA8 == 0x80) {
      D_800C5AA8 = 0;
    }
    off = D_800C5AA8 * 0xC;
    *(f32*)((u8*)&D_800E3370 + off) = effect_spawn_pos[0] * (1.0f / 1024.0f);
    *(f32*)((u8*)&D_800E3374 + off) =
        effect_spawn_pos[1] * (1.0f / 1024.0f) - D_800BA700 * 0.27f;
    *(f32*)((u8*)&D_800E3378 + off) = effect_spawn_pos[2] * (1.0f / 1024.0f);
  }

  if (D_800BE69C == 1) {
    return;
  }

  none = -1;
  idx = D_800C5AA8;
  zp = &D_800E2D78;
  dstoff = 0;
  srcoff = idx * 0xC;
  do {
    if (idx == none) {
      srcoff = 0x5F4;
      idx = 0x7F;
    }
    if (project_point_to_screen(*(f32*)((u8*)&D_800E3370 + srcoff),
                                *(f32*)((u8*)&D_800E3374 + srcoff),
                                *(f32*)((u8*)&D_800E3378 + srcoff),
                                screen) < 0) {
      *(s32*)((u8*)&D_800E2D70 + dstoff) = screen[0] << 2;
      h = D_800BA700;
      *(s32*)((u8*)&D_800E2D74 + dstoff) = screen[1] << 2;
      *zp = (s32)((f32)screen[2] - *(f32*)&screen[5] * (f32)(h * 7));
    } else {
      *zp = none;
    }
    zp += 3;
    dstoff += 0xC;
    srcoff -= 0xC;
    idx--;
  } while (zp != &D_800E2D78 + 0x180);

  guMtxIdent(D_800E1E90);

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  slot2 = gfx++;
  gDPSetCycleType(slot2, G_CYC_1CYCLE);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++,
                     G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(gfx++, D_E1E90, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
  gSPVertex(gfx++, D_800C5A60, 4, 0);

  i = 0;
  py = &D_800E2D74;
  px = py - 1;
  pz = py + 1;
  off2 = 0xC;
  while (i != 0x7F) {
    if (*pz == -1) {
      goto next;
    }
    if (*(s32*)((u8*)&D_800E2D78 + off2) == -1) {
      py += 3;
      px += 3;
      pz += 3;
      off2 += 0xC;
      i++;
      continue;
    }
    dx = (f32)(*px - *(s32*)((u8*)&D_800E2D70 + off2));
    dy = (f32)(*py - *(s32*)((u8*)&D_800E2D74 + off2));
    if ((dx == 0.0f) & (dy == 0.0f)) {
      py += 3;
      px += 3;
      pz += 3;
      off2 += 0xC;
      i++;
      continue;
    }
    scale = 4.0f / sqrtf(dx * dx + dy * dy);
    dx = dx * scale;
    dy = dy * scale;

    gDPPipeSync(gfx++);
    gDPSetPrimDepth(gfx++, *pz, 0);
    gDPPipeSync(gfx++);

    gSPModifyVertex(gfx++, 0, G_MWO_POINT_RGBA, (0x40 - i / 4) | 0xFFFFFF00);
    gSPModifyVertex(gfx++, 1, G_MWO_POINT_RGBA, (0x40 - i / 4) | 0xFFFFFF00);
    gSPModifyVertex(gfx++, 2, G_MWO_POINT_RGBA, (0x40 - i / 4) | 0xFFFFFF00);
    gSPModifyVertex(gfx++, 3, G_MWO_POINT_RGBA, (0x40 - i / 4) | 0xFFFFFF00);
    gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
                    ((*px + (s32)dy) << 16) + (*py - (s32)dx));
    gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
                    ((*px - (s32)dy) << 16) + (*py + (s32)dx));
    gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
                    ((*(s32*)((u8*)&D_800E2D70 + off2) + (s32)dy) << 16) +
                        (*(s32*)((u8*)&D_800E2D74 + off2) - (s32)dx));
    gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
                    ((*(s32*)((u8*)&D_800E2D70 + off2) - (s32)dy) << 16) +
                        (*(s32*)((u8*)&D_800E2D74 + off2) + (s32)dx));
    gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
  next:
    py += 3;
    px += 3;
    pz += 3;
    off2 += 0xC;
    i++;
  }

  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&proj[7]),
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);

  *gfxp = gfx;
}

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

extern f32 per_view_camera_state;
extern f32 D_801B54F0;
extern f32 D_801B54F4;
extern f32 D_801B54F8;
extern f32 D_801B54FC;
extern f32 D_801B5500;
extern f32 wind_dir_angle_rad;
extern Mtx D_800E1ED0[];
/* Same matrix bank as D_800E1ED0, addressed through segment 0 for the RSP. */
extern Mtx D_E1ED0[];
/* The single light this emitter installs. Its ambient half sits eight bytes
 * below, in the tail of the adjacent data label, which is why the ambient is
 * spelled as a negative displacement off this symbol; see the header comment.
 */
extern Light D_800C5AC0;
/* Spelled as arrays so the MEMs carry MEM_IN_STRUCT_P; see the header comment.
 */
extern s8 D_800FC861[];
extern s32 D_800B67F0;
extern s32 D_8012CFE4[];
extern Gfx D_80222400[];
/* This TU's double pool, shared with the still-asm siblings. Referenced rather
 * than spelled as literals; see the header comment. */
extern const f64 D_800D1B30; /* pi */
extern const f64 D_800D1B38; /* 0.3f widened */
extern const f64 D_800D1B40; /* 1 / (2 * pi) */
extern const f64 D_800D1B48; /* 180.0 */
extern const f64 D_800D1B50; /* pi / 2 */
extern const f64 D_800D1B58; /* 3 * pi / 2 */
extern const f64 D_800D1B60; /* pi / 2, the second window's own entry */
extern const f64 D_800D1B68; /* 3 * pi / 2, likewise */

extern f32 sqrtf(f32);
extern double atan2(double, double);
extern void func_80065A1C(f32 mf[4][4], f32* rot, f32* pos);
extern void func_80084468(Gfx** gfxp, f32 x, f32 y);

/**
 * Emits the 3D wind indicator: a mesh yawed to face the camera, counter-rotated
 * by the current wind heading, drawn 300 units in front of the viewer.
 *
 * The model yaw is the camera's own heading, taken as atan2 of the
 * eye-to-target delta (vertical delta over the horizontal distance), plus pi +
 * 0.3 rad. That matrix is concatenated with a heading of (180 degrees - the
 * wind direction in degrees) and scaled by -0.07, then packed into the
 * double-buffered sky matrix bank at sky_panel_bank_index and referenced from
 * the display list through the segment-0 alias of the same bank.
 *
 * func_80084468 draws the companion readout at (x, y). It is emitted BEFORE the
 * mesh while the wind points into the near half of the circle (heading outside
 * pi/2..3pi/2) and AFTER it otherwise: a two-bucket painter's-order sort so the
 * readout never disappears behind the mesh.
 *
 * Four notes on the shape of this body:
 * - `wind` exists so the address of wind_dir_angle_rad is formed in a pseudo;
 *   cse then parks it in $s2 across the five matrix calls. Reading the global
 *   directly for all three uses costs the callee-saved register and shrinks the
 *   frame by 8.
 * - D_800FC861 and D_8012CFE4 are spelled as arrays. As plain scalar globals
 *   their MEMs have MEM_IN_STRUCT_P == 0 with a non-varying address, which
 *   satisfies the "not in a struct and does not vary" arm of sched.c:834
 *   true_dependence and lets the scheduler hoist the `lb` above the light
 *   stores and sink the D_8012CFE4 store past the two G_DL words. The array
 *   spelling sets MEM_IN_STRUCT_P, restores the dependence, and pins both.
 * - The first wind window is written as two negated tests rather than
 *   `<= || >=`, because the ROM compares with c.lt.d in both windows; the
 *   `<=` spelling emits c.le.d.
 * - The light block is the three sub-macros of gSPSetLights1 rather than the
 *   composite. The composite needs a Lights1 symbol at 0x800C5AB8, whose
 *   ambient half lies inside the preceding data label and whose light half is
 *   D_800C5AC0, so declaring it would split one data label and overlap another.
 *   Both spellings were measured byte-exact; this one adds no symbol.
 */
void emit_wind_indicator_dl(Gfx** gfxp, f32 x, f32 y) {
  f32 look[3];
  f32 rot[3];
  f32 pos[3];
  f32 unused[3];
  f32 model[4][4];
  f32 spin[4][4];
  Gfx* gfx = *gfxp;
  f32* wind = &wind_dir_angle_rad;

  look[0] = D_801B54F8 - per_view_camera_state;
  look[1] = D_801B54FC - D_801B54F0;
  look[2] = D_801B5500 - D_801B54F4;

  rot[0] = atan2(look[1], sqrtf(look[0] * look[0] + look[2] * look[2])) +
           D_800D1B30 + D_800D1B38;
  rot[1] = 0.0f;
  rot[2] = 0.0f;
  pos[0] = x;
  pos[1] = y;
  pos[2] = -300.0f;
  func_80065A1C(model, rot, pos);

  guRotateRPYF(spin, 0.0f, D_800D1B48 - *wind * 360.0f * D_800D1B40, 0.0f);
  guMtxCatF(spin, model, model);
  guScaleF(spin, -0.07f, -0.07f, -0.07f);
  guMtxCatF(spin, model, model);
  guMtxF2L(model, &D_800E1ED0[sky_panel_bank_index]);

  if (!(*wind > D_800D1B50) || !(*wind < D_800D1B58)) {
    func_80084468(&gfx, x, y);
  }

  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, &D_E1ED0[sky_panel_bank_index],
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPNumLights(gfx++, NUMLIGHTS_1);
  gSPLight(gfx++, &D_800C5AC0, 1);
  gSPLight(gfx++, (u8*)&D_800C5AC0 - 8, 2);

  if (D_800FC861[0] != 0) {
    D_8012CFE4[0] = D_800B67F0;
    gSPDisplayList(gfx++, D_80222400);
  }

  if (wind_dir_angle_rad > D_800D1B60 && wind_dir_angle_rad < D_800D1B68) {
    func_80084468(&gfx, x, y);
  }

  *gfxp = gfx;
}

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

/* Club/wind head-up overlay: the projection reset, the two wind arrows, the
 * club-face swatch and the two-panel power meter, all in one chain.
 */
extern s8 D_800FC86C;
extern s8 D_800FC871;
extern s8 D_801061BF;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern u16 D_80106238;
extern u16 D_8010623A;
extern u8 D_80106241;
extern u8 D_80106243;
extern u16 D_800C5AD4[];
extern u8 D_80106240[];
extern s32 D_800C54C8[];
extern s32 D_800C5474;
extern u32 D_800FBE70;
extern s16 D_801B7222[][92];
extern s32 active_player_idx;
extern s32 active_club_id;
extern f32 D_8022D67C;
extern f32 D_8022D680;
extern f32 D_8022D684;
extern f32 D_8022D688;
/* The two quads the meter is drawn on, and their fixed-point model matrices in
 * both CPU and segmented form. */
extern Vtx D_800C5868[];
extern Vtx D_800C58A8[];
extern Mtx D_800E1F50[];
extern Mtx D_800E1FD0[];
extern Mtx D_E1F50[];
extern Mtx D_E1FD0[];
extern const f64 D_800D1B78; /* pi */
extern const f64 D_800D1B80; /* 2 * pi */
extern const f64 D_800D1B88; /* pi, the second arrow's own entry */
extern const f64 D_800D1B90; /* 2 * pi, likewise */

/* Only the asset id at 0x2E is read out of the club record here. */
typedef struct {
  /* 0x00 */ u8 pad00[0x2E];
  /* 0x2E */ u16 asset_id;
} ClubAsset;

extern ClubAsset* get_club_param(u32 id);
extern void convert_and_pack_floats_to_fixed(f32 mf[4][4], Mtx* m);
extern void func_802199EC(Gfx** gfxp, s32 x, s32 y, s32 arg3);
extern void func_8021FC20(Gfx** gfxp);

void func_8008534C(Gfx** gfxp) {
  f32 rot[3];
  f32 pos[3];
  f32 unusedA[4];
  f32 model[4][4];
  f32 unusedB[18];
  Gfx* gfx;
  ClubAsset* club;

  gfx = *gfxp;

  gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK);

  /* `D_80106240[2]`, not a scalar `D_80106242`: MEM_IN_STRUCT_P makes this
   * `lbu` may-alias the ten preamble packets above (sched.c:817 true_dependence
   * disambiguates a non-struct fixed-address read from a struct varying-address
   * store), which is what sinks the ten dead `gfx` writebacks below the twenty
   * packet words.  Same idiom, same reason, as `draw_animated_status_icon`.
   */
  if (D_80106240[2] == 1 || D_80106243 == 1) {
    if (D_800FC86C != 0) {
      f32* arrow = &D_8022D684;

      emit_wind_indicator_dl(&gfx, *arrow, D_8022D688);
      if (D_80106243 == 1) {
        func_802199EC(&gfx, (s32)(*arrow + 160.0f), (s32)(120.0f - D_8022D688),
                      0x40);
      }
    }
  } else if (wind_magnitude != 0 && D_801061BF != 0) {
    if (get_table_entry(D_800FBE70)->id != 11 && D_800BE69C == 0) {
      emit_wind_indicator_dl(&gfx, (f32)D_800C54C8[0] + 100.0f, 76.0f);
    }
  }

  gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gDPPipeSync(gfx++);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);

  if (D_800BE69C == 0 && D_800FC871 != 0) {
    func_8021FC20(&gfx);
  }

  if (D_801061C2 != 0 && D_800BE69C == 0) {
    RomLoadSlot buf[2];
    f32 angle;

    club = get_club_param(D_801B7222[active_player_idx][active_club_id]);
    if (club->asset_id != D_800C59D8) {
      D_800C59D8 = club->asset_id;
      func_8005062C(club->asset_id, buf);
      func_800506D4(D_800E2140, buf);
      D_800C5474 ^= 1;
      func_8005062C(D_800C5AD4[active_club_id], buf);
      func_800506D4(D_800E2144[D_800C5474], buf);
    }

    angle = D_80106238 * 9.58738019e-05f;
    if (angle > D_800D1B78) {
      angle = angle - D_800D1B80;
    }

    rot[0] = 0.0f;
    rot[1] = 0.0f;
    rot[2] = -angle * 0.5f;
    pos[0] = -128.0f - (f32)D_800C54C8[0];
    pos[1] = -76.0f - (f32)D_800C5AD0;
    pos[2] = -300.0f;
    func_80065A1C(model, rot, pos);
    convert_and_pack_floats_to_fixed(model, &D_800E1F50[sky_panel_bank_index]);

    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    if (D_80106240[0] & 2) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, 255);
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPLoadTextureBlock(gfx++, ((u32)D_800E2140 + 8) & ~7, G_IM_FMT_RGBA,
                        G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                        G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, &D_E1F50[sky_panel_bank_index],
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, D_800C5868, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
  }

  if ((D_801061C1 != 0 || D_80106241 != 0) && D_800BE69C == 0) {
    f32 angle;

    angle = D_8010623A * 9.58738019e-05f;
    if (angle > D_800D1B88) {
      angle = angle - D_800D1B90;
    }

    rot[0] = 0.0f;
    rot[1] = 0.0f;
    rot[2] = angle * 0.5f;

    if (D_801061C1 != 0) {
      pos[0] = (f32)(D_800C54C8[0] + 112);
      pos[1] = -70.0f - (f32)D_800C5AD0;
      pos[2] = -300.0f;
    } else if (D_800FC86C != 0) {
      pos[0] = D_8022D67C;
      pos[1] = D_8022D680;
      pos[2] = -300.0f;
      func_802199EC(&gfx, (s32)(D_8022D67C + 160.0f),
                    (s32)(120.0f - D_8022D680), 0x40);
    }

    func_80065A1C(model, rot, pos);
    convert_and_pack_floats_to_fixed(model, &D_800E1FD0[sky_panel_bank_index]);

    if (D_80106240[0] & 8) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, 255);
    }

    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_800E2138 + 8) & ~7);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gSPMatrix(gfx++, &D_E1FD0[sky_panel_bank_index],
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, D_800C58A8, 8, 0);
    gDPLoadTextureBlock(gfx++, ((u32)D_800E2138 + 8) & ~7, G_IM_FMT_RGBA,
                        G_IM_SIZ_16b, 48, 24, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                        G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPLoadTextureBlock(gfx++, ((u32)D_800E2138 + 0x908) & ~7, G_IM_FMT_RGBA,
                        G_IM_SIZ_16b, 48, 24, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                        G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 4, 5, 6, 0, 4, 6, 7, 0);
    gDPPipeSync(gfx++);
  }

  *gfxp = gfx;
}

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

/* The rotate / translate / scale triples the indicator's matrix is built from.
 * Spelled as nine symbols, not three arrays: the ROM materializes a %hi/%lo per
 * field. */
extern f32 D_800E2110;
extern f32 D_800E2114;
extern f32 D_800E2118;
extern f32 D_800E211C;
extern f32 D_800E2120;
extern f32 D_800E2124;
extern f32 D_800E2128;
extern f32 D_800E212C;
extern f32 D_800E2130;
extern Mtx D_800E2090[];
extern Mtx D_E2090[];
/* The 32-vertex arrow strip and the 16-vertex textured quad pair. */
extern Vtx D_800C5AF0[];
extern Vtx D_800C5BF0[];
/* Frame counter, sound-slot cursor, and the double-buffer toggle of the
 * streamed texture. */
extern s32 D_800C5E10;
extern s32 D_800C5E14;
extern s32 D_800C5E18;
extern u16 D_800C5DFA[];
/* Base of the streamed texture heap. */
extern u8* D_8012D3C0;
extern const f64 D_800D1B98; /* pi */
extern const f64 D_800D1BA0; /* pi / 180 */
extern f32 sinf(f32);

/**
 * Emits the off-screen ball indicator: a marker drawn at the projected ball
 * position once the ball has left the view.
 *
 * `D_800C5DF0` is the effect's phase, advanced 0.2 per call and retired by
 * setting it to -1 once it passes pi; a negative phase means the effect is off.
 * The ball's world position is projected, and only a rejected projection (a
 * negative return, i.e. off screen) draws anything.
 *
 * The course record's classifier (`func_80213C78`) picks between the two
 * appearances. Kinds other than 10 and 2 get the untextured arrow strip: a
 * shade-only combine, a phase-driven z rotation, and 32 vertices walked as four
 * groups of three two-triangle commands. Kinds 10 and 2 get the textured pair
 * instead, with a random rotation, a per-frame streamed 48x128 4-bit texture
 * pulled from the rom every fourth call, and a prim colour that is random for
 * kind 10 and fixed otherwise.
 */
void emit_ball_offscreen_indicator(Gfx** gfxp) {
  f32 mf[4][4];
  s32 screen[6];
  RomLoadSlot buf[2];
  Gfx* gfx;
  f32* rot;
  f32* pos;
  f32* sc;
  s32 kind;
  s32 i;
  f32 scale;

  gfx = *gfxp;

  if (D_800C5DF0 < 0.0f) {
    return;
  }
  D_800C5DF0 = D_800C5DF0 + 0.2f;
  if (D_800C5DF0 > D_800D1B98) {
    D_800C5DF0 = -1.0f;
    return;
  }

  if (project_point_to_screen(effect_spawn_pos[0] * (1.0f / 1024.0f),
                              effect_spawn_pos[1] * (1.0f / 1024.0f),
                              effect_spawn_pos[2] * (1.0f / 1024.0f),
                              screen) >= 0) {
    return;
  }

  kind = func_80213C78(&D_801B71D0[D_8010623F * 0xB8]);
  if ((kind != 0xA) & (kind != 2)) {
    scale = sinf(D_800C5DF0) * 0.5f;
    D_800E2128 = 0.0f;
    D_800E212C = 0.0f;
    D_800E2130 = (D_800C5DF0 + 0.7f) * 0.5f;
  } else {
    scale = sinf(D_800C5DF0) * 0.4f;
    D_800E2128 = 0.0f;
    D_800E212C = 0.0f;
    D_800E2130 = (f32)((f64)(guRandom() % 360) * D_800D1BA0);
  }

  rot = &D_800E2128;
  pos = &D_800E2110;
  sc = &D_800E211C;
  sc[0] = scale;
  D_800E2120 = scale;
  D_800E2124 = scale;
  pos[0] = (f32)(screen[0] - 0xA0);
  D_800E2114 = (f32)(-screen[1] + 0x78);
  D_800E2118 = -100.0f;
  mtx_from_rts(mf, rot, pos, sc);
  convert_and_pack_floats_to_fixed(mf, &D_800E2090[sky_panel_bank_index]);

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetBlendColor(gfx++, 255, 255, 255, 1);

  if ((kind != 0xA) & (kind != 2)) {
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_OFF);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 255, 255);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    if (D_800C5DF4 == 0) {
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    } else {
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &D_E2090[sky_panel_bank_index],
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, D_800C5BF0, 32, 0);
    i = 0;
    do {
      gSP2Triangles(gfx++, i * 8, i * 8 + 1, i * 8 + 2, 0, i * 8, i * 8 + 2,
                    i * 8 + 3, 0);
      gSP2Triangles(gfx++, i * 8 + 1, i * 8 + 4, i * 8 + 5, 0, i * 8 + 1,
                    i * 8 + 5, i * 8 + 2, 0);
      gSP2Triangles(gfx++, i * 8 + 4, i * 8 + 6, i * 8 + 7, 0, i * 8 + 4,
                    i * 8 + 7, i * 8 + 5, 0);
      i++;
    } while (i != 4);
  } else {
    D_800C5E14++;
    if (D_800C5E14 == 4) {
      D_800C5E10 = (D_800C5E10 + 1) % 6;
      D_800C5E14 = 0;
      func_8005062C(D_800C5DFA[D_800C5E10 * 2], buf);
      func_800506D4(D_8012D3C0 + (D_800C5E18 * 0xC08 + 0x970), buf);
      D_800C5E18 ^= 1;
    }

    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);

    if (kind == 0xA) {
      gDPSetEnvColor(gfx++, 255, 255, 127, 255);
      gDPSetPrimColor(gfx++, 0, 0, guRandom() % 255, guRandom() % 255,
                      guRandom() % 255, 191);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, 191, 0, 191, 191);
      gDPSetEnvColor(gfx++, 255, 255, 255, 255);
    }

    gDPSetCombineLERP(gfx++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE,
                      PRIMITIVE, 0, TEXEL0, 0, ENVIRONMENT, PRIMITIVE, TEXEL0,
                      PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
    gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &D_E2090[sky_panel_bank_index],
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, D_800C5AF0, 16, 0);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_8012D3C0 + 0x978) & ~7, G_IM_FMT_I,
                           48, 128, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    i = 0;
    do {
      gSP2Triangles(gfx++, i * 4, i * 4 + 1, i * 4 + 2, 0, i * 4, i * 4 + 2,
                    i * 4 + 3, 0);
      i++;
    } while (i != 2);

    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_8012D3C0 + 0x1580) & ~7, G_IM_FMT_I,
                           48, 128, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    i = 0;
    do {
      gSP2Triangles(gfx++, i * 4, i * 4 + 1, i * 4 + 2, 0, i * 4, i * 4 + 2,
                    i * 4 + 3, 0);
      i++;
    } while (i != 2);
    gDPPipeSync(gfx++);
  }

  gDPPipeSync(gfx++);
  *gfxp = gfx;
}

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

/* func_80087CB0: CARRY (S257, near-match, body 100% RE'd, 1 instruction long:
 * 253 vs 252). Aim-cursor sprite emitter on the global `glistp`:
 * gDPPipeSync + gDPLoadTLUT_pal16(0, (D_800E2184+8)&~7), then the animation
 * frame `D_800C5E24 = (D_800C5E24 + 1) % 11` and
 * `D_800E2134 = (D_80106240[0] & 8) ? D_800C5E24 << 7 : 0x580`, then
 * gDPLoadTextureBlock_4b((D_800E2134+0x28+D_800E2184)&~7, G_IM_FMT_CI, 16, 16)
 * + gDPPipeSync + a gSPScisTextureRectangle.
 *
 * KEY IDENTIFICATION (S257): the branchless coordinate clamp
 * (`sll 18; sra 16; nor; sra 31; and; andi 0xffc`) and the asymmetric s/t
 * clip math (`bgezl` on the s16 x but `bgez` on the s32 y, then
 * `slti 1; negu; and; negu`) are NOT hand-written clamps -- they are the
 * verbatim expansion of the SDK macro `gSPScisTextureRectangle`
 * (gbi.h:4498, "like gSPTextureRectangle but accepts negative position
 * arguments"): MAX((s16)xh,0) for the corners and
 * `(s) - ((s16)xl < 0 ? MIN(((s16)xl*(s16)dsdx)>>7, 0) : 0)` for s, with the
 * y branch testing the UNCAST `(yl) < 0`. dsdx = dtdy = 1<<10 gives the
 * `>>7` == `*8`. Reconstructing the clamps by hand never converges; use the
 * macro.
 *
 * Three other levers landed here and are reusable:
 *  - the `D_800E2134` assignment must be an if/else, NOT a ternary: two
 *    stores cross-jump-merge into one after the join, which resets cse's
 *    table so both `glistp` and `D_800E2134` are RE-LOADED for the second DL
 *    block (a ternary keeps the value in a pseudo and folds block2's stores
 *    onto block1's base register).
 *  - `D_80106240` must be read as an ARRAY element (`extern u8 D_80106240[]`
 *    + `D_80106240[0]`): MEM_IN_STRUCT_P makes the load may-alias the
 *    varying-address DL stores so sched1 cannot hoist it above them.
 *  - the DL-block address wants `(D_800E2134 + 0x28 + (u32)D_800E2184)`
 *    grouping, and the rect coords want `(GLOBAL + CONST) + ARRAY[0]`.
 *
 * S258 UPDATE: the fold reassociation is now SOLVED and the build is at the
 * EXACT instruction count (252), asm-differ 1580 -> 345. gcc-2.7.2's
 * `fold-const.c:3685 associate` decides which operand of a 3-term sum carries
 * the constant by WHICH SIDE it is parenthesised on: the arg0 split (:3722)
 * rewrites `(GLOBAL + C) + elem` to `GLOBAL + (elem + C)` (wrong), while the
 * arg1 split (:3759) rewrites `GLOBAL + (elem + C)` to `(GLOBAL + C) + elem`
 * (the ROM form). So the y coords are spelled
 * `D_800C54C4 + (D_800E21A8[0] + 0xAE) + D_800C5AD0`, and the x coords use a
 * local base temp (`s32 xl = D_800C54C8[0] + 0x108;` then `elem + xl`), which
 * is immune to reassociation AND yields the ROM's shared destructive
 * `addiu t0,t0,0x108`. A temp for the Y base is WRONG: the ROM re-loads the
 * three y globals in the post-`bgezl` t-clip block, so a y temp comes out 3
 * instrs short (249).
 * RESIDUAL: pure register permutation (`D_800C54C4` in a0 vs the ROM's a2,
 * with the clamp scratch swapped) plus one sched1 LUID tie -- the ROM loads
 * `D_800E2190[0]` BEFORE `D_800C54C8`, but the temp initialiser must precede
 * the macro call so my build emits them the other way round. Permuter run at
 * S258. Full reconstruction + measured variants in
 * docs/wip/func_80087CB0.near-match.md.
 */
extern s32 D_800C5E24;
extern u8 D_80106240[];
extern s32 D_800E2134;
extern s32 D_800C54C4;
extern s32 D_800C54C8[];

INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80087CB0);

extern s32 D_800C308C;
extern s32 D_800C5E48;
extern s32 D_800C5E4C;
extern s32 D_800C5E60;
extern f32 D_800C5E64;
extern f32 D_800C5E68;
/* Backing table is an s32[8] of resource ids at 0x800C5E28
 * ({0x648, 0x649, 0x64A, 0x64B, 0x64C, 0x64B, 0x64A, 0x649}, a ping-ponged
 * 5-frame loop). Only the low halfword of each entry is read, and the split
 * data label starts at the +2 offset, so it is spelled as a u16 array with a
 * doubled index. */
extern u16 D_800C5E2A[];

/**
 * Slides an animated status icon in toward (96, 24) and emits it, plus a
 * mode-dependent caption strip, through the caller's display-list cursor.
 *
 * The position pair eases 20% of the remaining distance per call, or snaps to
 * the top-left corner when the caller passes mode 2. A 4-call tick advances an
 * 8-step animation cursor; on each advance the next frame's resource is fetched
 * and unpacked into the icon buffer.
 *
 * Two sprites are drawn from the eased position: a 32x32 CI8 icon with its own
 * 256-entry palette, and a 64x16 CI4 caption strip (three variants selected by
 * the game mode) with a 16-entry palette, offset 24 pixels to the right so it
 * overlaps the icon's tail.
 */
void draw_animated_status_icon(Gfx** gfxp, s32 mode) {
  RomLoadSlot slot[2];
  Gfx* gfx = *gfxp;
  s32 caption;

  if (D_800C59E0 == 2) {
    if (mode == 2) {
      D_800C5E64 = 0.0f;
      D_800C5E68 = 0.0f;
    } else {
      D_800C5E64 += (96.0f - D_800C5E64) * 0.2f;
      D_800C5E68 += (24.0f - D_800C5E68) * 0.2f;
    }

    D_800C5E48++;
    if (D_800C5E48 == 4) {
      D_800C5E48 = 0;
      D_800C5E4C = (D_800C5E4C + 1) % 8;
      func_8005062C(D_800C5E2A[D_800C5E4C * 2], slot);
      func_800506D4(D_800E2158[D_800C5E60], slot);
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_800E2158[D_800C5E60] + 8) & ~7);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTextureBlock(gfx++, ((u32)D_800E2158[D_800C5E60] + 0x208) & ~7,
                        G_IM_FMT_CI, G_IM_SIZ_8b, 32, 32, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (s32)D_800C5E64 * 4, (s32)D_800C5E68 * 4,
                            ((s32)D_800C5E64 + 0x20) * 4,
                            ((s32)D_800C5E68 + 0x20) * 4, G_TX_RENDERTILE, 0, 0,
                            1 << 10, 1 << 10);
    gDPPipeSync(gfx++);

    if (D_800C308C == 1) {
      caption = 1;
    } else {
      caption = (D_800C308C != 2) ? 2 : 0;
    }

    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_800E214C[caption] + 8) & ~7);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_800E214C[caption] + 0x28) & ~7,
                           G_IM_FMT_CI, 64, 16, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, ((s32)D_800C5E64 + 0x18) * 4,
                            (s32)D_800C5E68 * 4, ((s32)D_800C5E64 + 0x58) * 4,
                            ((s32)D_800C5E68 + 0x10) * 4, G_TX_RENDERTILE, 0, 0,
                            1 << 10, 1 << 10);

    *gfxp = gfx;
  }
}

extern s8 D_801061BD;

s32 draw_letterbox_bars(Gfx** dl_ptr, s32 y) {
  Gfx* dl = *dl_ptr;

  if (D_801061BD != 0) {
    if (D_800C308C != 0 && D_800C308C != 5) {
      gDPPipeSync(dl++);
      gDPPipeSync(dl++);
      /* The ROM's rendermode word is 0x00504A40 = RM_XLU_SURF with ZMODE_XLU;
       * every SDK gbi.h on hand defines RM_XLU_SURF with ZMODE_OPA, so the
       * zmode is spelled out here. */
      gDPSetRenderMode(dl++, G_RM_XLU_SURF | ZMODE_XLU,
                       G_RM_XLU_SURF2 | ZMODE_XLU);
      gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
      gDPSetPrimColor(dl++, 0, 0, 0, 0, 0, 255);
      gDPPipeSync(dl++);
      if (y > 0) {
        gSPScisTextureRectangle(dl++, 8 << 2, 8 << 2, 312 << 2, (y + 8) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gSPScisTextureRectangle(dl++, 8 << 2, (232 - y) << 2, 312 << 2,
                                232 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                                1 << 10);
      }
      gDPPipeSync(dl++);
      y += 2;
      if (y > 16) {
        y = 16;
      }
    } else {
      y = 0;
    }
  }
  *dl_ptr = dl;
  return y;
}

/* func_80088A90: CARRY (S254, re-cracked S258 to the EXACT instruction count).
 * Glyph/units-string DL emitter: renders a measurement string as one
 * gSPTextureRectangle per glyph ('f'->s 0x60 w0x10, '.'->0x70 w4, 'm'->0x50 w8,
 * 'y'->0x58 w8, digit->(c-0x30)<<3 w8, ' '-> advance only), rect x..x+w by
 * row..row+0xC in quarter-pixels, on a caller-supplied Gfx** cursor.
 *
 * The S254 "terminal, not source-leverable" verdict is REFUTED. Writing the
 * 6-word block as gSPTextureRectangle(dl++, ...) instead of raw u32 stores
 * makes the emit block byte-identical (including the -4(a2)/0(a2) store pair
 * and the loop.c preheader hoist of word_hi/word_lo, the 3 constant command
 * words, and the 5 character literals -- the constant hoist is the only reason
 * this leaf has a frame at all). The two features S254 called unreachable both
 * fall out:
 *  - the redundant `andi v1,t0,0xFF` needs the char SPLIT into a `u8` for the
 *    `!= 0` loop test and an `s32` for the compares (`ch = c;` at the loop
 * top): the zero-extend then sits in a different BB from the `lbu` so combine
 *    cannot fold it, and the s32 keeps the range test signed (`slti`; a plain
 *    `u8 ch` gives `sltiu` and 86 instrs).
 *  - the `beql`+annulled `str++` on the space case needs the space handler
 *    OUT OF LINE (`goto skip_char;` with the label down among the glyph stubs);
 *    as an inline if-body gcc emits `bne`+`nop`+`j`+`addiu` instead.
 * RESIDUAL: one extra callee-saved reg. ROM keeps the 5 hoisted char constants
 * in s1/s0/t9/t8/t7 and REUSES the dead raw-char reg t0 for the glyph value;
 * my build gives the glyph its own t1, so a constant spills to s2 (3 saved
 * regs, bigger frame) and the whole loop renames. #local-alloc-qty-permutation,
 * no longer a reorg/branch-likely wall. asm-differ 913 at 83/83 instrs. Full
 * reconstruction + measured variants in docs/wip/func_80088A90.near-match.md.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_80080220", func_80088A90);

extern s32 D_800C5E6C[2][3];
extern s32 D_800C5E84;
extern s32 D_800C5E88;
extern s32 D_800C5E8C;
extern s32 D_800C5E90;
extern s32 D_800C5E94;
extern u16 D_800FBDCE;
extern u16 D_800FBDD6;
extern char D_800D1BB8[];
extern char D_800D1BC8[];
extern char D_800D1BCC[];
extern char D_800D1BDC[];
extern void emit_glyph_sheet_preamble_dl(Gfx** chain, s32 arg1);
extern void func_800747B0(Gfx** chain, s16 x, s16 y, char* str);

void func_80088BDC(Gfx** chain) {
  Gfx* dl = *chain;

  emit_glyph_sheet_preamble_dl(&dl, D_800C5E94);
  gDPSetPrimColor(dl++, 0, 0, D_800C5E6C[0][0], D_800C5E6C[0][1],
                  D_800C5E6C[0][2], 255);
  gDPSetEnvColor(dl++, D_800C5E6C[1][0], D_800C5E6C[1][1], D_800C5E6C[1][2],
                 255);
  gDPPipeSync(dl++);
  func_800747B0(&dl, D_800C5E84, D_800C5E88, D_800D1BB8);
  gDPPipeSync(dl++);
  check_and_print_grid(D_800D1BC8, D_800C5E8C * 5 + 0x13, D_800C5E90 + 5);

  if (D_800FBDD6 & 8) {
    D_800C5E90 ^= 1;
  }
  if (D_800FBDD6 & 4) {
    D_800C5E90 ^= 1;
  }
  if (D_800FBDD6 & 2) {
    D_800C5E8C = (D_800C5E8C + 2) % 3;
  }
  if (D_800FBDD6 & 1) {
    D_800C5E8C = (D_800C5E8C + 1) % 3;
  }

  if (D_800FBDCE & 0x8000) {
    D_800C5E6C[D_800C5E90][D_800C5E8C] += 4;
  }
  if (D_800FBDCE & 0x4000) {
    D_800C5E6C[D_800C5E90][D_800C5E8C] -= 4;
  }
  if (debug_menu_pad_buttons_c & 0x10) {
    D_800C5E94 ^= 1;
  }
  if (D_800FBDCE & 0x800) {
    D_800C5E88 -= 4;
  }
  if (D_800FBDCE & 0x400) {
    D_800C5E88 += 4;
  }
  if (D_800FBDCE & 0x200) {
    D_800C5E84 -= 4;
  }
  if (D_800FBDCE & 0x100) {
    D_800C5E84 += 4;
  }

  if (D_800C5E6C[D_800C5E90][D_800C5E8C] > 0xFF) {
    D_800C5E6C[D_800C5E90][D_800C5E8C] = 0xFF;
  }
  if (D_800C5E6C[D_800C5E90][D_800C5E8C] < 0) {
    D_800C5E6C[D_800C5E90][D_800C5E8C] = 0;
  }

  sprintf(D_80105118, D_800D1BCC, D_800C5E6C[0][0], D_800C5E6C[0][1],
          D_800C5E6C[0][2]);
  check_and_print_grid(D_80105118, 0x14, 5);
  sprintf(D_80105118, D_800D1BCC, D_800C5E6C[1][0], D_800C5E6C[1][1],
          D_800C5E6C[1][2]);
  check_and_print_grid(D_80105118, 0x14, 6);
  check_and_print_grid(D_800D1BDC, D_800C5E8C * 5 + 0x13, D_800C5E90 + 5);

  gDPPipeSync(dl++);
  gDPPipeSync(dl++);
  gDPSetCycleType(dl++, G_CYC_1CYCLE);
  *chain = dl;
}

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
extern void emit_glyph_sheet_preamble_dl(Gfx** chain, s32 arg1);
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
        emit_glyph_sheet_preamble_dl(&dl, 0);
        sprintf(D_80105118, D_800D1C84, mins, secs, centi);
        func_8007624C(&dl, 0xA8, 0xB0, D_80105118, 0xFFFF0000, 0x30FF0000,
                      0x80);
      } else {
        emit_glyph_sheet_preamble_dl(&dl, 0);
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
