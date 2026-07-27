#include "common.h"

extern s32 D_800C4010;
extern s32 D_800C4014;
extern s32 D_800C4018;
extern f32 D_800C401C;
extern s32 D_800C4020;
extern u8 D_800C4026[];
extern s32 D_800C4060;
extern s32 D_800C4064;
extern s32 D_800C4068;
extern s32 D_800C0E60;
extern u8 D_800C406C[];
extern s32 D_800C411C;
extern s32 D_800C4120;
extern s32 D_800C4124;
extern s32 D_800C4128;
extern s32 D_800C4144;
extern void play_sound_effect(s32 sfx, s32 arg1, s32 arg2);
extern s32 D_800FF4B0;
extern s32 D_800FF4B4;
extern s32 D_800FF4B8;
extern s32 D_800FF4D0;
extern s32 D_800FF4D4;
extern s32 D_800FF4D8;
extern s32 D_800FF4DC;
extern s32 D_800FF4E0;
extern s32 D_800FF4E4;
extern s32 D_800FF4E8;
extern s32 scenario_mode_id;
extern u8* func_8005AF50(void);
extern void* D_800E1C00;
extern void* D_800E1C04;
extern void* D_800E1C08;
extern void* D_800E1C0C;

extern void* heap3_alloc(u32 need);
extern void heap3_free(void** payload_ptr);
extern void func_80050DA0(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern u32 func_8005062C(u16 index, void* out);
extern void func_800506D4(void* data, void* slot);
void func_8006B54C(void);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A2C0);

void func_8006A4A0(Gfx** pgfx, u32 pal, u32 addr) {
  Gfx* gfx = *pgfx;

  gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, addr & ~7);
  gDPTileSync(gfx++);
  gDPSetTile(gfx++, 0, 0, 0, 256 | ((pal & 0xF) << 4), G_TX_LOADTILE, 0, 0, 0,
             0, 0, 0, 0);
  gDPLoadSync(gfx++);
  gDPLoadTLUTCmd(gfx++, G_TX_LOADTILE, 15);
  gDPPipeSync(gfx++);
  *pgfx = gfx;
}

void func_8006A548(Gfx** pgfx, u32 arg1) {
  Gfx* gfx = *pgfx;

  gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, arg1 & ~7);
  gDPTileSync(gfx++);
  gDPSetTile(gfx++, 0, 0, 0, 256, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
  gDPLoadSync(gfx++);
  gDPLoadTLUTCmd(gfx++, G_TX_LOADTILE, 255);
  gDPPipeSync(gfx++);
  *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A5E4);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006A84C);

void func_8006ACD8(void) {
  u8 sp10[0x20];

  D_800E1C04 = heap3_alloc(func_8005062C(0x63F, sp10));
  func_800506D4(D_800E1C04, sp10);
  func_8006B54C();
}

void func_8006AD1C(s32 arg0) {
  u8 sp10[0x20];

  func_8005062C(*(u16*)&D_800C4026[(arg0 % 14) * 4], sp10);
  func_800506D4(D_800E1C04, sp10);
}

void func_8006AD88(void) { D_800C4020 = 0; }

s32 func_8006AD94(void) { return D_800C4020; }

s32 func_8006ADA0(void) {
  if (D_800C4010 == 0) {
    return (s32)D_800C401C;
  }
  if (D_800C4014 == 1) {
    return 0;
  }
  if (D_800C4014 == 3) {
    D_800C4010 = 0;
    return 20;
  }
  return 0;
}

