#include "common.h"
#include <nusys.h>

extern s32 D_800BB03C;
extern s32 D_800B7770;
extern s32 D_800B7774;
extern s32 D_800B7778;
extern s32 D_800B777C;
extern s32 D_800B680C;
extern s32 D_800B6810;
extern s32 D_800B6818;
extern s32 D_801B552C;
extern s32 game_mode;
extern s32 D_800BB038;

extern s32 D_800BB020;
extern s32 D_800BB024;
extern s32 D_800BB02C;
extern s32 D_800BB030;
extern s32 D_800BB034;
extern s32 D_800B7768;
extern s32 D_800B776C;

extern s32 overlay_id_from_segment(s32);
extern s32 can_load_overlay(s32);
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

void init_rdp_and_draw_sky_background(Gfx** gfxp, s32 view_state_address);
void func_8002A9C4(Gfx** gfxp, s32 view_state_address);
void emit_sky_horizon_compositor_dl(Gfx** gfxp);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002A640);

void func_8002A90C(Mtx* matrix, f32 translate_x, f32 translate_y, f32 translate_z) {
  Mtx translation_matrix;
  guTranslate(&translation_matrix, translate_x, translate_y, translate_z);
  guMtxCatL(matrix, &translation_matrix, matrix);
}

void func_8002A944(Mtx* matrix, f32 scale_x, f32 scale_y, f32 scale_z) {
  Mtx scale_matrix;
  guScale(&scale_matrix, scale_x, scale_y, scale_z);
  guMtxCatL(matrix, &scale_matrix, matrix);
}

