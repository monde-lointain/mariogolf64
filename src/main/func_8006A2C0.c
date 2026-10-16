#include "common.h"
#include "controller.h"
#include "session.h"
#include "club.h"
#include "rom_asset.h"

/* D_801B71F3 is the record base; status, handicap, hole scores, and the
 * second byte of each hole slot alias offsets +3, +6, +8, and +9. */

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
/* The dialog renderer displays this text and maps its two choices to results. */
typedef struct {
  /* 0x00 */ s32 text_run_index;
  /* 0x04 */ s32 initial_choice_index;
  /* 0x08 */ s32 choice_results[2];
} DialogDefinition; /* 0x10 */

extern DialogDefinition D_800C406C[];
extern s32 D_800C411C;
extern s32 D_800C4120;
extern s32 D_800C4124;
extern s32 D_800C4128;
extern s32 D_800C4144;
extern void play_sound_effect(s32 sfx, s32 arg1, s32 arg2);
extern void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags,
                          u8 red, u8 green, u8 blue, u8 alpha);
extern s32 D_800FF4B0;
extern s32 D_800FF4B4;
extern s32 D_800FF4B8;
extern s32 scenario_mode_id;
extern u8* func_8005AF50(void);
extern void* D_800E1C00;
extern void* D_800E1C04;
extern void* D_800E1C08;
extern void* D_800E1C0C;

