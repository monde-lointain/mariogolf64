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

/* Sun / lens-flare emitter over a Gfx** parameter. S301 carry: reconstructed to
 * 634/634 instructions with an exact -0x110 frame and only 12 differing
 * instructions -- one four-instruction scheduling rotation in the address
 * setup, and four reload1.c spill-slot pair swaps. Every DL command word was
 * verified against gbi.h. Full body, resolved macro arguments and the measured
 * lever table (the decisive one: subscripting D_800C5F24/D_800C5F2C so the
 * loads conflict with the Gfx stores) are in
 * docs/wip/func_8008D3F4.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008D3F4);
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

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008E82C);

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
