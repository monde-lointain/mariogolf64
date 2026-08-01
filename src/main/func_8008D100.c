#include "common.h"

extern u32 sky_panel_cycle_mode_sel;
extern s32 D_800C5EE4;
extern u16* D_800C5EE0;
extern s32 D_800C7304;
extern s32 D_800C730C;

extern s32 D_801B6088;
extern s32 D_801B6098;
extern s32 scenario_mode_id;
extern s32 D_801B609C;
extern s32 D_801B608C;
extern s32 putter_mode_flag;
extern s32 wind_magnitude;
extern s16 wind_angle_bam;
extern u32 D_800BA9FC;
extern s16 g_scenery_wind_angle_b;
extern s16 g_scenery_wind_angle_a;
extern s32 D_801B6090;

extern void flag_set(s32);
extern void func_80069FBC(void);
extern void func_80069F38(void);
extern void cfb_set_num(u32);

extern Gfx* glistp;

typedef struct {
  s16 ulx;
  s16 uly;
  s16 lrx;
  s16 lry;
} ScissorRect;

extern ScissorRect D_801B7F30;

/* Game/session init: zero the 0x44-byte state block, seed mode/wind globals
 * (wind magnitude = (rand() & 0x3F) * 6000, wind angle from a second rand),
 * then run the sub-init calls and select framebuffer 2. */
void init_scenario_state(void) {
  memset(&D_801B6088, 0, 0x44);
  D_801B6098 = 9;
  scenario_mode_id = 0;
  D_801B609C = 0;
  D_801B608C = 0xC;
  putter_mode_flag = 0;
  wind_magnitude = (rand() & 0x3F) * 6000;
  wind_angle_bam = rand();
  D_800BA9FC = 6;
  g_scenery_wind_angle_b = 0x1000;
  g_scenery_wind_angle_a = 0x6000;
  flag_set(7);
  D_801B6090 = 1;
  func_80069FBC();
  func_80069F38();
  cfb_set_num(2);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008D1DC);

/**
 * Emits the sun's lens flare: eight sprites strung along the line from the sun
 * at (D_800C5F24, D_800C5F2C) through the screen centre, each with its own
 * primitive and environment colour and its own radius, all sharing one 64x64
 * 8-bit intensity texture under a (PRIM - ENV) * TEXEL0 + ENV combine.
 *
 * Nothing is emitted unless the sun is on screen (the nine early returns) and
 * the fade term `s`, which falls off with the sun's distance from the centre
 * and is capped by the per-scenario limit at `e->field_0x14->field_0x18`, is at
 * least 8. `s` doubles as the primitive alpha and scales every sprite's radius.
 *
 * Three spellings below are load-bearing:
 *   - `D_800C5F24` / `D_800C5F2C` are subscripted arrays, not scalars, so their
 *     loads conflict with the MEM_IN_STRUCT_P stores through `Gfx *` and stay
 *     inside the sprite loop. As plain scalars loop.c hoists them and the
 *     160.0f/96.0f terms with them, which costs $f20/$f22 and 0x20 of frame.
 *   - `pa` and `pb` are declared in blocks that open one packet early, so their
 *     pseudos are numbered ahead of the pipe-sync packets that precede them.
 *     reload1.c hands out spill slots in ascending pseudo number, and the ROM's
 *     Gfx* slots run 0x2C, 0x3C, 0x34, 0x54, 0x4C -- each pair inverted.
 *   - `tod` and `n << 8` are statements of their own, in that order, ahead of
 *     the `q` address. That fixes the emission order of the three independent
 *     computations feeding the entry address: the scheduler front-loads the
 *     `tod` load, and the two shifts then follow in source order.
 */
extern s32 sky_time_of_day_idx;
extern f32 D_800C5F24[];
extern f32 D_800C5F2C[];
extern f32 D_800C691C;
extern u8 D_800C6014[];
extern u32 D_800FE3D4;
extern s32 D_801B557C;
extern s32 func_80051FCC(void);

typedef struct {
  u8 prim_r;
  u8 prim_g;
  u8 prim_b;
  u8 env_r;
  u8 env_g;
  u8 env_b;
  u8 unk_06[2];
  s32 radius;
  u8 unk_0C[4];
  f32 t;
} FlarePoint;

extern FlarePoint D_800C6920[];

