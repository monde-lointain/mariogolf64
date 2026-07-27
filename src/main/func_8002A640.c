#include "common.h"

extern s32 D_800BB03C;
extern s32 D_800B7770;
extern s32 D_800B7774;
extern s32 D_800B7778;
extern s32 D_800B777C;
extern s32 D_800B680C;
extern s32 D_800B6810;
extern s32 D_800B6818;
extern s32 D_801B552C;
extern s32 D_801B608C;
extern s32 D_800BB038;

extern s32 D_800BB020;
extern s32 D_800BB024;
extern s32 D_800BB02C;
extern s32 D_800BB030;
extern s32 D_800BB034;
extern s32 D_800B7768;
extern s32 D_800B776C;

extern s32 func_80025D78(s32);
extern s32 func_80025F18(s32);
extern s32 load_overlay(s32);
extern void unload_overlay(s32);

extern Gfx* glistp;
extern u16* nuGfxCfb_ptr;

/* Source framebuffer for the re-blit below: seeded from the VI on the first
 * call, then re-pointed each frame at the buffer the previous call drew into.
 */
extern u32 D_800B7750;
/* The cfb the current call is drawing into, saved for the next call to read. */
extern u32 D_800B7754;

void init_rdp_and_draw_sky_background(Gfx** gfxp, s32 arg1);
void func_8002A9C4(Gfx** gfxp, s32 arg1);
void emit_sky_horizon_compositor_dl(Gfx** gfxp);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002A640);

void func_8002A90C(Mtx* arg0, f32 arg1, f32 arg2, f32 arg3) {
  Mtx sp10;
  guTranslate(&sp10, arg1, arg2, arg3);
  guMtxCatL(arg0, &sp10, arg0);
}

void func_8002A944(Mtx* arg0, f32 arg1, f32 arg2, f32 arg3) {
  Mtx sp10;
  guScale(&sp10, arg1, arg2, arg3);
  guMtxCatL(arg0, &sp10, arg0);
}

void func_8002A97C(s32 arg0, s32 arg1) {
  D_800B680C = arg0;
  D_800B6810 = arg1;
  if (arg0 == 4) {
    D_801B552C = 1;
    D_800B6818 = -0x280;
  } else {
    D_801B552C = 3;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002A9C4);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", emit_sky_dome_dl);

extern u16* nuGfxZBuffer;
/* Sky layer, blitted into the z-buffer scratch surface. */
extern u32 D_800B6814;
/* Horizon layer, blitted over the sky into the current colour buffer. */
extern u16* D_800B67A0;

/**
 * Composites the two sky layers in copy-cycle mode. Each pass re-blits the
 * 304x224 region at (8, 8), six scanlines per texrect: the first into the
 * z-buffer used as a scratch surface, the second into the current cfb.
 */
void emit_sky_horizon_compositor_dl(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  s32 i;

  D_800B6810 = 1;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, 0, 0);
  gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_COPY);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                   (u32)nuGfxZBuffer & ~7);
  gDPPipeSync(gfx++);

  /* `i` stays at function scope because the ROM keeps it in $t8 across both
   * loops (one pseudo); `lines` is re-declared per loop so its live range dies
   * between them. Splitting both, or neither, permutes the register set. */
  {
    s32 lines;

    for (i = 0; i != 38; i++) {
      /* The last strip is the 224-line remainder: 37 * 6 + 2. */
      lines = (i != 37) ? 6 : 2;

      gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                         (D_800B6814 + i * (320 * 2 * 6)) & ~7);
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
  }

  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                   osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
  gDPPipeSync(gfx++);

  {
    s32 lines;

    for (i = 0; i != 38; i++) {
      lines = (i != 37) ? 6 : 2;

      gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                         ((u32)D_800B67A0 + i * (320 * 2 * 6)) & ~7);
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
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);

  *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640",
            init_rdp_and_draw_sky_background);

void func_8002BD9C(Gfx** gfxp) {}