extern void* heap3_alloc(u32 need);
extern void heap3_free(void** payload_ptr);
extern void func_80050DA0(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
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

/**
 * Loads one 4-bit (CI4) texture block into TMEM through the caller's
 * display-list cursor. The whole body is a single gDPLoadTextureBlock_4b
 * expansion: SetTextureImage, SetTile, LoadSync, LoadBlock, PipeSync, SetTile,
 * SetTileSize.
 *
 * The image address is masked to an 8-byte boundary, matching the sibling TLUT
 * loaders above.
 */
void load_texture_block_4b(Gfx** pgfx, u32 timg, s32 fmt, s32 width, s32 height,
                           s32 pal, s32 cms, s32 cmt, s32 masks, s32 maskt,
                           s32 shifts, s32 shiftt) {
  Gfx* gfx = *pgfx;

  gDPLoadTextureBlock_4b(gfx++, timg & ~7, fmt, width, height, pal, cms, cmt,
                         masks, maskt, shifts, shiftt);
  *pgfx = gfx;
}

/**
 * Loads one 8-bit (CI8) or 16-bit (RGBA16) texture block into TMEM through the
 * caller's display-list cursor.
 *
 * gDPLoadTextureBlock cannot be used directly here: it token-pastes its `siz`
 * argument to reach the per-size load-block constants, so `siz` has to be a
 * literal. The block size is a run-time parameter, so the macro body is spelled
 * out once per supported size and only the load-block sizing constants differ
 * between the two arms. The render-tile gDPSetTile still takes the run-time
 * `siz`, which is why its size field is computed rather than folded.
 *
 * The image address is masked to an 8-byte boundary, matching the sibling
 * loaders above.
 */
void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz, s32 width,
                        s32 height, s32 pal, s32 cms, s32 cmt, s32 masks,
                        s32 maskt, s32 shifts, s32 shiftt) {
  Gfx* gfx = *pgfx;

  if (siz == G_IM_SIZ_8b) {
    gDPSetTextureImage(gfx++, fmt, G_IM_SIZ_8b_LOAD_BLOCK, 1, timg & ~7);
    gDPSetTile(gfx++, fmt, G_IM_SIZ_8b_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0, cmt,
               maskt, shiftt, cms, masks, shifts);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0,
                 ((width * height + G_IM_SIZ_8b_INCR) >> G_IM_SIZ_8b_SHIFT) - 1,
                 CALC_DXT(width, G_IM_SIZ_8b_BYTES));
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, fmt, siz, ((width * G_IM_SIZ_8b_LINE_BYTES) + 7) >> 3, 0,
               G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0,
                   (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
  } else {
    gDPSetTextureImage(gfx++, fmt, G_IM_SIZ_16b_LOAD_BLOCK, 1, timg & ~7);
    gDPSetTile(gfx++, fmt, G_IM_SIZ_16b_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0, cmt,
               maskt, shiftt, cms, masks, shifts);
    gDPLoadSync(gfx++);
    gDPLoadBlock(
        gfx++, G_TX_LOADTILE, 0, 0,
        ((width * height + G_IM_SIZ_16b_INCR) >> G_IM_SIZ_16b_SHIFT) - 1,
        CALC_DXT(width, G_IM_SIZ_16b_BYTES));
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, fmt, siz, ((width * G_IM_SIZ_16b_LINE_BYTES) + 7) >> 3, 0,
               G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0,
                   (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
  }
  *pgfx = gfx;
}

void func_8006ACD8(void) {
  RomAssetLoad asset;

  D_800E1C04 = heap3_alloc(get_rom_asset_allocation_size(0x63F, &asset));
  load_rom_asset_payload(D_800E1C04, &asset);
  func_8006B54C();
}

void func_8006AD1C(s32 table_index) {
  RomAssetLoad asset;

  get_rom_asset_allocation_size(*(u16*)&D_800C4026[(table_index % 14) * 4], &asset);
  load_rom_asset_payload(D_800E1C04, &asset);
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

/**
 * Emits the full-screen fade overlay.
 *
 * The overlay is a flat black primitive whose alpha ramps out over the second
 * half of the phase counter, masked by a scrolling 4-bit intensity texture so
 * the fade dissolves rather than dips uniformly. Mode 1 winds the phase back
 * down, modes 2 and 3 wind it up at one and two steps per frame; mode 0 emits
 * nothing. The phase reaching its ceiling clears the mode, ending the effect.
 *
 * Below two mask rows there is nothing to scroll, so that case degenerates to
 * a plain unmasked rectangle.
 */
void emit_screen_fade_overlay_dl(Gfx** pgfx) {
  Gfx* gfx = *pgfx;
  s32 mode = D_800C4020;
  s32 rows;
  s32 offset;
  s32 alpha;
  s32 scroll;
  f32 phase;

  if (mode == 0) {
    return;
  }

  if (mode == 1) {
    D_800C401C -= 1.0f;
    if (D_800C401C < 0.0f) {
      D_800C401C = 0.0f;
    }
  } else if (mode == 2 || mode == 3) {
    f32 v;

    if (mode == 3) {
      v = D_800C401C + 2.0f;
    } else {
      v = D_800C401C + 1.0f;
    }
    D_800C401C = v;
    if (v > 20.0f) {
      D_800C401C = 20.0f;
      D_800C4020 = 0;
    }
  }

  phase = D_800C401C;
  rows = phase * 38.0f;
  offset = (8.0f - phase) * 0.125f * -152.0f;
  if (phase < 10.0f) {
    alpha = 255;
  } else {
    alpha = (20.0f - phase) * 25.4f;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 255, 255, 255, 1);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, alpha);
  gDPPipeSync(gfx++);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
                    PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);

  if (rows < 2) {
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gSPTextureRectangle(gfx++, 32, 32, 1248, 928, 0, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  } else {
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_16b, 1,
                       ((u32)D_800E1C04 + 8) & ~7);
    gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
               G_TX_CLAMP, 0, 0, G_TX_CLAMP, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 1023, 512);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_4b, 4, 0, G_TX_RENDERTILE, 0,
               G_TX_CLAMP, 0, 0, G_TX_CLAMP, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 252, 252);
    gDPPipeSync(gfx++);
    scroll = (offset << 11) / rows;
    gSPScisTextureRectangle(gfx++, 32, -128, 1248, 1088, 0, scroll, scroll,
                            65536.0f / (f32)rows, 65536.0f / (f32)rows);
    gDPPipeSync(gfx++);
  }

  *pgfx = gfx;
}

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

void load_dialog_choice_assets(void) {
  RomAssetLoad asset;

  D_800E1C08 = heap3_alloc(get_rom_asset_allocation_size(0x70B, &asset));
  load_rom_asset_payload(D_800E1C08, &asset);
  D_800E1C0C = heap3_alloc(get_rom_asset_allocation_size(0x70C, &asset));
  load_rom_asset_payload(D_800E1C0C, &asset);
}

void free_dialog_choice_assets(void) {
  heap3_free(&D_800E1C08);
  heap3_free(&D_800E1C0C);
}