void emit_lens_flare_dl(Gfx** gfxp) {
  Gfx* gfx;
  u8* e;
  u8* q;
  f32 dx;
  f32 dy;
  f32 s;
  f32 px;
  f32 py;
  s32 n;
  s32 m;
  s32 tod;
  s32 i;
  s32 w;
  s32 d;

  gfx = *gfxp;
  n = func_80051FCC();
  m = putter_mode_flag;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  do {
    gDPPipeSync(gfx++);
  } while (0);

  tod = sky_time_of_day_idx;
  n = n << 8;
  q = D_800C6014 + (((m << 2) + tod) << 5);
  e = (u8*)(n + (u32)q);
  if (e[0x11] == 0) {
    return;
  }

  if (D_800C5F24[0] <= 16.0f) {
    return;
  }
  if (304.0f <= D_800C5F24[0]) {
    return;
  }
  if (D_800C5F2C[0] <= 16.0f) {
    return;
  }
  if (224.0f <= D_800C5F2C[0]) {
    return;
  }
  if (D_800C5F2C[0] < -64.0f) {
    return;
  }
  if (360.0f < D_800C5F2C[0]) {
    return;
  }
  if (D_800C5F24[0] < -120.0f) {
    return;
  }
  if (440.0f < D_800C5F24[0]) {
    return;
  }
  if (D_801B557C == 1) {
    return;
  }

  dx = 320.0f - D_800C5F24[0];
  if (160.0f < dx) {
    dx = 320.0f - dx;
  }
  dy = 240.0f - D_800C5F2C[0];
  if (96.0f < dy) {
    dy = 240.0f - dy;
  }
  s = (dx + dy + 16.0f) * D_800C691C;
  if (s < 8.0f) {
    return;
  }
  if (*(f32*)(*(u32*)(e + 0x14) + 0x18) < s) {
    s = *(f32*)(*(u32*)(e + 0x14) + 0x18);
  }

  gDPPipeSync(gfx++);
  {
    Gfx* pa;

    gDPPipeSync(gfx++);
    pa = gfx++;
    gDPSetTexturePersp(pa, G_TP_NONE);
  }
  {
    Gfx* pb;

    gDPPipeSync(gfx++);
    pb = gfx++;
    gDPSetTextureLUT(pb, G_TT_NONE);
  }
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 255, 255, 255, 1);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT,
                    PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, ENVIRONMENT, TEXEL0,
                    ENVIRONMENT, PRIMITIVE, 0, TEXEL0, 0);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gDPLoadTextureBlock(gfx++, D_800FE3D4 & ~7, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64,
                      0, G_TX_MIRROR, G_TX_MIRROR, 6, 6, G_TX_NOLOD,
                      G_TX_NOLOD);

  for (i = 0; i != 8; i++) {
    px = D_800C5F24[0] + (160.0f - D_800C5F24[0]) * D_800C6920[i].t;
    py = D_800C5F2C[0] + (96.0f - D_800C5F2C[0]) * D_800C6920[i].t;
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, D_800C6920[i].prim_r, D_800C6920[i].prim_g,
                    D_800C6920[i].prim_b, (u32)s);
    gDPSetEnvColor(gfx++, D_800C6920[i].env_r, D_800C6920[i].env_g,
                   D_800C6920[i].env_b, 255);
    w = (s32)((f32)D_800C6920[i].radius * (s + 16.0f) * (1.0f / 128.0f));
    d = 0x10000 / w;
    gSPScisTextureRectangle(
        gfx++, ((s32)(px - (f32)w)) << 2, ((s32)(py - (f32)w)) << 2,
        ((s32)(px + (f32)w)) << 2, ((s32)(py + (f32)w)) << 2, G_TX_RENDERTILE,
        0, 0, d, d);
  }

  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gDPPipeSync(gfx++);
  *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008DDDC);

/* Integer lerp, truncated: a0*(1-t) + a1*t, t in $a2 (o32 GPR).
 *
 * TRUE ORIGIN: this is a GCC nested function (an orphaned out-of-line child).
 * The dead 8-byte frame + `sw $v0,0(sp)` (never reloaded) is the
 * nested-function prologue homing the incoming STATIC CHAIN ($v0 ==
 * STATIC_CHAIN_REGNUM = GP_REG_FIRST+2, mips.h:1310); this child never reads a
 * parent variable so the chain is homed but unused. A clean nested child `int
 * f(int a,int b,float t){ return a*(1-t)+b*t; }` inside a parent emits these
 * exact 20 words (verified byte-for-byte). The parent inlined its call, so no
 * jal / fn-pointer to 0x8008E164 survives anywhere in the ROM (orphaned dead
 * body).
 *
 * The `volatile s32` local below is a standalone stand-in that reproduces the
 * same bytes (it cannot be written as a real nested fn without its parent,
 * which is undecompiled and would not re-emit this symbol). Move it inside the
 * parent as a nested function if that parent is ever recovered. See the memory
 * dead-frame-dead-v0-store-crack and docs/wip/func_8008E164.nested.md. */
s32 lerp_s32(s32 a0, s32 a1, f32 t) {
  s32 r;
  volatile s32 unused = r;
  return a0 * (1.0f - t) + a1 * t;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100",
            per_hole_skybox_palette_load);

/* One editable fog/light preset. The three-entry colour arrays are the page's
 * three columns; `a` has a fourth entry because row 3 also edits the blend
 * percentage at 0x10. */
typedef struct {
  /* 0x00 */ s32 cc;
  /* 0x04 */ u8 r[3];
  /* 0x07 */ u8 g[3];
  /* 0x0A */ u8 b[3];
  /* 0x0D */ u8 a[4];
  /* 0x11 */ u8 unk11[0x0F];
} FogPreset; /* 0x20 */

/* [time-of-day set][putter mode * 4 + sky slot] */
/* The preset table itself is `u8 D_800C6014[]` (declared above, and indexed the
 * same way by the sibling emitter). */
/* The preset the page currently edits; re-derived from the selectors below on
 * every frame. */
extern FogPreset* D_800E3980;
extern s32 D_800C6CE4; /* cursor row: 0=R 1=G 2=B 3=A 4=fog preset B */
extern s32 D_800C6CE8; /* cursor column within the row */
/* Four copies of ten 16-byte light records; only each record's first three
 * bytes (its RGB triple) are rebuilt here. */
extern u8 D_800C69CC[4][10][0x10];
extern u16 D_800C6C52[];
/* One five-word record: R, G, B, z-min, z-max. ghidra_symbols.txt names each
 * word separately, so the array carries the first word's name. */
extern s32 g_fog_presetB_color_r[5];
extern u16 D_800FBDCE;
extern u16 debug_menu_pad_buttons_c;
extern void* course_panorama_ptr;
extern char D_80105118[];

extern s32 flag_is_set(s32 flag);
extern void check_and_print_grid(char* str, s32 col, s32 row);
extern s32 func_80051FCC(void);
extern u32 func_8005062C(u16 index, void* out);
extern void func_800506D4(void* data, void* slot);
extern void func_8002A97C(s32 arg0, s32 arg1);