void func_8002A97C(s32 sky_mode, s32 blend_countdown) {
  D_800B680C = sky_mode;
  D_800B6810 = blend_countdown;
  if (sky_mode == 4) {
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
extern u16* g_auxiliary_framebuffer;

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

      gDPSetTextureImage(
          gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
          ((u32)g_auxiliary_framebuffer + i * (320 * 2 * 6)) & ~7);
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

typedef struct {
  /* 0x000 */ u8 unknown_000[0x1C0];
  /* 0x1C0 */ Mtx projection_matrix;
} ViewMatrixBlock; /* 0x200 */

void setup_view_by_camera_mode(ViewMatrixBlock* matrix_block, s32 view_state_address) {
  if (D_800B680C == 0) {
    init_rdp_and_draw_sky_background(&glistp, view_state_address);
  } else {
    if (D_800B680C == 2) {
      func_8002A9C4(&glistp, view_state_address);
      return;
    }
    if (D_800B680C != 1) {
      /* Preserve the retail branch around projection setup. */
      goto check3;
    }
    func_8002BD9C(&glistp);
  }

  gSPPopMatrix(glistp++, G_MTX_MODELVIEW);
  gSPMatrix(glistp++, &matrix_block->projection_matrix,
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

extern s32 render_buffer_index;
extern Gfx D_801B94A0[2][0x36B0];
extern u32 D_800B7738;
extern void append_viewport_setup(s32, Gfx**);
extern void append_framebuffer_clear(s32, Gfx**);

void func_800318A8(void) {
  Gfx* display_list = D_801B94A0[render_buffer_index];
  Gfx* display_list_start = display_list;
  s32 iteration = 0;

  /* Preserve the matched one-iteration loop shape. */
  do {
    append_viewport_setup(render_buffer_index, &display_list);
    append_framebuffer_clear(0, &display_list);
    gDPPipeSync(display_list++);
    gDPSetScissor(display_list++, G_SC_NON_INTERLACE, 0, 0, 320, 240);
    copy_previous_frame_to_cfb(&display_list);
    gDPPipeSync(display_list++);
    gDPPipeSync(display_list++);
    gDPSetCycleType(display_list++, G_CYC_1CYCLE);
    gDPPipeSync(display_list++);
    gDPSetPrimColor(display_list++, 0, 0, 0, 0, 16, 64);
    gDPSetCombineLERP(display_list++, CENTER, K5, PRIMITIVE, COMBINED_ALPHA, 0, 0,
                      0, PRIMITIVE, CENTER, K5, PRIMITIVE, COMBINED_ALPHA, 0, 0,
                      0, PRIMITIVE);
    gDPPipeSync(display_list++);
    gDPSetRenderMode(display_list++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTextureRectangle(display_list++, 0, 0, 320 << 2, 240 << 2, G_TX_RENDERTILE,
                        0, 0, 1 << 10, 1 << 10);
    gDPFullSync(display_list++);
    gSPEndDisplayList(display_list++);
    nuGfxTaskStart(display_list_start,
                   (display_list - display_list_start) * sizeof(Gfx), 0, 1);
    D_800B7738 = (osGetTime() << 6) / 3000;
    iteration++;
  } while (iteration != 1);
  render_buffer_index ^= 1;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80031AF4);

void func_80032520(void) {
  if (D_800BB020 == 0x19) {
    if (D_800B7768 == 0) {
      D_800B7768 = 1;
      unload_overlay(overlay_id_from_segment(D_800BB02C));
    } else if (can_load_overlay(overlay_id_from_segment(D_800BB030)) != 0) {
      load_overlay(overlay_id_from_segment(D_800BB030));
      D_800B7768 = 0;
      D_800BB020 = D_800BB024;
    }
  }
  if (D_800BB03C == 1) {
    if (D_800B776C == 0) {
      D_800B776C = 1;
      unload_overlay(overlay_id_from_segment(D_800BB034));
    } else if (can_load_overlay(overlay_id_from_segment(D_800BB038)) != 0) {
      load_overlay(overlay_id_from_segment(D_800BB038));
      D_800BB03C = 0;
      D_800B776C = 0;
      D_800BB034 = D_800BB038;
    }
  }
}

s32 func_80032658(void) { return D_800BB03C == 0; }

void func_80032668(void) {
  u32 current_game_mode = game_mode;
  s32 target_overlay_segment;

  D_800BB03C = 1;
  switch (current_game_mode) {
    case 3:
    case 9:
      target_overlay_segment = 0x15;
      break;
    case 11:
      target_overlay_segment = 0x1A;
      break;
    default:
      target_overlay_segment = 5;
      break;
  }
  D_800BB038 = target_overlay_segment;
}

void unload_active_overlay(void) {
  unload_overlay(overlay_id_from_segment(D_800BB034));
  D_800BB03C = -1;
}

void func_800326FC(s32 origin_x, s32 origin_y, s32 horizontal_offset_per_level,
                   s32 vertical_offset_per_level) {
  D_800B7778 = horizontal_offset_per_level;
  D_800B777C = vertical_offset_per_level;
  D_800B7770 = origin_x;
  D_800B7774 = origin_y;
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
  s32* meter_state_pointer = &D_801B7F70;
  s32 meter_state = *meter_state_pointer;
  s32 horizontal_offset;
  s32 vertical_offset;
  s32 horizontal_offset_eighths;

  if (meter_state == 1) {
    if (putting_meter_level == meter_state) {
      func_80050DA0(0x64, D_800C0E60, 0x3F, 0x1E, 0x7F);
    }
    D_801B557C = meter_state;
    putting_meter_level = putting_meter_level + 1;
    if (putting_meter_level >= 8) {
      putting_meter_level = 8;
      *meter_state_pointer = 0;
    }
  } else if (meter_state == 2) {
    if (putting_meter_level == 7) {
      func_80050DA0(0x65, D_800C0E60, 0x3F, 0x1E, 0x7F);
    }
    putting_meter_level = putting_meter_level - 1;
    if (putting_meter_level <= 0) {
      putting_meter_level = 0;
      *meter_state_pointer = 0;
      D_801B557C = 0;
    }
  }

  horizontal_offset = putting_meter_level * D_800B7778;
  horizontal_offset_eighths = horizontal_offset / 8;
  vertical_offset = putting_meter_level * D_800B777C;
  D_801B5560 = horizontal_offset_eighths;
  D_801B5564 = vertical_offset / 8;
  D_801B5558 = D_800B7770 - horizontal_offset / 16;
  D_801B555C = D_800B7774 - vertical_offset / 16;
  D_801B5578 = 0.9f;
}