s32 func_8006BA10(void) { return !~D_800C4124; }

void func_8006BA24(s32 dialog_index) {
  DialogDefinition* dialog;
  s32 initial_choice_index;

  if (dialog_index == -1) {
    return;
  }
  dialog = &D_800C406C[dialog_index];
  D_800C4120 = 0;
  D_800C411C = dialog_index;
  load_dialog_choice_assets();
  initial_choice_index = dialog->initial_choice_index;
  D_800C4124 = 0;
  D_800C4128 = initial_choice_index;
  play_sound_effect(0x64, D_800C0E60, 0x64);
}

s32 func_8006BA94(ControllerState* input) {
  DialogDefinition* dialog = &D_800C406C[D_800C411C];
  s32 mode = D_800C4124;
  if (mode == 1) {
    func_800719A0(0xA0, 0x6C, dialog->text_run_index, 1,
                  0, 0x10, 0x5F, 0x3F, 0xFF, 0xFF);
    if (input->pressed == 0x8000) {
      s32 sound;
      s32 volume;
      D_800C4124 = 2;
      if ((u32)dialog->choice_results[D_800C4128] < 2) {
        sound = 0x3B;
        volume = 0xF;
      } else {
        volume = D_800C0E60;
        sound = 0x5D;
      }
      play_sound_effect(sound, volume, 0x64);
      return -1;
    }
    if (input->pressed == 0x4000) {
      D_800C4128 = 1;
      D_800C4124 = 2;
      play_sound_effect(0x5D, D_800C0E60, 0x64);
      return -1;
    }
    if (input->repeated & 0x300) {
      D_800C4128 ^= 1;
      play_sound_effect(0x5C, D_800C0E60, 0x64);
    }
  } else if (mode == 2) {
    D_800C4124 = 3;
    play_sound_effect(0x65, D_800C0E60, 0x64);
    func_800719A0(0, 0, -1, 1, 0, 1, 0, 0, 0, 0);
  } else if (mode == 4) {
    D_800C4124 = -1;
    free_dialog_choice_assets();
    return dialog->choice_results[D_800C4128];
  }
  return -1;
}

extern f32 D_800C412C;
extern const f64 D_800D1440; /* 20 degrees in radians */

extern void func_8007399C(s32 arg0, s32 arg1, void* out0, void* out1, s32 arg4);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);

/* The loop-top `yc = 0;` is a PLACEHOLDER for an unknown source statement, not
 * RE'd truth: the ROM's loop body carries one more RTL insn than the natural
 * spelling, and it dies in flow after loop, so it produces no instruction. Any
 * single dead assignment at the loop top reproduces the ROM exactly; which one
 * the original source wrote is not recoverable from the asm. See
 * docs/wip/func_8006BC80.near-match.md Finding 1. */