/* Debug fog/light editor page, one frame. Steps the row/column cursor from the
 * pad, applies the held-direction edit to the selected channel, clamps every
 * edited value, rebuilds the ten light-record RGB triples from the preset, and
 * prints the five channel rows. Editing the alpha row also re-loads and
 * re-blends the course panorama palette, which is why that arm raises
 * `refresh`.
 *
 * Returns 1 when the page ran and 0 when its debug flag is clear; the caller
 * skips the normal sky update on a 1. */
s32 run_fog_debug_editor(void) {
  u8 spvar[0x20];
  u8* q;
  s32 refresh;
  s32 n;
  s32 m;
  s32 tod;
  s32 i;
  s32 j;
  s32 half;
  s32 quarter;
  s32 red;
  s32 green;
  s32 blue;
  s32 alpha;
  s32 weight;
  u16* pal;
  u16 texel;
  u16 sum;

  refresh = 0;
  if (!flag_is_set(0x2D)) {
    return 0;
  }

  check_and_print_grid(" ", D_800C6CE4 * 4 + 14, D_800C6CE8 + 5);
  /* Same three-statement spelling as the sibling emitter above: the `tod` load
   * and the `n << 8` shift are statements of their own, ahead of `q`. */
  n = func_80051FCC();
  m = putter_mode_flag;
  tod = sky_time_of_day_idx;
  D_800E3980 = &((FogPreset(*)[8])D_800C6014)[n][(m << 2) + tod];

  if (debug_menu_pad_buttons_c & 0x202) {
    if (D_800C6CE4 > 0) {
      D_800C6CE4--;
    }
  }
  if (debug_menu_pad_buttons_c & 0x101) {
    if (D_800C6CE4 < 4) {
      D_800C6CE4++;
    }
  }
  if (debug_menu_pad_buttons_c & 0x808) {
    D_800C6CE8--;
  }
  if (debug_menu_pad_buttons_c & 0x404) {
    D_800C6CE8++;
  }
  if (D_800C6CE8 < 0) {
    D_800C6CE8 = 0;
  }
  /* Rows 3 and 4 are one and two columns wider than the colour rows, and their
   * limit happens to equal the row index. */
  if (D_800C6CE4 == 3) {
    if (D_800C6CE8 >= 4) {
      D_800C6CE8 = D_800C6CE4;
    }
  } else if (D_800C6CE4 == 4) {
    if (D_800C6CE8 >= 5) {
      D_800C6CE8 = D_800C6CE4;
    }
  } else if (D_800C6CE8 >= 3) {
    D_800C6CE8 = 2;
  }

  if (D_800FBDCE & 0x8000) {
    switch (D_800C6CE4) {
      case 0:
        D_800E3980->r[D_800C6CE8] += 4;
        D_800E3980->r[D_800C6CE8] %= 0x100;
        break;
      case 1:
        D_800E3980->g[D_800C6CE8] += 4;
        D_800E3980->g[D_800C6CE8] %= 0x100;
        break;
      case 2:
        D_800E3980->b[D_800C6CE8] += 4;
        D_800E3980->b[D_800C6CE8] %= 0x100;
        break;
      case 3:
        D_800E3980->a[D_800C6CE8] += 1;
        refresh = 1;
        if (D_800C6CE8 == 3) {
          if (D_800E3980->a[3] == 0x65) {
            D_800E3980->a[3] = 0;
          }
          if (D_800E3980->a[D_800C6CE8] >= 0x66) {
            D_800E3980->a[D_800C6CE8] = 0x64;
          }
        } else {
          D_800E3980->a[D_800C6CE8] &= 0x1F;
        }
        break;
      case 4:
        g_fog_presetB_color_r[D_800C6CE8] += 4;
        break;
    }
  } else if (D_800FBDCE & 0x4000) {
    switch (D_800C6CE4) {
      case 0:
        D_800E3980->r[D_800C6CE8] -= 4;
        D_800E3980->r[D_800C6CE8] %= 0x100;
        break;
      case 1:
        D_800E3980->g[D_800C6CE8] -= 4;
        D_800E3980->g[D_800C6CE8] %= 0x100;
        break;
      case 2:
        D_800E3980->b[D_800C6CE8] -= 4;
        D_800E3980->b[D_800C6CE8] %= 0x100;
        break;
      case 3:
        D_800E3980->a[D_800C6CE8] -= 1;
        refresh = 1;
        if (D_800C6CE8 == 3) {
          if (D_800E3980->a[3] == 0x65) {
            D_800E3980->a[3] = 0;
          }
          if (D_800E3980->a[D_800C6CE8] >= 0x66) {
            D_800E3980->a[D_800C6CE8] = 0x64;
          }
        } else {
          D_800E3980->a[D_800C6CE8] &= 0x1F;
        }
        break;
      case 4:
        g_fog_presetB_color_r[D_800C6CE8] -= 4;
        break;
    }
  }

  if (g_fog_presetB_color_r[0] > 0xFF) {
    g_fog_presetB_color_r[0] = 0xFF;
  }
  if (g_fog_presetB_color_r[1] > 0xFF) {
    g_fog_presetB_color_r[1] = 0xFF;
  }
  if (g_fog_presetB_color_r[2] > 0xFF) {
    g_fog_presetB_color_r[2] = 0xFF;
  }
  if (g_fog_presetB_color_r[3] > 0x4B0) {
    g_fog_presetB_color_r[3] = 0x4B0;
  }
  if (g_fog_presetB_color_r[4] > 0x4B0) {
    g_fog_presetB_color_r[4] = 0x4B0;
  }
  if (g_fog_presetB_color_r[0] < 0) {
    g_fog_presetB_color_r[0] = 0;
  }
  if (g_fog_presetB_color_r[1] < 0) {
    g_fog_presetB_color_r[1] = 0;
  }
  if (g_fog_presetB_color_r[2] < 0) {
    g_fog_presetB_color_r[2] = 0;
  }
  if (g_fog_presetB_color_r[3] < 0) {
    g_fog_presetB_color_r[3] = 0;
  }
  if (g_fog_presetB_color_r[4] < 0) {
    g_fog_presetB_color_r[4] = 0;
  }

  i = 0;
  do {
    j = 0;
    do {
      D_800C69CC[i][0][j] = D_800E3980->r[j];
      D_800C69CC[i][1][j] = D_800E3980->g[j];
      D_800C69CC[i][2][j] = D_800E3980->g[j];
      D_800C69CC[i][3][j] = D_800E3980->r[j];
      D_800C69CC[i][4][j] = D_800E3980->r[j];
      sum = D_800E3980->r[j] + D_800E3980->g[j];
      tod = (s32)((f32)sum * 0.5f);
      half = (s32)((f32)(D_800E3980->r[j] + 63) * 0.5f);
      quarter = (s32)((f32)(tod + 63) * 0.5f);
      D_800C69CC[i][5][j] = half;
      D_800C69CC[i][6][j] = quarter;
      D_800C69CC[i][7][j] = quarter;
      D_800C69CC[i][8][j] = half;
      D_800C69CC[i][9][j] = half;
      j++;
    } while (j != 3);
    i++;
  } while (i != 4);

  if (refresh) {
    func_8005062C(D_800C6C52[func_80051FCC() * 2], spvar);
    func_800506D4(course_panorama_ptr, spvar);

    i = 0;
    do {
      texel = ((u16*)course_panorama_ptr)[i + 4];
      red = texel >> 11;
      green = (texel >> 6) & 0x1F;
      blue = (texel >> 1) & 0x1F;
      alpha = ((u16*)course_panorama_ptr)[i + 4] & 1;
      if (alpha == 0) {
        red = (D_800E3980->g[0] * 5 + D_800E3980->r[0]) / 48;
        green = (D_800E3980->g[1] * 5 + D_800E3980->r[1]) / 48;
        blue = (D_800E3980->g[2] * 5 + D_800E3980->r[2]) / 48;
      } else {
        weight = 100 - D_800E3980->a[3];
        red = (red * weight + D_800E3980->a[0] * D_800E3980->a[3]) / 100;
        green = (green * weight + D_800E3980->a[1] * D_800E3980->a[3]) / 100;
        blue = (blue * weight + D_800E3980->a[2] * D_800E3980->a[3]) / 100;
      }
      if (red < 0) {
        red = 0;
      }
      if (green < 0) {
        green = 0;
      }
      if (blue < 0) {
        blue = 0;
      }
      if (red >= 0x20) {
        red = 0x1F;
      }
      if (green >= 0x20) {
        green = 0x1F;
      }
      if (blue >= 0x20) {
        blue = 0x1F;
      }
      ((u16*)course_panorama_ptr)[i + 4] =
          (red << 11) | (green << 6) | (blue << 1) | alpha;
      i++;
    } while (i != 0x100);
  }

  check_and_print_grid("   T   B   C   BG  FOG", 13, 11);
  sprintf(D_80105118, "R: %3d %3d %3d %3d %4d  ", D_800E3980->r[0],
          D_800E3980->g[0], D_800E3980->b[0], D_800E3980->a[0],
          g_fog_presetB_color_r[0]);
  check_and_print_grid(D_80105118, 13, 12);
  sprintf(D_80105118, "G: %3d %3d %3d %3d %4d  ", D_800E3980->r[1],
          D_800E3980->g[1], D_800E3980->b[1], D_800E3980->a[1],
          g_fog_presetB_color_r[1]);
  check_and_print_grid(D_80105118, 13, 13);
  sprintf(D_80105118, "B: %3d %3d %3d %3d %4d  ", D_800E3980->r[2],
          D_800E3980->g[2], D_800E3980->b[2], D_800E3980->a[2],
          g_fog_presetB_color_r[2]);
  check_and_print_grid(D_80105118, 13, 14);
  sprintf(D_80105118, "A:             %3d %4d  ", D_800E3980->a[3],
          g_fog_presetB_color_r[3]);
  check_and_print_grid(D_80105118, 13, 15);
  sprintf(D_80105118, "CC:%d               %4d  ", D_800E3980->cc,
          g_fog_presetB_color_r[4]);
  check_and_print_grid(D_80105118, 13, 16);
  quarter = D_800C6CE8 + 12;
  check_and_print_grid(">", D_800C6CE4 * 4 + 15, quarter);
  cfb_set_num(3);
  func_8002A97C(4, 0);
  return 1;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", update_sky_panel_verts);

void set_sky_panel_cycle_mode_sel(u32 arg0) { sky_panel_cycle_mode_sel = arg0; }

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", emit_sky_bg_panel_dl);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", emit_sky_panorama_strips_dl);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8009226C);