void setup_view_by_camera_mode(u8* arg0, s32 arg1) {
  if (D_800B680C == 0) {
    init_rdp_and_draw_sky_background(&glistp, arg1);
  } else {
    if (D_800B680C == 2) {
      func_8002A9C4(&glistp, arg1);
      return;
    }
    if (D_800B680C != 1) {
      goto check3;
    }
    func_8002BD9C(&glistp);
  }
  gSPPopMatrix(glistp++, G_MTX_MODELVIEW);
  gSPMatrix(glistp++, (Mtx*)(arg0 + 0x1C0),
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  return;
check3:
  if (D_800B680C == 3) {
    emit_sky_horizon_compositor_dl(&glistp);
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002BE78);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", draw_ground_shadow_decals);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002CDA8);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002DAC0);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", render_frame);

/**
 * Re-blits the previous frame into the current colour buffer: the 304x224
 * region at (8, 8) is copied in copy-cycle mode, six scanlines per texrect.
 */
void copy_previous_frame_to_cfb(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  s32 i;
  s32 lines;

  if (D_800B7750 == 0) {
    D_800B7750 = (u32)osViGetNextFramebuffer();
  } else {
    D_800B7750 = D_800B7754;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, 0, 0);
  gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_COPY);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  D_800B7754 = (u32)nuGfxCfb_ptr;
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                   osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
  gDPPipeSync(gfx++);

  for (i = 0; i != 38; i++) {
    /* The last strip is the 224-line remainder: 37 * 6 + 2. */
    lines = (i != 37) ? 6 : 2;

    gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                       (D_800B7750 + i * (320 * 2 * 6)) & ~7);
    gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 76, 0, G_TX_LOADTILE, 0, 0,
               0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadTile(gfx++, G_TX_LOADTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 76, 0, G_TX_RENDERTILE, 0, 0,
               0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 8 << G_TEXTURE_IMAGE_FRAC,
                   8 << G_TEXTURE_IMAGE_FRAC, 311 << G_TEXTURE_IMAGE_FRAC,
                   (8 + lines - 1) << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, 8 << G_TEXTURE_IMAGE_FRAC,
                            (8 + i * 6) << G_TEXTURE_IMAGE_FRAC,
                            311 << G_TEXTURE_IMAGE_FRAC,
                            (8 + i * 6 + lines - 1) << G_TEXTURE_IMAGE_FRAC,
                            G_TX_RENDERTILE, 8 << 5, 8 << 5, 4 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);

  *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_800318A8);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80031AF4);

void func_80032520(void) {
  if (D_800BB020 == 0x19) {
    if (D_800B7768 == 0) {
      D_800B7768 = 1;
      unload_overlay(func_80025D78(D_800BB02C));
    } else if (func_80025F18(func_80025D78(D_800BB030)) != 0) {
      load_overlay(func_80025D78(D_800BB030));
      D_800B7768 = 0;
      D_800BB020 = D_800BB024;
    }
  }
  if (D_800BB03C == 1) {
    if (D_800B776C == 0) {
      D_800B776C = 1;
      unload_overlay(func_80025D78(D_800BB034));
    } else if (func_80025F18(func_80025D78(D_800BB038)) != 0) {
      load_overlay(func_80025D78(D_800BB038));
      D_800BB03C = 0;
      D_800B776C = 0;
      D_800BB034 = D_800BB038;
    }
  }
}

s32 func_80032658(void) { return D_800BB03C == 0; }

void func_80032668(void) {
  u32 v = D_801B608C;
  s32 r;

  D_800BB03C = 1;
  switch (v) {
    case 3:
    case 9:
      r = 0x15;
      break;
    case 11:
      r = 0x1A;
      break;
    default:
      r = 5;
      break;
  }
  D_800BB038 = r;
}

void unload_active_overlay(void) {
  unload_overlay(func_80025D78(D_800BB034));
  D_800BB03C = -1;
}

void func_800326FC(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
  D_800B7778 = arg2;
  D_800B777C = arg3;
  D_800B7770 = arg0;
  D_800B7774 = arg1;
}

extern s32 putting_meter_level;
extern s32 D_800C0E60;
extern s32 D_801B7F70;
extern s32 D_801B557C;
extern s32 D_801B5558;
extern s32 D_801B5560;
extern s32 D_801B5564;
extern s32 D_801B555C;
extern f32 D_801B5578;
extern void func_80050DA0(s32, s32, s32, s32, s32);

void update_putting_meter(void) {
  s32* mode = &D_801B7F70;
  s32 state = *mode;
  s32 x;
  s32 y;
  s32 x8;

  if (state == 1) {
    if (putting_meter_level == state) {
      func_80050DA0(0x64, D_800C0E60, 0x3F, 0x1E, 0x7F);
    }
    D_801B557C = state;
    putting_meter_level = putting_meter_level + 1;
    if (putting_meter_level >= 8) {
      putting_meter_level = 8;
      *mode = 0;
    }
  } else if (state == 2) {
    if (putting_meter_level == 7) {
      func_80050DA0(0x65, D_800C0E60, 0x3F, 0x1E, 0x7F);
    }
    putting_meter_level = putting_meter_level - 1;
    if (putting_meter_level <= 0) {
      putting_meter_level = 0;
      *mode = 0;
      D_801B557C = 0;
    }
  }

  x = putting_meter_level * D_800B7778;
  x8 = x / 8;
  y = putting_meter_level * D_800B777C;
  D_801B5560 = x8;
  D_801B5564 = y / 8;
  D_801B5558 = D_800B7770 - x / 16;
  D_801B555C = D_800B7774 - y / 16;
  D_801B5578 = 0.9f;
}