void emit_hole_banner_dl(Gfx** pgfx) {
  Gfx* gfx;
  Gfx** gp;
  DialogDefinition* entry;
  s32 text_width;
  s32 text_height;
  s32 span;
  s32 i;
  s32 off;
  s32 halfw;
  s32 hgt;
  s32 ulx;
  s32 yc;
  s32 ulx2;
  s32 uly2;
  s32 yc2;
  s32 yc3;
  s32 lrx2;
  s32 lry2;
  s32 dsdx2;
  s32 dtdy2;
  s32 y;

  gfx = *pgfx;
  entry = &D_800C406C[D_800C411C];
  if (D_800C4124 == -1) {
    return;
  }

  func_8007399C(entry->text_run_index, 0xC, &text_width, &text_height, 0);
  span = text_height + 0x18;
  if (span < 0x30) {
    span = 0x30;
  }

  if (D_800C4124 == 0) {
    D_800C4120++;
    if (D_800C4120 == 4) {
      D_800C4124 = 1;
    }
  } else if (D_800C4124 == 3) {
    D_800C4120--;
    if (D_800C4120 == 0) {
      D_800C4124 = 4;
      return;
    }
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPSetEnvColor(gfx++, 191, 191, 191, 255);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineMode(gfx++, G_CC_BLENDPEDECALA, G_CC_BLENDPEDECALA);

  i = 0;
  gp = &gfx;
  off = 8;
  for (; i != 2; i++) {
    yc = 0;
    if (D_800C4124 == 1) {
      hgt = span;
      halfw = 0x88;
    } else {
      s32 step = D_800C4120;

      halfw = step * 34;
      hgt = span * step / 4;
    }

    load_texture_block_4b(gp, (u32)D_800E1C08 + off, G_IM_FMT_I, 256, 32, 0, 0,
                          0, 0, 0, 0, 0);

    y = i * hgt + 0x74;
    func_8006A2C0(gp, (0xA0 - halfw) << 2, (y - hgt) << 2, (halfw + 0xA0) << 2,
                  y << 2, 0, 0, 0, 0x20000 / halfw, 0x8000 / hgt);
    off += 0x1000;
  }

  if (D_800C4124 == 1) {
    s32 unused[2];

    D_800C412C = D_800C412C + D_800D1440;

    if (D_800C4128 == 0) {
      f32 wobble;

      halfw = sinf(D_800C412C) * 6.0f + 32.0f;
      wobble = sinf(D_800C412C);
      hgt = wobble + wobble + 12.0f;
    } else {
      halfw = 0x18;
      hgt = 8;
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    func_8006A548(&gfx, (u32)D_800E1C0C + 8);
    load_texture_block(&gfx, (u32)D_800E1C0C + 0x208, G_IM_FMT_CI, G_IM_SIZ_8b,
                       0x48, 0x18, 0, 0, 0, 0, 0, 0, 0);

    ulx = (0x80 - halfw) << 2;
    yc = span / 4 + 0x86;
    func_8006A2C0(&gfx, ulx, (yc - hgt) << 2, (halfw + 0x80) << 2,
                  (hgt + yc) << 2, 0, 0, 0, 0x9000 / halfw, 0x3000 / hgt);

    if (D_800C4128 == 1) {
      f32 wobble;

      halfw = sinf(D_800C412C) * 6.0f + 32.0f;
      wobble = sinf(D_800C412C);
      hgt = wobble + wobble + 12.0f;
    } else {
      halfw = 0x18;
      hgt = 8;
    }

    load_texture_block(&gfx, (u32)D_800E1C0C + 0x8C8, G_IM_FMT_CI, G_IM_SIZ_8b,
                       0x48, 0x18, 0, 0, 0, 0, 0, 0, 0);

    ulx2 = (0xC0 - halfw) << 2;
    yc2 = span / 4 + 0x86;
    do {
      uly2 = (yc2 - hgt) << 2;
    } while (0);
    lrx2 = (halfw + 0xC0) << 2;
    yc3 = span / 4 + 0x86;
    lry2 = (hgt + yc3) << 2;
    dsdx2 = 0x9000 / halfw;
    dtdy2 = 0x3000 / hgt;
    func_8006A2C0(&gfx, ulx2, uly2, lrx2, lry2, 0, 0, 0, dsdx2, dtdy2);
  }

  *pgfx = gfx;
}

void func_8006C3E8(void) {
  RomAssetLoad asset;

  if (D_800C4010 == 0) {
    D_800E1C00 = heap3_alloc(get_rom_asset_allocation_size(0x955, &asset));
    load_rom_asset_payload(D_800E1C00, &asset);
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
 * The set-path loop note and the loop-scoped t preserve the measured
 * register-allocation order; flattening both clamp paths changes it.
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

      cj = j;
      ci = i;
      if (cj > 0x4B) {
        cj = 0x4B;
      }
      if (ci > 0x6F) {
        ci = 0x6F;
      }
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

/* Shared return labels preserve the branch-shaped bit test; separate early
 * returns let jump.c fold the last test into a shift. */
s32 func_8006C8CC(ControllerState* state) {
  if (D_800C4010 == 0) goto yes;
  if (D_800C4014 != 0) goto yes;
  if (D_800C4018 >= 0) goto no;
  if (state->pressed & 0x8000) goto yes;
no:
  return 0;
yes:
  return 1;
}

extern Gfx D_800C3FC0[];
extern s8 g_debug_mode_enabled;
extern char D_80105118[];
extern char D_800D1448[];

extern int sprintf(char* s, const char* fmt, ...);
extern void check_and_print_grid(char* str, s32 col, s32 row);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);

/**
 * Advances the iris wipe over the hole view and emits its masked overlay.
 *
 * The wipe radius lives in D_800C4018 and is stepped by the phase in
 * D_800C4014: closing shrinks it 8 rings per frame until it passes -0x95, and
 * opening grows it 12 rings per frame until it reaches 0x90, each phase firing
 * one sound cue as it crosses its threshold. The mask itself is then blitted as
 * 38 six-scanline strips, the last one being the 224-line remainder.
 *
 * Two variable choices below are load-bearing for register allocation, not
 * style: `lry` holds `h + 8` as its own statement so fold.c's associate_trees
 * cannot pull the constant onto the row induction variable, and the row origin
 * stays spelled `i * 6` inline rather than as a named variable so loop.c
 * creates the three induction variables in body order.
 */
void update_and_draw_iris_wipe(Gfx** pgfx) {
  Gfx* gfx = *pgfx;
  s32 i;
  s32 h;
  s32 lry;

  if (D_800C4010 == 0) {
    return;
  }

  if (D_800C4014 == 0) {
    if (D_800C4018 > 0) {
      for (i = 0; i != 8; i++) {
        stamp_circle_ring_alpha((u32*)((u8*)D_800E1C00 + 8), 0x4C, 0x70,
                                D_800C4018);
        D_800C4018--;
      }
      if (D_800C4018 == 0x60) {
        func_80050DA0(0x65, 2, 0x28, 0x14, 0x7F);
      }
    } else if (D_800C4018 >= -0x95) {
      D_800C4018--;
    } else {
      D_800C4014 = 1;
    }
  }

  if (D_800C4014 == 2) {
    if (D_800C4018 < 0) {
      D_800C4018++;
    } else if (D_800C4018 < 0x90) {
      for (i = 0; i != 12; i++) {
        stamp_circle_ring_alpha((u32*)((u8*)D_800E1C00 + 8), 0x4C, 0x70,
                                D_800C4018);
        D_800C4018++;
      }
      if (D_800C4018 == 0x30) {
        func_80050DA0(0x64, 2, 0x28, 0x14, 0x7F);
      }
    } else {
      D_800C4014 = 3;
      D_800C4010 = 2;
      func_8006C450();
      return;
    }
  }

  if (g_debug_mode_enabled != 0) {
    sprintf(D_80105118, D_800D1448, D_800C4018);
    check_and_print_grid(D_80105118, 0x14, 5);
  }

  gSPDisplayList(gfx++, D_800C3FC0);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0, TEXEL0, 0,
                    1, TEXEL0, 1);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);

  for (i = 0; i != 0x26; i++) {
    h = (i == 0x25) ? 2 : 6;
    lry = h + 8;
    gDPLoadTextureTile(gfx++, ((u32)D_800E1C00 + 8 + i * 0xE40) & ~7,
                       G_IM_FMT_RGBA, G_IM_SIZ_16b, 304, 0, 0, 0, 303, h - 1, 0,
                       0, 0, 0, 0, 0, 0);
    func_8006A2C0(&gfx, 8 << 2, (i * 6 + 8) << 2, 312 << 2, (i * 6 + lry) << 2,
                  0, 0, 0, 0x400, 0x400);
  }

  *pgfx = gfx;
}

extern s32 get_round_first_hole(void);
extern s32 get_current_course_id(void);
extern s32 round_hole_index;
extern s32 calculate_hole_handicap_adjustment(s32 course_id, s32 hole_index,
                                              s32 score);

s32 func_8006CD50(s32 hole_limit, s32 player) {
  s32 total = 0;
  s8* course = (s8*)&D_800C1420[get_current_course_id()];
  s32 i;
  if (round_hole_index == hole_limit) {
    hole_limit += player_records[player].hole_status != 0;
  }
  for (i = get_round_first_hole(); i < hole_limit; i++) {
    s32 adjustment = calculate_hole_handicap_adjustment(
        scenario_mode_id, i, player_records[player].handicap);
    total += player_records[player].hole_scores[i].score - course[i * 10 + 20] +
             adjustment;
  }
  return total;
}

extern s32 active_player_count[];
extern u16 D_80106230[];
extern s8 D_801050BC[];
extern s8 D_801050BD;
extern s8 D_801050BE;
extern char D_800D1450[];


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

  for (col = 0; col != active_player_count[0]; col++) {
    D_80106230[col] = 0;
  }

  hasZero = 0;
  for (i = 0; i != active_player_count[0]; i++) {
    if (player_records[i].hole_status == 0) {
      hasZero = 1;
    }
  }
  arg0 += (hasZero == 0);

  D_801050BC[0] = -1;
  D_801050BD = 0;
  D_801050BE = 0;

  counter = 1;
  col = get_round_first_hole();
  {
    for (; col < arg0; col++) {
      last = -1;
      for (i = 0; i != active_player_count[0]; i++) {
        if (player_records[i].hole_scores[col].score != 0) {
          last = i;
          player_records[i].hole_scores[col].score = counter;
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

extern s32 get_course_hole_primary_flag(s32 course, s32 hole);
extern s32 get_course_hole_secondary_flag(s32 course, s32 hole);
extern void func_8006D214(void);
extern void func_8006D38C(void);
/* -2 marks a pending per-player result; mode 1 converts it to -1.
 * Field-rooted byte scans retain the retail induction and aliasing shape. */

void func_8006D4EC(s32 mode) {
  s32 hole = scenario_state_id.round_hole_index;
  s32 i;
  s32 ready;
  s32 count;
  s32* status;
  s32 offset;
  s32 sentinel;
  if (D_800C413C != 0) {
    return;
  }
  if (get_course_hole_primary_flag(scenario_mode_id, hole)) {
    if (mode == 1) {
      for (i = 0; i != scenario_state_id.active_player_count; i++) {
        status = &player_records[i].unknown_80;
        if (*status == -2) {
          *status = -1;
        }
      }
    }
    ready = 0;
    i = 0;
    if (scenario_state_id.active_player_count != 0) {
      sentinel = -2;
      count = scenario_state_id.active_player_count;
      offset = 0;
    primary_scan:
      ready += *(s32*)((u8*)&player_records[0].unknown_80 + offset) != sentinel;
      offset += sizeof(PlayerState);
      i++;
      if (i != count) {
        goto primary_scan;
      }
    }
    if (ready == scenario_state_id.active_player_count) {
      func_8006D214();
      D_800C413C = 1;
    }
  } else if (get_course_hole_secondary_flag(scenario_mode_id, hole)) {
    if (mode == 1) {
      for (i = 0; i != scenario_state_id.active_player_count; i++) {
        status = &player_records[i].unknown_84;
        if (*status == -2) {
          *status = -1;
        }
      }
    }
    ready = 0;
    i = 0;
    if (scenario_state_id.active_player_count != 0) {
      sentinel = -2;
      count = scenario_state_id.active_player_count;
      offset = 0;
    secondary_scan:
      ready += *(s32*)((u8*)&player_records[0].unknown_84 + offset) != sentinel;
      offset += sizeof(PlayerState);
      i++;
      if (i != count) {
        goto secondary_scan;
      }
    }
    if (ready == scenario_state_id.active_player_count) {
      func_8006D38C();
      D_800C413C = 1;
    }
  } else {
    D_800C4144 = -1;
  }
}

extern void func_8005DE60(void);

/**
 * Folds the finished round into the 0x74-byte per-mode record at
 * func_8005AF50() + scenario_mode_id * 116 + 0xE98. Its sample count stops
 * incrementing at 9999; subsequent totals decay by 9999/10000. The explicit
 * ScenarioStatistics layout names observed fields and leaves uncertain
 * counters/totals as unknown_<offset>.
 *
 * PlayerState owns the score/status aliases: hole_status at +0x26, handicap
 * at +0x29, hole_scores at +0x2B, and the second byte of each hole score at
 * +0x2C. Accesses use the enclosing record's members, not separate arrays.
 */
void accumulate_mode_stats(void) {
  u8* rec;
  ScenarioStatistics* stats;
  s32* g;
  s8* cfg;
  s8* p;
  s32 sat;
  s64 v;
  s64 sum;
  s32 character;
  s32 off;
  s32 i;
  s32 n;
  s32 x;

  rec = func_8005AF50() + (scenario_mode_id * 116 + 0xE98);
  stats = (ScenarioStatistics*)rec;
  g = &D_800FF4D0.unknown_00;
  cfg = (s8*)&D_800C1420[scenario_mode_id];

  if (stats->sample_count <= 0) {
    stats->sample_count = 0;
    stats->best_round_score = 0x7FFFFFFF;
    stats->hole_score_total = 0;
    stats->high_water_mark = 0;
    stats->weighted_average = 0;
    stats->average_sample_count = 0;
    stats->unknown_18 = 0;
    stats->unknown_1C = 0;
    stats->unknown_counter_20 = 0;
    stats->unknown_counter_24 = 0;
    stats->unknown_28 = 0;
    stats->unknown_2C = 0;
  }
  if (stats->sample_count < 9999) {
    sat = 0;
    stats->sample_count = stats->sample_count + 1;
  } else {
    sat = 1;
  }

  player_records[0].hole_status = 0;
  v = func_8006CD50(0x12, 0);
  if (v < stats->best_round_score) {
    stats->best_round_score = v;
  }
  if (stats->best_round_score < -0x36) {
    stats->best_round_score = -0x36;
  }
  character = player_records[0].character_index;
  if (v < (s16)(u16)stats->best_round_score_by_character[character]) {
    stats->best_round_score_by_character[character] = v;
    stats->character_has_record[player_records[0].character_index] = 1;
    character = player_records[0].character_index;
    if (stats->best_round_score_by_character[character] < -0x36) {
      stats->best_round_score_by_character[character] = -0x36;
    }
  }

  sum = 0;
  for (off = 0; off != sizeof(player_records[0].hole_scores); off += sizeof(PlayerHoleScore)) {
    sum += (s8) * (u8*)((u8*)&player_records[0].hole_scores[0].score + off);
  }
  sum += stats->hole_score_total;
  if (sat == 1) {
    sum = sum * 9999 / 10000;
  }
  if (sum > 0x7FFFFFFF) {
    sum = 0x7FFFFFFF;
  }
  stats->hole_score_total = sum;
  if (stats->high_water_mark < g[3]) {
    stats->high_water_mark = g[3];
  }

  n = stats->average_sample_count;
  if (n == 0) {
    if (g[5] == 0) {
      stats->weighted_average = 0;
    } else if (g[5] > 0) {
      stats->weighted_average = g[4] / g[5];
      stats->average_sample_count = 1;
    } else {
      stats->weighted_average = 0;
    }
  } else if (g[5] > 0) {
    stats->weighted_average = (stats->weighted_average * n + g[4]) / (n + g[5]);
    stats->average_sample_count = stats->average_sample_count + g[5];
  }

  x = stats->unknown_18 + g[2];
  if (x > 0x7FFFFFFF) {
    stats->unknown_18 = 0x7FFFFFFF;
  } else {
    stats->unknown_18 = x;
  }
  if (sat == 1) {
    stats->unknown_18 = stats->unknown_18 * 9999 / 10000;
  }

  sum = 0;
  for (i = 0; i != 0x12; i++) {
    s32 b;

    off = i * sizeof(PlayerHoleScore);
    b = (s8) * (u8*)((u8*)&player_records[0].hole_scores[0].unknown_01 + off);
    if (b != -1) {
      sum += b;
    }
  }
  sum += stats->unknown_1C;
  if (sat == 1) {
    sum = sum * 9999 / 10000;
  }
  if (sum > 0x7FFFFFFF) {
    sum = 0x7FFFFFFF;
  }
  stats->unknown_1C = sum;

  for (i = 0; i != 0x12; i++) {
    off = i * sizeof(PlayerHoleScore);
    p = &cfg[i * 10];
    if (*(s8*)((u8*)&player_records[0].hole_scores[0].score + off) <= p[0x14]) {
      x = stats->unknown_counter_20;
      if (x <= 0x7FFFFFFE) {
        stats->unknown_counter_20 = x + 1;
      }
      if (sat == 1) {
        stats->unknown_counter_20 = stats->unknown_counter_20 * 9999 / 10000;
      }
    }
  }

  for (i = 0; i != 0x12; i++) {
    s32 d;

    off = i * sizeof(PlayerHoleScore);
    p = &cfg[i * 10];
    d = *(s8*)((u8*)&player_records[0].hole_scores[0].score + off) - *(s8*)((u8*)&player_records[0].hole_scores[0].unknown_01 + off);
    if (p[0x14] - d >= 2) {
      x = stats->unknown_counter_24;
      if (x <= 0x7FFFFFFE) {
        stats->unknown_counter_24 = x + 1;
      }
      if (sat == 1) {
        stats->unknown_counter_24 = stats->unknown_counter_24 * 9999 / 10000;
      }
    }
  }

  x = stats->unknown_28 + g[0];
  if (x > 0x7FFFFFFF) {
    stats->unknown_28 = 0x7FFFFFFF;
  } else {
    stats->unknown_28 = x;
  }
  x = stats->unknown_2C + g[1];
  if (x > 0x7FFFFFFF) {
    stats->unknown_2C = 0x7FFFFFFF;
  } else {
    stats->unknown_2C = x;
  }
  func_8005DE60();
}

void func_8006DDCC(void) {
  u8* p = func_8005AF50();
  s32 v;
  ScenarioStatistics* stats;
  s32* high_water_mark;

  D_800FF4D0.unknown_00 = 0;
  D_800FF4D0.unknown_04 = 0;
  D_800FF4D0.result_count = 0;
  D_800FF4D0.average_sample_count = 0;
  stats = (ScenarioStatistics*)(p + (scenario_mode_id * sizeof(ScenarioStatistics) + 0xE98));
  high_water_mark = &stats->high_water_mark;
  v = *high_water_mark;
  D_800FF4D0.result_total = 0;
  D_800FF4D0.include_in_average = 0;
  D_800FF4D0.result_max = v;
}

void func_8006DE44(void) {
  u8* p = func_8005AF50();
  s32 v;
  ScenarioStatistics* stats;
  s32* high_water_mark;

  D_800FF4D0.unknown_00 = 0;
  D_800FF4D0.unknown_04 = 0;
  D_800FF4D0.result_count = 0;
  D_800FF4D0.average_sample_count = 0;
  stats = (ScenarioStatistics*)(p + (scenario_mode_id * sizeof(ScenarioStatistics) + 0xE98));
  high_water_mark = &stats->high_water_mark;
  v = *high_water_mark;
  D_800FF4D0.include_in_average = 0;
  D_800FF4D0.result_max = v;
}


void func_8006DEB4(void) {
  s32* counts;
  volatile s32* pending;

  func_8005AF50();
  counts = &D_800FF4D0.unknown_00;
  if (player_records[0].unknown_9C != 0) {
    if (player_records[0].base_score <= hole_environment.unknown_2C) {
      counts[0]++;
    } else {
      D_800FF4D0.unknown_04++;
    }
  }
  /* Retail takes three distinct snapshots of the pending result. */
  pending = &player_records[0].pending_result;
  if (*pending >= 0) {
    counts[2]++;
    {
      s32 value = *pending;
      if (counts[3] < value) {
        counts[3] = value;
      }
    }
    if (counts[6] == 1) {
      counts[5]++;
      counts[4] += *pending;
    }
  }
}


s32 func_8006DF84(void) {
  s32* base = &D_800FF4D0.unknown_00;
  s32 pending;
  s16 field;
  ClubShot* shot;
  s16* shot_field;

  shot = get_shot_data();
  pending = player_records[0].unknown_80;
  player_records[0].pending_result = pending;
  shot_field = &shot->club_index;
  field = *shot_field;
  if (field != 0) {
    goto clear;
  }
  if (pending > 0) {
    D_800FF4D0.include_in_average = 1;
    goto done;
  }
clear:
  base[6] = 0;
done:
  return player_records[0].pending_result > base[3];
}

/* func_8006E210 + func_8006DFF0: CARRY (S310). Both reach the ROM's exact
 * instruction count and exact frame; the residual is three register/placement
 * clusters (~20 of 679 instructions). func_8006DFF0 is a GCC NESTED FUNCTION of
 * func_8006E210 -- it takes the static chain in $v0 and shares the parent's
 * `i`/`j` through it -- so the two bank as one slice and the permuter is
 * unavailable for the TU. The full reconstruction, the nine levers that closed
 * 526 of 543 instructions, and the jump.c store-flag analysis are in
 * docs/wip/func_8006E210.near-match.md; the bank-time carve is
 * `- [0xAC860, .rodata, main/func_8006A2C0]`.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006DFF0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006A2C0", func_8006E210);