void func_80092324(void) {}

/* Snapshot-replay display-list emitter over a Gfx** parameter, dispatching on
 * D_800C5EE8. S302 carry: reconstructed to 698 instructions against the ROM's
 * 697 with an exact -0x38 frame, every DL command word verified against gbi.h.
 * The residual is one register-allocation decision -- loop B hoists 0xF3000000
 * where the ROM materialises it in-body, which costs the callee-saved scratch
 * the ROM spends on spilling `src` to 0xC($sp) -- and it permutes the whole
 * body. Full body, the resolved macro table and the measured lever table are in
 * docs/wip/func_8009232C.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8009232C);

/**
 * Replays the captured frame at `D_800C5EE0` as an 8x8 grid of 38x28 RGBA16
 * tiles peeled off in a clockwise spiral, `D_800C5EE4` tiles at a time.
 *
 * The snapshot is treated as a 304x224 image, so tile (x, y) loads the region
 * at (x * 38, y * 28) and draws it at (+8, +8) -- the same 8-pixel border the
 * capture wrote it under. `D_800C5EE4` counts down by four per call, so each
 * call emits four fewer tiles than the last and the frozen frame retreats along
 * the spiral. The first `count - 4` tiles are opaque DECALRGB; the trailing
 * four are the fading edge, re-emitted under G_RM_CLD_SURF with a TEXEL0 *
 * PRIMITIVE alpha combine and a primitive alpha stepping 204, 153, 102, 51.
 *
 * TRUE ORIGIN of `func_80092E10`: `spiral_step` below is a GCC nested function.
 * The ROM child homes its incoming static chain ($v0 == STATIC_CHAIN_REGNUM =
 * GP_REG_FIRST + 2, mips.h:1310) into an 8-byte frame and reaches the seven
 * parent counters through it, which is also why the parent re-loads `x` and `y`
 * from the frame on every iteration instead of keeping them in registers. gcc
 * emits the child ahead of the parent, which is why 0x80092E10 sits below
 * 0x80092F18, and why no standalone symbol for it survives here.
 *
 * The seven counters are declared in frame order (`x` .. `x_min` at sp+0x10 ..
 * sp+0x28) and seeded largest-offset-first so the two `7` stores share one
 * register ahead of the zero stores, matching the ROM's store order.
 */