void func_8006ADF8(s32 arg) {
  if (D_800C4010 == 1) {
    if ((u32)(arg - 2) < 2) {
      D_800C4014 = 2;
      D_800C4018 = -1;
    }
  } else {
    D_800C4020 = arg;
    if (arg == 1) {
      D_800C401C = 12.0f;
      func_80050DA0(0x64, 2, 0x28, 0x14, 0x7F);
    } else if (arg > 0) {
      if (arg < 4) {
        D_800C401C = -2.0f;
        func_80050DA0(0x65, 2, 0x28, 0x14, 0x7F);
      }
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006AEA4);

void func_8006B54C(void) { D_800C4068 = -1; }

s32 func_8006B55C(void) { return D_800C4064 < 2; }

void func_8006B56C(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
  D_800C4068 = 0;
  D_800C4060 = arg3;
  D_800C4064 = arg3;
  D_800FF4B0 = arg0;
  D_800FF4B4 = arg1;
  D_800FF4B8 = arg2;
}

void func_8006B5A0(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
  D_800C4068 = 1;
  D_800C4060 = arg3;
  D_800C4064 = arg3;
  D_800FF4B0 = arg0;
  D_800FF4B4 = arg1;
  D_800FF4B8 = arg2;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006B5D8);

void func_8006B980(void) {
  u8 sp10[0x20];

  D_800E1C08 = heap3_alloc(func_8005062C(0x70B, sp10));
  func_800506D4(D_800E1C08, sp10);
  D_800E1C0C = heap3_alloc(func_8005062C(0x70C, sp10));
  func_800506D4(D_800E1C0C, sp10);
}

void func_8006B9E4(void) {
  heap3_free(&D_800E1C08);
  heap3_free(&D_800E1C0C);
}

s32 func_8006BA10(void) { return !~D_800C4124; }

void func_8006BA24(s32 arg0) {
  u8* e;
  s32 t;

  if (arg0 == -1) {
    return;
  }
  e = D_800C406C + arg0 * 16;
  D_800C4120 = 0;
  D_800C411C = arg0;
  func_8006B980();
  t = *(s32*)(e + 4);
  D_800C4124 = 0;
  D_800C4128 = t;
  play_sound_effect(0x64, D_800C0E60, 0x64);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006BA94);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006BC80);

void func_8006C3E8(void) {
  u8 sp10[0x20];

  if (D_800C4010 == 0) {
    D_800E1C00 = heap3_alloc(func_8005062C(0x955, sp10));
    func_800506D4(D_800E1C00, sp10);
  }
  D_800C4010 = 1;
  D_800C4014 = 0;
  D_800C4018 = 0x90;
}

void func_8006C450(void) {
  if (D_800C4010 != 0) {
    heap3_free(&D_800E1C00);
  }
  D_800C4010 = 0;
}

/**
 * Stamps one 8-way-symmetric Bresenham circle of radius r into a 16-bit image,
 * flipping only the alpha bit of each pixel (0x0001 per halfword, so 0x00010001
 * per u32 pixel pair). D_800C4014 selects clear (hide) or set (show); the
 * caller walks r downward to sweep a filled iris.
 *
 * cx counts u32 pixel pairs and cy counts rows of 152 pairs; every stamp also
 * repeats one row down, so the visible step is a 2x2 pixel block.
 *
 * The empty do/while and the loop-scoped t are codegen shaping, not logic: they
 * set the register-allocation order this function's asm depends on.
 */
void stamp_circle_ring_alpha(u32* grid, s32 cx, s32 cy, s32 r) {
  s32 i;
  s32 j;
  s32 d;
  s32 cj;
  s32 ci;

  i = 0;
  j = r;
  d = r;
  if (D_800C4014 == 0) {
    while (i <= j) {
      s32 t;

      do {
        cj = j;
        ci = i;
        if (cj > 0x4B) {
          cj = 0x4B;
        }
        if (ci > 0x6F) {
          ci = 0x6F;
        }
      } while (0);
      grid[(cy + ci) * 152 + (cx + cj)] &= ~0x10001;
      grid[(cy - ci) * 152 + (cx + cj)] &= ~0x10001;
      grid[(cy + ci) * 152 + (cx + cj) + 152] &= ~0x10001;
      grid[(cy - ci) * 152 + (cx + cj) + 152] &= ~0x10001;
      grid[(cy + ci) * 152 + (cx - cj)] &= ~0x10001;
      grid[(cy - ci) * 152 + (cx - cj)] &= ~0x10001;
      grid[(cy + ci) * 152 + (cx - cj) + 152] &= ~0x10001;
      grid[(cy - ci) * 152 + (cx - cj) + 152] &= ~0x10001;

      ci = i;
      cj = j;
      if (cj > 0x6E) {
        cj = 0x6E;
      }
      if (ci > 0x4B) {
        ci = 0x4B;
      }
      grid[(cy + cj) * 152 + (cx + ci)] &= ~0x10001;
      grid[(cy - cj) * 152 + (cx + ci)] &= ~0x10001;
      grid[(cy + cj) * 152 + (cx + ci) + 152] &= ~0x10001;
      grid[(cy - cj) * 152 + (cx + ci) + 152] &= ~0x10001;
      grid[(cy + cj) * 152 + (cx - ci)] &= ~0x10001;
      grid[(cy - cj) * 152 + (cx - ci)] &= ~0x10001;
      grid[(cy + cj) * 152 + (cx - ci) + 152] &= ~0x10001;
      grid[(cy - cj) * 152 + (cx - ci) + 152] &= ~0x10001;

      t = d - 1;
      d = t - i * 2;
      if (d < 0) {
        d += (j - 1) * 2;
        j--;
      }
      i++;
    }
  } else {
    while (i <= j) {
      s32 t;

      do {
        cj = j;
        ci = i;
        if (cj > 0x4B) {
          cj = 0x4B;
        }
        if (ci > 0x6F) {
          ci = 0x6F;
        }
      } while (0);
      grid[(cy + ci) * 152 + (cx + cj)] |= 0x10001;
      grid[(cy - ci) * 152 + (cx + cj)] |= 0x10001;
      grid[(cy + ci) * 152 + (cx + cj) + 152] |= 0x10001;
      grid[(cy - ci) * 152 + (cx + cj) + 152] |= 0x10001;
      grid[(cy + ci) * 152 + (cx - cj)] |= 0x10001;
      grid[(cy - ci) * 152 + (cx - cj)] |= 0x10001;
      grid[(cy + ci) * 152 + (cx - cj) + 152] |= 0x10001;
      grid[(cy - ci) * 152 + (cx - cj) + 152] |= 0x10001;

      ci = i;
      cj = j;
      if (cj > 0x6E) {
        cj = 0x6E;
      }
      if (ci > 0x4B) {
        ci = 0x4B;
      }
      grid[(cy + cj) * 152 + (cx + ci)] |= 0x10001;
      grid[(cy - cj) * 152 + (cx + ci)] |= 0x10001;
      grid[(cy + cj) * 152 + (cx + ci) + 152] |= 0x10001;
      grid[(cy - cj) * 152 + (cx + ci) + 152] |= 0x10001;
      grid[(cy + cj) * 152 + (cx - ci)] |= 0x10001;
      grid[(cy - cj) * 152 + (cx - ci)] |= 0x10001;
      grid[(cy + cj) * 152 + (cx - ci) + 152] |= 0x10001;
      grid[(cy - cj) * 152 + (cx - ci) + 152] |= 0x10001;

      t = d - 1;
      d = t - i * 2;
      if (d < 0) {
        d += (j - 1) * 2;
        j--;
      }
      i++;
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006C8CC);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006C918);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006CD50);

extern s32 D_801B6090[];
extern u16 D_80106230[];
extern s8 D_801B71F6[];
extern s8 D_801B71FB[];
extern s8 D_801050BC[];
extern s8 D_801050BD;
extern s8 D_801050BE;
extern char D_80105118[];
extern char D_800D1450[];

extern s32 func_800521C0(void);
extern int sprintf(char* s, const char* fmt, ...);
extern void check_and_print_grid(char* str, s32 col, s32 row);

/* Builds and prints the debug flag grid. Three variable choices are
 * load-bearing for register allocation, not style: the clear loop counts in
 * `col` (with `i` it colours one register lower), the row index is copied
 * through `hasZero` (dead since the scan) before indexing D_80106230, and the
 * count is read as `D_801B6090[0]` so MEM_IN_STRUCT_P forces the ROM's re-read
 * at both nesting levels of the column loop -- a cached `s32*` pointer to it
 * keeps ONE pseudo and comes out 2 instructions short. The clear must also be
 * INDEX-form
 * (`D_80106230[col] = 0`): loop.c then hoists the base into the preheader,
 * which sits AFTER the entry guard as the ROM has it, where an explicit `u16*
 * p` initialiser is emitted before the guard. */
void func_8006CE88(s32 arg0) {
  s32 i;
  s32 hasZero;
  s32 col;
  s32 last;
  s32 counter;

  for (col = 0; col != D_801B6090[0]; col++) {
    D_80106230[col] = 0;
  }

  hasZero = 0;
  for (i = 0; i != D_801B6090[0]; i++) {
    s32 off = i * 0xB8;
    if (*(s8*)((u8*)D_801B71F6 + off) == 0) {
      hasZero = 1;
    }
  }
  arg0 += (hasZero == 0);

  D_801050BC[0] = -1;
  D_801050BD = 0;
  D_801050BE = 0;

  counter = 1;
  col = func_800521C0();
  {
    for (; col < arg0; col++) {
      last = -1;
      for (i = 0; i != D_801B6090[0]; i++) {
        s32 off = col * 2 + i * 0xB8;
        if (*(s8*)((u8*)D_801B71FB + off) != 0) {
          last = i;
          *(s8*)((u8*)D_801B71FB + off) = counter;
        }
      }
      if (col == arg0 - 1) {
        D_801050BC[0] = counter - 1;
        D_801050BC[1] = counter;
      }
      if (last == -1) {
        counter++;
        D_801050BC[2] = counter - 1;
      } else {
        hasZero = last;
        D_80106230[hasZero] += counter;
        counter = 1;
        D_801050BC[2] = 0;
      }
    }
  }

  sprintf(D_80105118, D_800D1450, D_801050BC[0], D_801050BD);
  check_and_print_grid(D_80105118, 0x11, 5);
}

extern u16 D_8012D3C8[4][7];
extern u16 D_801B725E[];
extern s8 D_801B60BA;
extern s8 D_801B60BB;
extern s8 D_801B60BE;
extern s8 D_801B60BF;
extern s32 D_800C4130;
extern s32 D_800C4134;
extern s32 D_800C4138;
extern s32 D_800C413C;
extern s16 D_801B60C6;
extern s16 D_801B60C8;
extern s16 D_801B60CA;

void func_8006D058(void) {
  s32 i;
  s32 j;
  s32 x;
  s32 y;
  s32 z;
  s32 z2;
  u16* dst;

  for (i = 0; i != 4; i++) {
    for (j = 0; j != 7; j++) {
      D_8012D3C8[i][j] = 0;
    }
  }

  for (i = 0; i != 4; i++) {
    dst = (u16*)((u8*)D_801B725E + i * 0xB8);
    for (j = 0; j != 7; j++) {
      dst[j] = D_8012D3C8[i][j];
    }
  }

  x = D_801B60BA;
  y = D_801B60BB;
  z = D_801B60BF;
  D_800C413C = 0;
  D_800C4144 = -1;
  D_800C4130 = x;
  D_800C4134 = y;
  if (z != -1) {
    D_800C4138 = z;
  }
  z2 = D_801B60BE;
  if (z2 != -1) {
    D_800C4138 = z2;
  }
  D_801B60C6 = x;
  D_801B60C8 = y;
  D_801B60CA = D_800C4138;
}

/* Snapshot 4 rows of 7 u16 out of the 0xB8-stride record array into the
 * scoreboard grid, then reset the readout state. The outer loop is a goto loop
 * on purpose: loop.c never sees it, so the `4` bound stays re-materialized
 * inside the loop the way the ROM has it. */
void func_8006D164(void) {
  s32 i = 0;
  s32 j;
  s32 n = 7;
  u16* src = D_801B725E;
  u16* dst = D_8012D3C8[0];

outer:
  j = 0;
  do {
    dst[j] = src[j];
    j++;
  } while (j != n);
  src += 0x5C;
  i++;
  dst += 7;
  if (i != 4) {
    goto outer;
  }

  D_800C413C = 0;
  D_800C4144 = -1;
  D_800C4130 = D_801B60C6;
  D_800C4134 = D_801B60C8;
  D_800C4138 = D_801B60CA;
}

s32 func_8006D1FC(void) { return D_800C4144; }

void func_8006D208(s32 arg0) { D_800C4144 = arg0; }

/* func_8006D214: CARRY (S241 terminal #base-register-vs-displacement, DEEPENED
 * S259). Same shape as func_8006D38C (see its comment for the derivation) plus
 * an FP scale on the compared value. Builds at 94/94 instructions: the
 * func_8006D38C model transferred verbatim, fixing the same two of three access
 * shapes and leaving the same one open — the ROM reaches the count by NEGATIVE
 * DISPLACEMENT off the held flag base (`lw t0,-0x2A(v1)`). Replacing the
 * array-form guard with a plain scalar read lets everything CSE and comes out
 * 87/94, so the array form is load-bearing.
 * Full reconstruction in docs/wip/func_8006D214.near-match.md.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D214);

/* func_8006D38C: CARRY (S241 terminal #base-register-vs-displacement, DEEPENED
 * S259 to 84/84, THIRD ACCESS SHAPE REACHED S260). Min-search over the
 * 0xB8-stride record array + a weighted u16-table redistribute.
 *
 * TWO of the three access shapes S241 called unreachable were solved in S259:
 *  - "ROM materializes &D_801B60BB in full and does `lb 0(a0)`" -> hold the
 *    address in a POINTER LOCAL (`s8* flag = &D_801B60BB;`) and read `*flag`;
 *  - "ROM holds &D_801B6090 across the inner loop and re-reads `lw 0(a3)`" ->
 *    read it as an ARRAY element (`extern s32 D_801B6090[]` + `D_801B6090[0]`),
 *    whose MEM_IN_STRUCT_P may-alias defeats the CSE that folded it to one
 * load. The inner loop must also be a `do`-`while`: the ROM has only ONE
 * zero-guard (the `if`), so a top-tested `for` adds a second `beqz` and a `for`
 * over a cached count CSEs the bound away.
 *
 * S260 REACHED THE THIRD SHAPE, `lw t0,-0x2B(a0)`. It needs the count and the
 * flag to be the SAME SYMBOL, because cse's use_related_value only relates
 * offsets within one symbol -- two distinct `D_` symbols can never produce a
 * negative displacement off each other. Model the region as a struct and view
 * it through the existing array symbol:
 *
 *   typedef struct { s32 count; u8 pad[0x27]; s8 flag; } RoundState;
 *   #define ROUND (*(RoundState*)D_801B6090)
 *   s8* flag = &ROUND.flag;           // la a0,D_801B60BB + lb 0(a0)
 *   ... ROUND.count ...               // addiu t0,a0,-0x2B / lw 0(t0)
 *
 * REMAINING (2 instructions short, 82/84): with both reads on one symbol, cse
 * merges the loop-bound read into the entry guard's read, so the preheader
 * `lw t0,-0x2B(a0)` disappears. The ROM keeps both loads with only a
 * constant-address store (`D_800C4144 = -1`) between them, which cannot
 * invalidate the first (cse.c note_mem_written:7564 sets `nonscalar` only for a
 * VARYING store address, and invalidate_memory:7715 purges `in_struct` entries
 * only then). Tried and rejected: array guard + struct bound (same address,
 * still merged), caching the bound in a preheader local (merges into a `move`),
 * and routing the D_800C4144 store through a pointer local to make its address
 * vary (expand still folds it to the symbol before cse records the write). Full
 * reconstruction in docs/wip/func_8006D38C.near-match.md.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D38C);
void func_8006D4DC(void) { D_800C4144 = -1; }

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006D4EC);

extern s8 D_801B71ED[];
extern s8 D_801B71FC[];
extern s8 D_800C1420[];
extern s32 func_8006CD50(s32 arg0, s32 arg1);
extern void func_8005DE60(void);

/**
 * Folds the finished round into the running per-mode statistics record
 * (`base + scenario_mode_id * 116 + 0xE98`, zero-initialised on first use).
 *
 * The record keeps a sample counter that saturates at 9999; once saturated
 * every running total is scaled by 9999/10000 each round, so the record decays
 * into a moving average instead of overflowing. Fields: 0x00 sample count,
 * 0x04 best value (clamped at -54), 0x08 and 0x1C per-hole totals, 0x0C a
 * high-water mark, 0x10/0x14 a weighted average and its sample count, 0x18 and
 * 0x28/0x2C saturating totals, 0x20/0x24 hole tallies against the course table,
 * 0x3C[18] per-hole bests and 0x60[18] per-hole played flags.
 *
 * Four spellings are load-bearing for codegen, not style. The loops count in
 * `i` and derive `off`/`p` from it so loop.c creates them as induction
 * variables, whose initialisers it then emits after the hoisted constants as
 * the ROM has them (an explicit `off += 2` IV inverts that order). The
 * `(s8) * (u8*)` and `(s16) * (u16*)` casts reproduce the ROM's unsigned load
 * plus sign-extend pair where a plain signed load would be one instruction
 * shorter. `D_801B71ED[0]` is re-read at each use because the intervening
 * stores invalidate it, and the flag store spells its address
 * `rec + D_801B71ED[0] + 0x60` so the record base stays the first addend.
 */
void accumulate_mode_stats(void) {
  u8* rec;
  s32* g;
  s8* cfg;
  s8* p;
  s32 sat;
  s64 v;
  s64 sum;
  s32 hole;
  s32 off;
  s32 i;
  s32 n;
  s32 x;

  rec = func_8005AF50() + (scenario_mode_id * 116 + 0xE98);
  g = &D_800FF4D0;
  cfg = &D_800C1420[scenario_mode_id * 200];

  if (*(s32*)(rec + 0x00) <= 0) {
    *(s32*)(rec + 0x00) = 0;
    *(s32*)(rec + 0x04) = 0x7FFFFFFF;
    *(s32*)(rec + 0x08) = 0;
    *(s32*)(rec + 0x0C) = 0;
    *(s32*)(rec + 0x10) = 0;
    *(s32*)(rec + 0x14) = 0;
    *(s32*)(rec + 0x18) = 0;
    *(s32*)(rec + 0x1C) = 0;
    *(s32*)(rec + 0x20) = 0;
    *(s32*)(rec + 0x24) = 0;
    *(s32*)(rec + 0x28) = 0;
    *(s32*)(rec + 0x2C) = 0;
  }
  if (*(s32*)(rec + 0x00) < 9999) {
    sat = 0;
    *(s32*)(rec + 0x00) = *(s32*)(rec + 0x00) + 1;
  } else {
    sat = 1;
  }

  D_801B71F6[0] = 0;
  v = func_8006CD50(0x12, 0);
  if (v < *(s32*)(rec + 0x04)) {
    *(s32*)(rec + 0x04) = v;
  }
  if (*(s32*)(rec + 0x04) < -0x36) {
    *(s32*)(rec + 0x04) = -0x36;
  }
  hole = D_801B71ED[0];
  if (v < (s16) * (u16*)(rec + 0x3C + hole * 2)) {
    *(s16*)(rec + 0x3C + hole * 2) = v;
    *(u8*)(rec + D_801B71ED[0] + 0x60) = 1;
    hole = D_801B71ED[0];
    if (*(s16*)(rec + 0x3C + hole * 2) < -0x36) {
      *(s16*)(rec + 0x3C + hole * 2) = -0x36;
    }
  }

  sum = 0;
  for (off = 0; off != 0x24; off += 2) {
    sum += (s8) * (u8*)((u8*)D_801B71FB + off);
  }
  sum += *(s32*)(rec + 0x08);
  if (sat == 1) {
    sum = sum * 9999 / 10000;
  }
  if (sum > 0x7FFFFFFF) {
    sum = 0x7FFFFFFF;
  }
  *(s32*)(rec + 0x08) = sum;
  if (*(s32*)(rec + 0x0C) < g[3]) {
    *(s32*)(rec + 0x0C) = g[3];
  }

  n = *(s32*)(rec + 0x14);
  if (n == 0) {
    if (g[5] == 0) {
      *(s32*)(rec + 0x10) = 0;
    } else if (g[5] > 0) {
      *(s32*)(rec + 0x10) = g[4] / g[5];
      *(s32*)(rec + 0x14) = 1;
    } else {
      *(s32*)(rec + 0x10) = 0;
    }
  } else if (g[5] > 0) {
    *(s32*)(rec + 0x10) = (*(s32*)(rec + 0x10) * n + g[4]) / (n + g[5]);
    *(s32*)(rec + 0x14) = *(s32*)(rec + 0x14) + g[5];
  }

  x = *(s32*)(rec + 0x18) + g[2];
  if (x > 0x7FFFFFFF) {
    *(s32*)(rec + 0x18) = 0x7FFFFFFF;
  } else {
    *(s32*)(rec + 0x18) = x;
  }
  if (sat == 1) {
    *(s32*)(rec + 0x18) = *(s32*)(rec + 0x18) * 9999 / 10000;
  }

  sum = 0;
  for (i = 0; i != 0x12; i++) {
    s32 b;

    off = i * 2;
    b = (s8) * (u8*)((u8*)D_801B71FC + off);
    if (b != -1) {
      sum += b;
    }
  }
  sum += *(s32*)(rec + 0x1C);
  if (sat == 1) {
    sum = sum * 9999 / 10000;
  }
  if (sum > 0x7FFFFFFF) {
    sum = 0x7FFFFFFF;
  }
  *(s32*)(rec + 0x1C) = sum;

  for (i = 0; i != 0x12; i++) {
    off = i * 2;
    p = &cfg[i * 10];
    if (*(s8*)((u8*)D_801B71FB + off) <= p[0x14]) {
      x = *(s32*)(rec + 0x20);
      if (x <= 0x7FFFFFFE) {
        *(s32*)(rec + 0x20) = x + 1;
      }
      if (sat == 1) {
        *(s32*)(rec + 0x20) = *(s32*)(rec + 0x20) * 9999 / 10000;
      }
    }
  }

  for (i = 0; i != 0x12; i++) {
    s32 d;

    off = i * 2;
    p = &cfg[i * 10];
    d = *(s8*)((u8*)D_801B71FB + off) - *(s8*)((u8*)D_801B71FC + off);
    if (p[0x14] - d >= 2) {
      x = *(s32*)(rec + 0x24);
      if (x <= 0x7FFFFFFE) {
        *(s32*)(rec + 0x24) = x + 1;
      }
      if (sat == 1) {
        *(s32*)(rec + 0x24) = *(s32*)(rec + 0x24) * 9999 / 10000;
      }
    }
  }

  x = *(s32*)(rec + 0x28) + g[0];
  if (x > 0x7FFFFFFF) {
    *(s32*)(rec + 0x28) = 0x7FFFFFFF;
  } else {
    *(s32*)(rec + 0x28) = x;
  }
  x = *(s32*)(rec + 0x2C) + g[1];
  if (x > 0x7FFFFFFF) {
    *(s32*)(rec + 0x2C) = 0x7FFFFFFF;
  } else {
    *(s32*)(rec + 0x2C) = x;
  }
  func_8005DE60();
}

void func_8006DDCC(void) {
  u8* p = func_8005AF50();
  s32 v;

  D_800FF4D0 = 0;
  D_800FF4D4 = 0;
  D_800FF4D8 = 0;
  D_800FF4E4 = 0;
  v = *(s32*)(p + scenario_mode_id * 116 + 0xEA4);
  D_800FF4E0 = 0;
  D_800FF4E8 = 0;
  D_800FF4DC = v;
}

void func_8006DE44(void) {
  u8* p = func_8005AF50();
  s32 v;

  D_800FF4D0 = 0;
  D_800FF4D4 = 0;
  D_800FF4D8 = 0;
  D_800FF4E4 = 0;
  v = *(s32*)(p + scenario_mode_id * 116 + 0xEA4);
  D_800FF4E8 = 0;
  D_800FF4DC = v;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DEB4);

extern s32 D_801B7250;
extern s32 D_801B7270;
extern void* get_shot_data(void);

s32 func_8006DF84(void) {
  s32* base = &D_800FF4D0;
  s32 pending;
  s16 field;
  void* shot;

  shot = get_shot_data();
  pending = D_801B7250;
  D_801B7270 = pending;
  field = *(s16*)((u8*)shot + 0x16);
  if (field != 0) {
    goto clear;
  }
  if (pending > 0) {
    D_800FF4E8 = 1;
    goto done;
  }
clear:
  base[6] = 0;
done:
  return D_801B7270 > base[3];
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DFF0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006E210);