void emit_snapshot_spiral_wipe_dl(Gfx** gfxp) {
  s32 x;
  s32 x_max;
  s32 y_min;
  s32 leg;
  s32 y;
  s32 y_max;
  s32 x_min;
  Gfx* gfx;
  s32 n;
  s32 count;
  s32 i;
  s32 alpha;
  u32 src;

  void spiral_step(void) {
    switch (leg) {
      case 0:
        if (++x == x_max) {
          y_min++;
          leg++;
        }
        break;
      case 1:
        if (++y == y_max) {
          x_max--;
          leg++;
        }
        break;
      case 2:
        if (--x == x_min) {
          y_max--;
          leg++;
        }
        break;
      case 3:
        if (--y == y_min) {
          leg = 0;
          x_min++;
        }
        break;
    }
  }

  gfx = *gfxp;
  n = D_800C5EE4;
  src = (u32)D_800C5EE0;

  if (n > 0) {
    count = n - 4;
    alpha = 255;
    y_max = 7;
    x_max = 7;
    y_min = 0;
    x_min = 0;
    leg = 0;
    x = 0;
    y = 0;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);

    if (count > 64) {
      count = 64;
    }

    for (i = 0; i != count; i++) {
      gDPLoadTextureTile(gfx++, src & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b, 304, 224,
                         x * 38, y * 28, x * 38 + 37, y * 28 + 27, 0, 0, 0, 0,
                         0, 0, 0);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++, (x * 38 + 8) << G_TEXTURE_IMAGE_FRAC,
                          (y * 28 + 8) << G_TEXTURE_IMAGE_FRAC,
                          (x * 38 + 46) << G_TEXTURE_IMAGE_FRAC,
                          (y * 28 + 36) << G_TEXTURE_IMAGE_FRAC,
                          G_TX_RENDERTILE, (x * 38) << 5, (y * 28) << 5,
                          1 << 10, 1 << 10);
      gDPPipeSync(gfx++);
      spiral_step();
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0,
                      TEXEL0, 0, 0, 0, PRIMITIVE);

    count += 4;
    if (count > 64) {
      count = 64;
    }

    for (; i != count; i++) {
      alpha -= 51;
      gDPLoadTextureTile(gfx++, src & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b, 304, 224,
                         x * 38, y * 28, x * 38 + 37, y * 28 + 27, 0, 0, 0, 0,
                         0, 0, 0);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++, (x * 38 + 8) << G_TEXTURE_IMAGE_FRAC,
                          (y * 28 + 8) << G_TEXTURE_IMAGE_FRAC,
                          (x * 38 + 46) << G_TEXTURE_IMAGE_FRAC,
                          (y * 28 + 36) << G_TEXTURE_IMAGE_FRAC,
                          G_TX_RENDERTILE, (x * 38) << 5, (y * 28) << 5,
                          1 << 10, 1 << 10);
      gDPPipeSync(gfx++);
      spiral_step();
    }

    D_800C5EE4 -= 4;
  }

  *gfxp = gfx;
}

void func_800934CC(s32 arg0) {
  if (arg0 == -1) {
    D_800C7304 = 0;
  } else if (arg0 == 3) {
    D_800C730C = arg0;
    D_800C7304 = 2;
  } else {
    D_800C7304 = 1;
    D_800C730C = arg0;
  }
}

extern u16* nuGfxCfb_ptr;
extern s32 D_800C7310;
extern u16* D_800C5EE0;
/* Source framebuffer for the snapshot copy below. */
extern u32 D_800FED10;

/**
 * Copies the previous frame into the snapshot buffer once, the first time the
 * capture flag asks for it.
 *
 * The copy-cycle preamble is emitted unconditionally; the copy itself runs only
 * on the D_800C7304 == 1 edge, which then advances the flag to 2 so later
 * frames emit the preamble alone. The 304x224 region at (8, 8) is copied six
 * scanlines per texrect over 38 strips, the last one being the 224-line
 * remainder, after which the colour image is pointed back at the live cfb.
 *
 * `src` holds the source framebuffer address in a local rather than re-reading
 * the global: the display-list stores alias it, so the read cannot be hoisted
 * out of the loop otherwise, and the copy is what lets loop.c strength-reduce
 * the row address to a single stepped induction variable.
 */
void capture_frame_snapshot(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  s32 i;
  s32 lines;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, 0, 0);
  gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_COPY);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);

  if (D_800C7304 == 1) {
    u32 src = D_800FED10;

    D_800C7304 = 2;
    D_800C7310 = 0xFF;

    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                     OS_K0_TO_PHYSICAL(D_800C5EE0) & ~7);
    gDPPipeSync(gfx++);

    for (i = 0; i != 38; i++) {
      lines = (i != 37) ? 6 : 2;

      gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                         (src + i * (320 * 2 * 6)) & ~7);
      gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 76, 0, G_TX_LOADTILE, 0, 0,
                 0, 0, 0, 0, 0);
      gDPLoadSync(gfx++);
      gDPLoadTile(gfx++, G_TX_LOADTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                  8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                  (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 76, 0, G_TX_RENDERTILE, 0,
                 0, 0, 0, 0, 0, 0);
      gDPSetTileSize(gfx++, G_TX_RENDERTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                     8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                     (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++, 8 << G_TEXTURE_IMAGE_FRAC, (8 + i * 6) << G_TEXTURE_IMAGE_FRAC,
          311 << G_TEXTURE_IMAGE_FRAC,
          (8 + i * 6 + lines - 1) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
          8 << 5, 8 << 5, 4 << 10, 1 << 10);
      gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                     OS_K0_TO_PHYSICAL(nuGfxCfb_ptr) & ~7);
  }

  *gfxp = gfx;
}

/* Asset block for the mode-3 scrolling pattern: 16-entry TLUT at +8, the
 * 64x64 4-bit CI texels at +0x28. */
extern void* D_800FF4F0;
/* Low byte of D_800C7310: the effect-2 grey level. */
extern u8 D_800C7313;
extern s32 D_800C7314;

/**
 * Emits the screen-transition overlay for the current effect id (D_800C730C).
 *
 * The shared preamble puts the RDP in 1-cycle MODULATEI_PRIM, so every variant
 * below tints its texture with the primitive colour it sets. The fade level in
 * D_800C7310 is stepped down 0x20 per call until it reaches 0x80.
 *
 * Effect 3 tiles a 64x64 4-bit CI pattern over the play area, scrolling it 32
 * texels per call off the D_800C7314 tick, then puts the cycle type and TLUT
 * back. Effects 0 and 2 replay the captured frame at D_800C5EE0 as RGBA16 --
 * white for 0, grey (D_800C7313) for 2. Any other effect replays the same
 * buffer read as IA16 under an orange primitive colour.
 *
 * The replay loops copy 6 scanlines per texture rectangle over 38 strips, the
 * last one being the 224-line remainder. `lines` is declared inside the loop
 * body on purpose: at function scope the two loops share one pseudo whose live
 * range spans both, which makes it a global allocno and costs an extra
 * callee-saved register; block scope keeps it a short local quantity in $v0.
 * D_800C5EE0 is read inside the loop because the display-list stores may alias
 * it, so the load cannot be hoisted.
 */
void emit_screen_transition_overlay(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  s32 i;
  s32 scroll;
  u32 asset;

  if (D_800C7310 > 0x80) {
    D_800C7310 -= 0x20;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);

  if (D_800C730C == 3) {
    scroll = D_800C7314 + 1;
    D_800C7314 = scroll;
    scroll *= 32;
    asset = (u32)D_800FF4F0;

    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, (asset + 0x28) & ~7, G_IM_FMT_CI, 64, 64, 0,
                           0, 0, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, (asset + 8) & ~7);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, 8 << G_TEXTURE_IMAGE_FRAC,
                        8 << G_TEXTURE_IMAGE_FRAC, 312 << G_TEXTURE_IMAGE_FRAC,
                        232 << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE, scroll,
                        scroll, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
  } else if (D_800C730C == 0 || D_800C730C == 2) {
    if (D_800C730C == 0) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, D_800C7313, D_800C7313, D_800C7313, 255);
    }

    for (i = 0; i != 38; i++) {
      s32 lines = (i != 37) ? 6 : 2;

      gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                         ((u32)D_800C5EE0 + i * (320 * 2 * 6)) & ~7);
      gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 76, 0, G_TX_LOADTILE, 0, 0,
                 0, 0, 0, 0, 0);
      gDPLoadSync(gfx++);
      gDPLoadTile(gfx++, G_TX_LOADTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                  8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                  (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 76, 0, G_TX_RENDERTILE, 0,
                 0, 0, 0, 0, 0, 0);
      gDPSetTileSize(gfx++, G_TX_RENDERTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                     8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                     (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++, 8 << G_TEXTURE_IMAGE_FRAC, (8 + i * 6) << G_TEXTURE_IMAGE_FRAC,
          312 << G_TEXTURE_IMAGE_FRAC,
          (8 + i * 6 + lines) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE, 8 << 5,
          8 << 5, 1 << 10, 1 << 10);
      gDPPipeSync(gfx++);
    }
  } else {
    gDPSetPrimColor(gfx++, 0, 0, 0x90, 0x68, 0x20, 255);

    for (i = 0; i != 38; i++) {
      s32 lines = (i != 37) ? 6 : 2;

      gDPSetTextureImage(gfx++, G_IM_FMT_IA, G_IM_SIZ_16b, 320,
                         ((u32)D_800C5EE0 + i * (320 * 2 * 6)) & ~7);
      gDPSetTile(gfx++, G_IM_FMT_IA, G_IM_SIZ_16b, 76, 0, G_TX_LOADTILE, 0, 0,
                 0, 0, 0, 0, 0);
      gDPLoadSync(gfx++);
      gDPLoadTile(gfx++, G_TX_LOADTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                  8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                  (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gDPSetTile(gfx++, G_IM_FMT_IA, G_IM_SIZ_16b, 76, 0, G_TX_RENDERTILE, 0, 0,
                 0, 0, 0, 0, 0);
      gDPSetTileSize(gfx++, G_TX_RENDERTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                     8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                     (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++, 8 << G_TEXTURE_IMAGE_FRAC, (8 + i * 6) << G_TEXTURE_IMAGE_FRAC,
          312 << G_TEXTURE_IMAGE_FRAC,
          (8 + i * 6 + lines) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE, 8 << 5,
          8 << 5, 1 << 10, 1 << 10);
      gDPPipeSync(gfx++);
    }
  }

  *gfxp = gfx;
}

/* M_PI/2 and (M_PI/2)/255: the panel tilt sweeps a quarter turn as the
 * D_800C5EE4 countdown runs from 255 down to 0. */
extern const f64 D_800D1E38;
extern const f64 D_800D1E40;

extern u32 sky_panel_bank_index;
extern u16 sky_panel_persp_norm;

/* Projection matrix, and the double-buffered bank of 35 per-panel modelview
 * matrices (bank stride 35 * sizeof(Mtx) == 2240). */
extern Mtx D_800FE2F0;
extern Mtx D_800E3A50[];

/* Two unit quads: D_800C7358 is the wider one used by the rightmost column. */
extern Vtx D_800C7318[];
extern Vtx D_800C7358[];

extern void func_80065A1C(f32* mf, f32* angles, f32* pos);
extern void convert_and_pack_floats_to_fixed(f32* mf, Mtx* mtx);

/**
 * Replays the captured frame at `D_800C5EE0` as a 7x5 grid of 64x32 RGBA16
 * panels floating in 3D, each with its own modelview matrix, so the snapshot
 * peels away from the camera as `D_800C5EE4` counts down by ten per call.
 *
 * The 35 panels tile the 320x224 capture exactly (35 * 64 * 32 * 2 bytes), and
 * `idx` walks it 0x400 texels (one panel) at a time. Panel (i, j) sits at
 * x = (d + 0x40) * (j - 2) + 8, y = -(d + 0x20) * (i - 3), z = -290 - 2 * d,
 * where `d = 255 - D_800C5EE4` grows as the effect runs: the grid both spreads
 * apart and recedes. All 35 panels share one tilt built from `angles`.
 *
 * The `sinf`/`cosf` pair is dead -- the results are discarded -- but the calls
 * are what force `D_800C5EE4` to be re-read for `angles[1]`.
 *
 * Three spellings below are load-bearing:
 *   - `mfp` holds `mf` in a callee-saved register across both calls; using the
 *     array name directly rematerialises `sp + 0x30` at each call site.
 *   - `bank_off` is a statement of its own so the bank load and its multiply
 *     are emitted before the `&D_800E3A50[...]` address, which is what lets the
 *     two share one scratch register.
 *   - `idx += 0x400` sits between the texture load and the triangle pair; at
 *     the end of the body the gSP2Triangles command word schedules ahead of the
 *     `idx * 4` shift instead of behind it.
 */
void emit_snapshot_panel_grid_dl(void) {
  f32 angles[3];
  f32 pos[3];
  f32 mf[16];
  f32* mfp;
  Mtx* m;
  u32 bank_off;
  s32 n;
  s32 i;
  s32 j;
  s32 d;
  s32 idx;
  f32 ang;

  n = D_800C5EE4;
  if (n <= 0) {
    return;
  }

  ang = (f32)(D_800D1E40 - n * D_800D1E38) + 0.2f;
  sinf(ang);
  cosf(ang);

  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetCycleType(glistp++, G_CYC_1CYCLE);
  gDPPipeSync(glistp++);
  gSPClearGeometryMode(glistp++, 0xFFFFFF);
  gSPSetGeometryMode(glistp++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
  gDPPipeSync(glistp++);
  gDPSetRenderMode(glistp++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gSPPerspNormalize(glistp++, sky_panel_persp_norm);
  gDPPipeSync(glistp++);
  gDPSetTexturePersp(glistp++, G_TP_PERSP);
  gDPSetCombineMode(glistp++, G_CC_DECALRGB, G_CC_DECALRGB);
  gDPPipeSync(glistp++);
  gDPSetTextureFilter(glistp++, G_TF_BILERP);
  gSPMatrix(glistp++, OS_K0_TO_PHYSICAL(&D_800FE2F0),
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gDPPipeSync(glistp++);

  angles[0] = 0.0f;
  angles[1] = (f32)(D_800D1E40 - D_800C5EE4 * D_800D1E38);
  angles[2] = angles[1];

  idx = 0;
  for (i = 0; i != 7; i++) {
    mfp = mf;
    for (j = 0; j != 5; j++) {
      d = 0xFF - D_800C5EE4;
      pos[0] = (f32)((d + 0x40) * (j - 2) + 8);
      pos[1] = (f32)(-(d + 0x20) * (i - 3));
      pos[2] = -290.0f - (f32)(d * 2);
      func_80065A1C(mfp, angles, pos);

      bank_off = sky_panel_bank_index * 2240;
      m = &D_800E3A50[i * 5 + j];
      convert_and_pack_floats_to_fixed(mfp, (Mtx*)(bank_off + (u32)m));
      gSPMatrix(glistp++,
                OS_K0_TO_PHYSICAL(sky_panel_bank_index * 2240 + (u32)m),
                G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
      if (j == 4) {
        gSPVertex(glistp++, D_800C7358, 4, 0);
      } else {
        gSPVertex(glistp++, D_800C7318, 4, 0);
      }
      gDPLoadTextureBlock(glistp++, ((u32)D_800C5EE0 + idx * 4) & ~7,
                          G_IM_FMT_RGBA, G_IM_SIZ_16b, 64, 32, 0, 0, 0, 0, 0, 0,
                          0);
      idx += 0x400;
      gDPPipeSync(glistp++);
      gSP2Triangles(glistp++, 0, 1, 2, 0, 0, 2, 3, 0);
      gDPPipeSync(glistp++);
    }
  }

  gDPPipeSync(glistp++);
  D_800C5EE4 -= 10;
}

/* Sky-panel snapshot emitter, sibling of emit_snapshot_panel_grid_dl above.
 * S301 carry: reconstructed to 618/618 instructions with an exact -0x98 frame
 * and six of seven global registers matching. The seventh is a quantified
 * loop.c:1631 movable-selection coin: threshold 122 stepping down by 3 against
 * an inner-loop insn_count of 115 admits exactly three life-1 invariants, and
 * the constant 4 sits behind the gDPSetTile word 0xF5100000 in the movable
 * list. Full arithmetic, dump table and working body in
 * docs/wip/func_800947A8.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_800947A8);
INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_80095150);

extern void play_sound_effect(s32 sfx, s32 arg1, s32 arg2);

/**
 * Replays the captured frame at `D_800C5EE0` as a grid of 80x15 RGBA16 tiles,
 * one more band of eight tiles per step as `D_800C5EE4` counts down from 0x40,
 * so the snapshot fills the screen a band at a time.
 *
 * Each outer step emits one 80-pixel column of a 120-pixel band: `col` walks
 * 0..3 across the 320-pixel screen and `row` advances a band every fourth step,
 * so the eight inner tiles stack 15 scanlines apart. `idx` is the running
 * RGBA16 offset into the snapshot, 600 words (80 * 15 texels) per tile. A sound
 * effect fires on every eighth step.
 *
 * Three spellings below are load-bearing for the loop.c invariant-hoist split
 * (the `threshold -= 3` decay at loop.c:1719 gating loop.c:1631), which decides
 * which of the twelve display-list constants leave the inner loop. The ROM
 * moves exactly nine, leaving `0xF2000000`, `0x0013C038` and `0x04000400`
 * behind:
 *   - `col4` is a separate statement so exactly one invariant insn precedes the
 *     display-list constants; a two-insn `col * 5` prefix costs three more
 *     threshold and strands `gDPSetTile`'s `0xF5102800` in the inner loop.
 *   - the two x coordinates each re-shift `(col4 + col)` instead of sharing a
 *     `col * 320` temp, which is what makes combine emit the ROM's two separate
 *     `sll ..., 6` instructions.
 *   - `row` is a plain counter scaled by 120 at use, not a stepped `ybase`
 *     accumulator: that makes the band base a loop.c induction variable, so its
 *     zero-init is emitted into the loop preheader after the hoisted constants
 *     rather than ahead of them.
 */
void emit_snapshot_tile_wipe_dl(void) {
  s32 n;
  s32 rem;
  s32 count;
  s32 i;
  s32 j;
  s32 col;
  s32 idx;
  s32 row;

  n = D_800C5EE4;
  if (n <= 0) {
    return;
  }

  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetCycleType(glistp++, G_CYC_1CYCLE);
  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetTexturePersp(glistp++, G_TP_NONE);
  gDPSetCombineMode(glistp++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
  gDPPipeSync(glistp++);

  col = 0;
  idx = 0;

  rem = 0x40 - n;
  count = rem / 8 + 1;
  if ((rem & 7) == 0) {
    play_sound_effect(0x45, 0xD, 0x7F);
  }

  row = 0;
  for (i = 0; i != count; i++) {
    s32 ybase = row * 120;

    for (j = 0; j != 8; j++) {
      s32 col4 = col * 4;
      s32 y = ybase + j * 15;

      gDPLoadTextureBlock(glistp++, ((u32)D_800C5EE0 + idx * 4) & ~7,
                          G_IM_FMT_RGBA, G_IM_SIZ_16b, 80, 15, 0, 0, 0, 0, 0, 0,
                          0);
      gDPPipeSync(glistp++);
      gSPTextureRectangle(glistp++, ((col4 + col) << 4) << G_TEXTURE_IMAGE_FRAC,
                          y << G_TEXTURE_IMAGE_FRAC,
                          (((col4 + col) << 4) + 80) << G_TEXTURE_IMAGE_FRAC,
                          (y + 15) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE, 0,
                          0, 1 << 10, 1 << 10);
      gDPPipeSync(glistp++);
      idx += 600;
    }

    col++;
    if (col == 4) {
      col = 0;
      row++;
    }
  }

  D_800C5EE4--;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_800957F0);

/* gsDPPipeSync + gsDPSetScissor emitter for the full 320x240 (0x140 x 0xF0)
 * screen. Coords come from a persistent 4x s16 rect at D_801B7F30; the scissor
 * macro does the *4.0f 10.2 fixed-point conversion. */
void emit_fullscreen_scissor_dl(void) {
  D_801B7F30.ulx = 0;
  D_801B7F30.lrx = 0x140;
  D_801B7F30.lry = 0xF0;
  D_801B7F30.uly = 0;
  gDPPipeSync(glistp++);
  gDPSetScissor(glistp++, G_SC_NON_INTERLACE, D_801B7F30.ulx, D_801B7F30.uly,
                D_801B7F30.lrx, D_801B7F30.lry);
}

s32 func_800959F8(void) { return D_800C5EE4 < 1; }
