#include "common.h"
#include "rom_asset.h"

extern void* heap3_alloc(u32 need);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);
extern void heap3_free(void** payload_ptr);
extern void func_800718C4(void);
extern void func_8007512C(void);
extern u8 D_151CD40[];
extern u8 D_151CD50[];
extern void* D_8012F4E0;
extern void* D_800E1C30;
extern void* D_800E1C34;
extern void* D_800E1C38;
extern void* D_800E1C3C;
extern void* D_800E1C40;
extern void* D_800E1C4C;
extern void* D_800E1C44;
extern u16* D_800E1C48[];
extern u32 D_80104FD0;

/* Load seven tagged HUD/glyph assets through get_rom_asset_allocation_size,
 * heap3_alloc, and load_rom_asset_payload, then two raw-ROM images:
 * D_151CD40 (w*h+8) and D_151CD50 (size-prefixed). */
void load_hud_glyph_assets(void) {
  RomAssetLoad asset;
  void* headerBuf;
  s32 width;
  s32 height;
  s32 size2;

  D_8012F4E0 = heap3_alloc(get_rom_asset_allocation_size(0x4B2, &asset));
  load_rom_asset_payload(D_8012F4E0, &asset);
  D_800E1C30 = heap3_alloc(get_rom_asset_allocation_size(0x4B3, &asset));
  load_rom_asset_payload(D_800E1C30, &asset);
  D_800E1C34 = heap3_alloc(get_rom_asset_allocation_size(0x4B5, &asset));
  load_rom_asset_payload(D_800E1C34, &asset);
  D_800E1C38 = heap3_alloc(get_rom_asset_allocation_size(0x4B6, &asset));
  load_rom_asset_payload(D_800E1C38, &asset);
  D_800E1C4C = heap3_alloc(get_rom_asset_allocation_size(0x4B0, &asset));
  load_rom_asset_payload(D_800E1C4C, &asset);
  D_800E1C3C = heap3_alloc(get_rom_asset_allocation_size(0x4B9, &asset));
  load_rom_asset_payload(D_800E1C3C, &asset);
  D_800E1C40 = heap3_alloc(get_rom_asset_allocation_size(0x4B8, &asset));
  load_rom_asset_payload(D_800E1C40, &asset);

  headerBuf = heap3_alloc(8);
  nuPiReadRom((u32)D_151CD40, headerBuf, 8);
  width = (((u8*)headerBuf)[2] << 8) | ((u8*)headerBuf)[3];
  height = (((u8*)headerBuf)[4] << 8) | ((u8*)headerBuf)[5];
  D_800E1C44 = heap3_alloc(size2 = width * height + 8);
  nuPiReadRom((u32)D_151CD40, D_800E1C44, width * height + 8);

  nuPiReadRom((u32)D_151CD50, headerBuf, 8);
  size2 = (((u8*)headerBuf)[0] << 8) | ((u8*)headerBuf)[1];
  heap3_free(&headerBuf);
  D_800E1C48[0] = heap3_alloc(size2);
  nuPiReadRom((u32)D_151CD50, D_800E1C48[0], size2);

  D_80104FD0 = (D_800E1C48[0][1] << 1) + 2;
  func_800718C4();
  func_8007512C();
}

void func_800715A0(void) {
  heap3_free(&D_8012F4E0);
  heap3_free(&D_800E1C30);
  heap3_free(&D_800E1C34);
  heap3_free(&D_800E1C38);
  heap3_free(&D_800E1C4C);
  heap3_free(&D_800E1C44);
  heap3_free((void**)D_800E1C48);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071608);

typedef struct {
  /* 0x00 */ s32 screen_x;
  /* 0x04 */ s32 screen_y;
  /* 0x08 */ s32 glyph_size;
  /* 0x0C */ s32 visible_glyph_count;
  /* 0x10 */ s32 flags;
  /* 0x14 */ s32 glyph_run_id;
  /* 0x18 */ u8 red;
  /* 0x19 */ u8 green;
  /* 0x1A */ u8 blue;
  /* 0x1B */ u8 alpha;
  /* 0x1C */ s32 unknown_1C[4];
} HudTextEntry; /* 0x2C */
extern HudTextEntry D_8012F510[];

/* Initialize the glyph run index of each of the 12 entries. */
void func_800718C4(void) {
  s32 i;
  s32 bound;
  s32 val;

  i = 0;
  val = -1;
  bound = 12;
  do {
    D_8012F510[i].glyph_run_id = val;
    i++;
  } while (i != bound);
}

extern u8 D_8010CFA8;
extern u8 D_8010CFA9;
extern u8 D_8010CFAA;

void func_800718F4(u8 r, u8 g, u8 b, s32 idx) {
  (&D_8010CFA8)[idx * 3] = r;
  (&D_8010CFA9)[idx * 3] = g;
  (&D_8010CFAA)[idx * 3] = b;
}

/* Entry offset 0x1C contains four words through the 0x2C stride boundary.
 * The original D_8012F52C symbol is the same interior location as
 * D_8012F510[0].unknown_1C[0]; the selector extent is unknown, so do not clamp. */
void func_80071924(s32 value, s32 entry_index, s32 word_index) {
  D_8012F510[entry_index].unknown_1C[word_index] = value;
}

extern u8 D_801321B0;
extern u8 D_801321C3;

void func_80071954(u8* src) {
  u8* dst = &D_801321B0;
  u8* end = src + 0x13;

  do {
    u8 c = *src;
    *dst = c;
    if ((u8)(c - 0x61) < 0x1A) {
      *dst = c + 0x80;
    }
    if (*src == 0) {
      break;
    }
    src++;
    dst++;
  } while (src != end);
  D_801321C3 = 0;
}

extern void func_8007399C(s32 id, s32 size, s32* out_width, s32* out_height,
                          s32 player);
/* Member-rooted scalar views preserve the retail per-field address lowering. */
void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags,
                   u8 red, u8 green, u8 blue, u8 alpha) {
  s32 width;
  s32 height;
  s32 offset;
  s32 w;
  s32 h;
  if (run != -1) {
    width = 0;
    height = 0;
    if (flags & 0x10) {
      func_8007399C(run, 12, &width, &height, index);
    }
    offset = index * sizeof(HudTextEntry);
    w = width;
    h = height;
    *((u8*)&D_8012F510[0].red + offset) = red;
    *((u8*)&D_8012F510[0].green + offset) = green;
    *((u8*)&D_8012F510[0].blue + offset) = blue;
    *((u8*)&D_8012F510[0].alpha + offset) = alpha;
    *(s32*)((u8*)&D_8012F510[0].glyph_size + offset) = size;
    *(s32*)((u8*)&D_8012F510[0].flags + offset) = flags;
    *(s32*)((u8*)&D_8012F510[0].screen_x + offset) = x - w / 2;
    *(s32*)((u8*)&D_8012F510[0].screen_y + offset) = y - h / 2;
    if (*(s32*)((u8*)&D_8012F510[0].glyph_run_id + offset) != run) {
      *(s32*)((u8*)&D_8012F510[0].visible_glyph_count + offset) = 0;
    }
  }
  *(s32*)((u8*)&D_8012F510[0].glyph_run_id + index * sizeof(HudTextEntry)) =
      run;
}

extern void func_80071608(Gfx** dl);
extern void func_80072A08(Gfx** dl, s32 i);
extern void func_80071CE4(Gfx** dl, s32 i);

/* D_8012F5xx names are interior symbols for this same 0x2C-stride table:
 * 514=+4, 518=+8, 51C=+0xC, 520=+0x10, 524=+0x14, 528..52B=+0x18..1B,
 * and 52C=+0x1C. Use the enclosing record to keep these fields together. */
void emit_hud_table_prim_dl(Gfx** cursor) {
  Gfx* dl;
  s32 i;

  dl = *cursor;
  func_80071608(&dl);
  for (i = 0; i != 12; i++) {
    if (D_8012F510[i].glyph_run_id == -1) {
      continue;
    }
    if (D_8012F510[i].flags & 0x20) {
      continue;
    }
    gDPSetPrimColor(dl++, 0, 0, D_8012F510[i].red, D_8012F510[i].green,
                    D_8012F510[i].blue, D_8012F510[i].alpha);
    if (D_8012F510[i].glyph_size == 0xA) {
      func_80072A08(&dl, i);
    } else {
      func_80071CE4(&dl, i);
    }
  }
  *cursor = dl;
}

void func_80071C74(s32 entry_index) {
  D_8012F510[entry_index].visible_glyph_count = 0xFF;
}

s32 func_80071C9C(s32 entry_index) {
  HudTextEntry* entry = &D_8012F510[entry_index];
  s32 glyph_run_id = entry->glyph_run_id;
  u16* base = D_800E1C48[0];
  s32 visible_glyph_count = entry->visible_glyph_count;

  return visible_glyph_count >= (s32)base[glyph_run_id * 2 + 3];
}

/* D_800FF573 aliases the signed tag byte at D_800FF530 + 0x43. */
typedef struct {
  Mtx mtx;     /* 0x00 */
  u16 glyph;   /* 0x40 */
  u8 size;     /* 0x42 */
  s8 tag;      /* 0x43 */
  u8 flags;    /* 0x44 */
  u8 pad[3];   /* 0x45 */
} GlyphSprite; /* 0x48 */

extern GlyphSprite D_800FF530[];

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071CE4);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80072A08);

/**
 * Emits the fixed 22-command preamble for the small glyph sheet: 2-cycle
 * XLU_SURF state (point filter, no perspective, no LUT, alpha dither off, env
 * and blend colour) followed by a 16x12 4-bit I-format load into TMEM 0x100 on
 * tile 1.
 *
 * `sel & 1` picks between the two sheets `load_hud_glyph_assets` allocates
 * (asset 0x4B9 in `D_800E1C3C`, asset 0x4B8 in `D_800E1C40`, adjacent, so the
 * selector indexes off the first). The `+ 8` skips the asset header and the
 * `& ~7` re-aligns the pixel base for the RDP fetch.
 *
 * The load block is the stock `gDPLoadMultiBlock_4b` composite driven by
 * `gfx++`: the macro re-evaluates its packet argument once per sub-command,
 * which is what produces the ROM's per-command address temps and its 0x108
 * frame. Passing a non-incrementing `gfx` and adding 7 afterwards is 22
 * instructions short.
 */
void emit_glyph_sheet_preamble_dl(Gfx** cursor, s32 sel) {
  Gfx* gfx = *cursor;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_2CYCLE);
  gSPLoadGeometryMode(gfx++, 0);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPSetCombineLERP(gfx++, ENVIRONMENT, PRIMITIVE, TEXEL1, PRIMITIVE, 0, 0, 0,
                    TEXEL0, 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
  gDPSetEnvColor(gfx++, 0x80, 0x80, 0xFF, 0xFF);
  gDPSetBlendColor(gfx++, 0, 0, 0, 1);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPLoadMultiBlock_4b(gfx++, (((u32)(&D_800E1C3C)[sel & 1]) + 8) & ~7, 0x100,
                       1, G_IM_FMT_I, 16, 12, 0, G_TX_CLAMP, G_TX_WRAP, 0, 4, 0,
                       0);
  gDPPipeSync(gfx++);

  *cursor = gfx;
}

extern u8 D_800C4390[];

/* Font-string pixel-width accumulator. Per glyph (remapping lowercase a-z into
 * the 0xE1-0xFA custom range), width is added unless the glyph is a combining
 * mark ([0x9E,0x9F] or [0xDE,0xDF]). a1==0xC selects the proportional path
 * (fixed 0xC, or per-glyph D_800C4390[c] when flags&2 is set); otherwise a1 is
 * the fixed per-glyph advance. The classification uses separate slti tests
 * feeding one shared add block: a boolean `if` folds the 0xE0/0xDE pair into a
 * range test (fold-const.c fold_range_test), and an else-if chain fails to
 * cross-jump loop1's add into a single block, so goto-to-a-shared-label is the
 * only structure that matches (reorg delay-slot-duplicates loop2's tiny add).
 */
s32 func_800738BC(u8* str, s32 a1, s32 flags) {
  s32 width;
  s32 c;

  width = 0;
  if (a1 == 0xC) {
    flags &= 2;
    if (*str != 0) {
      do {
        c = *str;
        if ((u32)(c - 0x61) < 0x1A) {
          c += 0x80;
        }
        if (c < 0x9E) {
          goto add1;
        }
        if (c < 0xA0) {
          goto next1;
        }
        if (c >= 0xE0) {
          goto add1;
        }
        if (c >= 0xDE) {
          goto next1;
        }
      add1:
        if (flags != 0) {
          width += D_800C4390[c];
        } else {
          width += 0xC;
        }
      next1:
        str++;
      } while (*str != 0);
    }
  } else {
    if (*str != 0) {
      do {
        c = *str;
        if ((u32)(c - 0x61) < 0x1A) {
          c += 0x80;
        }
        if (c < 0x9E) {
          goto add2;
        }
        if (c < 0xA0) {
          goto next2;
        }
        if (c >= 0xE0) {
          goto add2;
        }
        if (c >= 0xDE) {
          goto next2;
        }
      add2:
        width += a1;
      next2:
        str++;
      } while (*str != 0);
    }
  }
  return width;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007399C);

s32 func_80073BF0(u8* str) {
  s32 len = 0;

  if (str[0] != 0) {
    while (str[0] != 0) {
      str++;
      len++;
    }
  }
  return len;
}

/**
 * Emits one string of 12x12 glyphs from the I4 font atlas at D_800E1C38 (192
 * texels wide, 16 glyphs per row), advancing the caller's display-list cursor.
 *
 * Per glyph: a 4-bit texture-tile load, a second tile-size declaration for tile
 * 1, a pipe sync and a texture rectangle. Lowercase is folded into the 0x80
 * page; the punctuation arms nudge the quad and shrink the advance.
 *
 * The goto chain is the same idiom the banked func_800738BC above uses, and for
 * the same reason. uls/ult are named temps rather than inlined macro arguments:
 * ending `code`'s live range at the div/mod is what emits the ROM's glyph-index
 * copy and hoists both atlas offsets to the top of the emit block.
 */
void emit_glyph_string_12px(Gfx** dl, s16 x, s16 y, u8* str) {
  Gfx* gfx = *dl;
  u8 c;

  gDPPipeSync(gfx++);
  c = *str;
  if (c != 0) {
    do {
      s32 code;
      s32 xpos;
      s32 ypos;
      s32 uls;
      s32 ult;

      if (c == 0x20) {
        str++;
        x += 12;
        continue;
      }
      xpos = x;
      ypos = y;
      code = c;
      if ((u32)(code - 0x61) < 0x1A) {
        code += 0x80;
      }
      if (code >= 0xA0) {
        goto hi;
      }
      if (code >= 0x9E) {
        goto mark;
      }
      if (code == 0x2E) {
        goto period;
      }
      goto draw;
    hi:
      if (code >= 0xE0) {
        goto draw;
      }
      if (code < 0xDE) {
        goto draw;
      }
    mark:
      xpos -= 6;
      ypos -= 4;
      x -= 12;
      goto draw;
    period:
      xpos -= 6;
      x -= 9;
    draw:
      uls = (code % 16) * 12;
      ult = (code / 16) * 12;
      gDPLoadTextureTile_4b(gfx++, ((u32)D_800E1C38 + 8) & ~7, G_IM_FMT_I, 192,
                            12, uls, ult, uls + 15, ult + 11, 0, 0, 0, 0, 0, 0,
                            0);
      gDPSetTileSize(gfx++, 1, uls << G_TEXTURE_IMAGE_FRAC,
                     ult << G_TEXTURE_IMAGE_FRAC,
                     (uls + 16) << G_TEXTURE_IMAGE_FRAC,
                     (ult + 12) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++, xpos << 2, ypos << 2, (xpos + 12) << 2,
                          (ypos + 12) << 2, G_TX_RENDERTILE, uls << 5, ult << 5,
                          1 << 10, 1 << 10);
      x += 12;
      str++;
    } while ((c = *str) != 0);
  }
  gDPPipeSync(gfx++);
  *dl = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80073F24);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074230);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074500);

extern void func_80073F24(Gfx** dl, s16 x, s16 y, u8* str);

void func_800747B0(Gfx** arg0, s32 arg1, s32 arg2, u8* str) {
  Gfx* saved = *arg0;
  s32 w = func_800738BC(str, 0xC, 2);

  func_80073F24(&saved, arg1 - w / 2, arg2, str);
  *arg0 = saved;
}

extern void func_80074230(s32* dst, s16 x, s16 y, u8* str);

void func_80074840(s32* arg0, s32 arg1, s32 arg2, u8* str) {
  s32 saved = *arg0;
  s32 w = func_800738BC(str, 0xA, 2);

  func_80074230(&saved, arg1 - w / 2, arg2, str);
  *arg0 = saved;
}

extern void func_80074500(s32* dst, s16 x, s16 y, u8* str);

void func_800748D0(s32* arg0, s32 arg1, s32 arg2, u8* str) {
  s32 saved = *arg0;
  s32 w = func_800738BC(str, 8, 2);

  func_80074500(&saved, arg1 - w / 2, arg2, str);
  *arg0 = saved;
}

void func_80074960(void) {}

/**
 * Emits the fixed 27-command render-mode and texture-load preamble for the HUD
 * glyph sheet into a caller-supplied glist. The texture is the asset
 * `load_hud_glyph_assets` stashes in `D_8012F4E0` (tag 0x4B2), skipped past its
 * 8-byte header and 8-aligned: IA 4-bit, 256x32.
 *
 * Near-twin of `emit_text_glyph_dl_preamble` (`src/main/func_8004D190.c`): the
 * same command sequence, plus a `gSPLoadGeometryMode(0)` in slot 2, and a
 * 256x32 tile where that one loads 256x24.
 */
void emit_hud_glyph_dl_preamble(Gfx** dlp) {
  Gfx* g = *dlp;

  gDPPipeSync(g++);
  gSPLoadGeometryMode(g++, 0);
  gDPPipeSync(g++);
  gDPSetCycleType(g++, G_CYC_1CYCLE);
  gDPPipeSync(g++);
  gDPSetRenderMode(
      g++,
      IM_RD | CLR_ON_CVG | CVG_X_ALPHA |
          GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
      IM_RD | CLR_ON_CVG | CVG_X_ALPHA |
          GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM));
  gDPSetCombineMode(g++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
  gSPTexture(g++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(g++);
  gDPSetAlphaCompare(g++, G_AC_NONE);
  gDPPipeSync(g++);
  gDPSetTexturePersp(g++, G_TP_NONE);
  gDPPipeSync(g++);
  gDPSetTextureFilter(g++, G_TF_POINT);
  gDPPipeSync(g++);
  gDPSetAlphaDither(g++, G_AD_DISABLE);
  gDPPipeSync(g++);
  gDPSetTextureLUT(g++, G_TT_NONE);
  gDPPipeSync(g++);
  gDPLoadTextureBlock_4b(g++, ((u32)D_8012F4E0 + 8) & ~7, G_IM_FMT_IA, 256, 32,
                         0, 0, 0, 0, 0, 0, 0);
  gDPPipeSync(g++);

  *dlp = g;
}

void func_80074CA8(Gfx** pgfx, s32 r, s32 g, s32 b) {
  Gfx* gfx = *pgfx;

  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, r, g, b, 0xFF);
  gDPPipeSync(gfx++);
  *pgfx = gfx;
}

void func_80074D0C(Gfx** dl, s16 x, s16 y, u8* str) {
  Gfx* gfx = *dl;
  u8 c;

  gDPPipeSync(gfx++);
  c = *str;
  if (c != 0) {
    while (c != 0) {
      s32 ch = c & 0xff;
      s32 idx;
      s32 quot;
      s32 rem;

      if (ch == 0x2E) {
        x -= 3;
      }
      idx = ch - 0x20;
      quot = idx / 31;
      rem = idx % 31;
      gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 8) << 2, (y + 8) << 2, 0,
                          rem << 8, (quot << 8) & 0xff00, 1 << 10, 1 << 10);
      if (*str == 0x2E) {
        x += 5;
      } else {
        x += 8;
      }
      str++;
      c = *str;
    }
  }
  gDPPipeSync(gfx++);
  *dl = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074E5C);

void func_80074EFC(Gfx** dl, s16 x, s16 y, u8* str) {
  Gfx* gfx = *dl;
  s32 count;
  u8* p;
  u8 c;

  gDPPipeSync(gfx++);
  count = 0;
  if (*str != 0) {
    p = str;
    while (*p != 0) {
      p++;
      count++;
    }
  }
  c = *str;
  x -= count * 3;
  if (c != 0) {
    while (c != 0) {
      gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 7) << 2, (y + 8) << 2, 0,
                          (c - 0x27) << 8, 0x300, 1 << 10, 1 << 10);
      str++;
      c = *str;
      x += 6;
    }
  }
  gDPPipeSync(gfx++);
  *dl = gfx;
}

void func_80075010(Gfx** dl, s16 x, s16 y, u8* str) {
  Gfx* gfx = *dl;
  u8 c;

  gDPPipeSync(gfx++);
  c = *str;
  if (c != 0) {
    while (c != 0) {
      s32 idx = c - 0x20;
      s32 quot = idx / 31;
      s32 rem = idx % 31;
      gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 8) << 2, (y + 8) << 2, 0,
                          rem << 8, (quot << 8) & 0xff00, 1 << 10, 1 << 10);
      str++;
      c = *str;
      x += 7;
    }
  }
  gDPPipeSync(gfx++);
  *dl = gfx;
}

/* D_800FF573 aliases the signed tag byte at D_800FF530 + 0x43. */
void func_8007512C(void) {
  s32 i = 0;
  do {
    D_800FF530[i].tag = -1;
  } while (++i != 300);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007515C);


extern u8 D_800FBE18[];

/* Lays out the glyph run `runId` as up-to-`count` translated sprite records
 * owned by `tag`, centred on (x, y): each free record (tag == -1) gets a
 * guTranslate matrix at the running pen position, the glyph code and the
 * owner tag. `size == -1` instead releases every record this tag owns.
 * Returns the run's half-extents packed as (width << 16) | height on the draw
 * path; the release path returns without a value. */
s32 place_glyph_sprite_run(s32 tag, s32 size, s32 x, s32 y, s32 runId) {
  s32 w;
  s32 h;
  s32 start;
  s32 count;
  s32 i;
  s32 j;
  s32 pen;
  s32 c;

  if (size == -1) {
    s32 bound;
    s32 val;

    j = 0;
    val = -1;
    bound = 300;
    do {
      if (D_800FF530[j].tag == tag) {
        D_800FF530[j].tag = val;
      }
      j++;
    } while (j != bound);
    return;
  }

  start = D_800E1C48[0][runId * 2 + 2];
  count = D_800E1C48[0][runId * 2 + 3];
  func_8007399C(runId, size, &w, &h, 0);
  w = w / 2;
  h = h / 2;
  y += h;
  D_800FBE18[tag] = count;
  {
    s32 bound2;
    s32 val2;

    j = 0;
    val2 = -1;
    bound2 = 300;
    do {
      if (D_800FF530[j].tag == tag) {
        D_800FF530[j].tag = val2;
      }
      j++;
    } while (j != bound2);
  }

  i = 0;
  pen = 0;
  if (count != 0) {
    s32 j = 0;

    do {
      if (D_800FF530[j].tag == -1) {
        c = D_800E1C48[0][D_80104FD0 + start + i];
        if (c < 0x100) {
          if ((u32)(c - 0xDE) < 2) {
            guTranslate(&D_800FF530[j].mtx, (f32)(pen - w + x) - 6.0f,
                        (f32)y + 4.0f, 0.0f);
          } else {
            guTranslate(&D_800FF530[j].mtx, (f32)(pen - w + x), (f32)y, 0.0f);
            pen += D_800C4390[c];
          }
          D_800FF530[j].glyph = c;
          D_800FF530[j].size = size;
          D_800FF530[j].tag = tag;
        } else if (c > 0x7FFF) {
          switch ((s32)(c - 0x8000)) {
            case 25: {
              s32 t = y - 6;

              pen = 0;
              y = t - size;
              break;
            }
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 12:
            case 13:
            case 14:
              guTranslate(&D_800FF530[j].mtx, (f32)(pen - w + x), (f32)y, 0.0f);
              pen += size;
              D_800FF530[j].glyph = c;
              D_800FF530[j].size = size;
              D_800FF530[j].tag = tag;
              break;
          }
        }
        i++;
      }
      j++;
    } while (i != count);
  }
  return (w << 16) | h;
}

extern void* D_800E1C30;
extern void* D_800E1C34;
extern void* D_800E1C38;
extern void* D_800E1C4C;
extern Vtx D_800C4490[];
extern Vtx D_800C44D0[];
extern Vtx D_800C4510[];
extern Vtx D_800C4550[];
extern Vtx D_800C4590[];

/* Emits the sprite display list for every glyph record owned by `tag`: a
 * push/pop matrix pair around a 4-vertex quad whose ST coordinates select the
 * glyph cell. Codes below 0x100 come from the three fixed-pitch I8 sheets
 * (12/10/other), codes above 0x7FFF from the paletted IA8 icon sheet, which
 * needs its TLUT loaded once (`lutLoaded`). */
void emit_glyph_sprite_dl(Gfx** dl, s32 tag) {
  Gfx* gfx = *dl;
  s32 i;
  s32 lutLoaded;
  s32 u;
  s32 v;
  s32 iconOff;
  s32 s0;
  s32 t0;
  s32 t1;

  lutLoaded = 0;
  gDPPipeSync(gfx++);
  for (i = 0; i != 300; i++) {
    u32 c;
    s32 size;

    if (D_800FF530[i].tag != tag) {
      continue;
    }
    iconOff = -1;
    if (lutLoaded == 1) {
      gDPSetCombineLERP(gfx++, K5, K5, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0,
                        K5, K5, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_NONE);
      lutLoaded = 0;
    }
    c = D_800FF530[i].glyph;
    if (c < 0x100) {
      s32 cell = D_800FF530[i].size;

      u = (c & 0xF) * cell;
      v = (c >> 4) * cell;
      gDPPipeSync(gfx++);
      {
        s32 sz = D_800FF530[i].size;

        if (sz == 0xC) {
          gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 96,
                             (void*)(((u32)D_800E1C38 + 8) & ~7));
        } else if (sz == 0xA) {
          gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 80,
                             (void*)(((u32)D_800E1C34 + 8) & ~7));
        } else {
          gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 64,
                             (void*)(((u32)D_800E1C30 + 8) & ~7));
        }
      }
      gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, 0, 0,
                 0, 0, 0, 0);
      gDPLoadSync(gfx++);
      gDPLoadTile(gfx++, G_TX_LOADTILE, u * 2, v * 4, (u + 15) * 2,
                  (v + 11) * 4);
      gDPPipeSync(gfx++);
      gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, 0, 0,
                 0, 0, 0, 0);
      gDPSetTileSize(gfx++, G_TX_RENDERTILE, u * 4, v * 4, (u + 15) * 4,
                     (v + 15) * 4);
    } else if (c > 0x7FFF) {
      switch ((s32)(c - 0x8000)) {
        case 0:
          iconOff = 0;
          break;
        case 1:
          iconOff = 0x90;
          break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
          iconOff = 0x1B0;
          break;
        case 7:
          iconOff = 0x360;
          break;
        case 8:
          iconOff = 0x3F0;
          break;
        case 9:
          iconOff = 0x480;
          break;
        case 10:
          iconOff = 0x510;
          break;
        case 12:
          iconOff = 0x120;
          break;
        case 13:
          iconOff = 0x240;
          break;
        case 14:
          iconOff = 0x2D0;
          break;
      }
      if (lutLoaded == 0) {
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(gfx++, (void*)(((u32)D_800E1C4C + 8) & ~7));
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        lutLoaded = 1;
      }
      {
        s32 off = iconOff + 0x208;

        gDPLoadTextureTile(gfx++, (void*)(((u32)D_800E1C4C + off) & ~7),
                           G_IM_FMT_CI, G_IM_SIZ_8b, 12, 12, 0, 0, 11, 11, 0, 0,
                           0, 0, 0, 0, 0);
      }
      u = 0;
      v = 0;
    }
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_800FF530[i].mtx),
              G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    size = D_800FF530[i].size;
    t0 = 0;
    s0 = 0;
    t1 = size;
    if (size == 0xC) {
      if (D_800FF530[i].flags & 1) {
        gSPVertex(gfx++, D_800C44D0, 4, 0);
        t1 = 7;
      } else if (D_800FF530[i].flags & 2) {
        gSPVertex(gfx++, D_800C4510, 4, 0);
        t1 = D_800FF530[i].size;
        t0 = 7;
      } else {
        gSPVertex(gfx++, D_800C4490, 4, 0);
      }
    } else if (size == 0xA) {
      gSPVertex(gfx++, D_800C4550, 4, 0);
    } else {
      gSPVertex(gfx++, D_800C4590, 4, 0);
    }
    gSPModifyVertex(gfx++, 0, G_MWO_POINT_ST,
                    ((s0 + u) << 21) | ((t0 + v) << 5));
    gSPModifyVertex(gfx++, 1, G_MWO_POINT_ST,
                    ((s0 + u) << 21) | ((t1 + v) << 5));
    gSPModifyVertex(gfx++, 2, G_MWO_POINT_ST,
                    ((size + u) << 21) | ((t1 + v) << 5));
    gSPModifyVertex(gfx++, 3, G_MWO_POINT_ST,
                    ((size + u) << 21) | ((t0 + v) << 5));
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  }
  gDPPipeSync(gfx++);
  *dl = gfx;
}

void func_80075E48(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetBlendColor(gfx++, 0, 0, 0, 1);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPPipeSync(gfx++);
  gDPSetCombineLERP(gfx++, K5, K5, 0, PRIMITIVE, 0, 0, 0, TEXEL0, K5, K5, 0,
                    PRIMITIVE, 0, 0, 0, TEXEL0);
  gDPPipeSync(gfx++);
  *gfxp = gfx;
}

s32 func_800760CC(s32 arg0) {
  u8 c = arg0 & 0xFF;

  switch (c) {
    case ':':
      return 0xB0;
    case '*':
      return 0;
    case '+':
      return 0xC0;
    case '-':
      return 0xD0;
    case '#':
      return 0xE0;
    case '!':
      return 0xF0;
    default:
      return (c - 0x2F) << 4;
  }
}

extern void func_8006A2C0(Gfx** gfxp, s32, s32, s32, s32, s32, s32, s32, s32,
                          s32);
extern u8 D_80105118[];

/* Emits a bitmap-font glyph-string display list at (y, x): a leading + trailing
 * gDPPipeSync (0xE7000000), and per character a func_800760CC glyph-code lookup
 * feeding a func_8006A2C0 texture-rect emitter (advancing y by 0xE per glyph).
 * The loop runs while D_80105118[i] != 0. A preheader-local `p = str` defers
 * the str param's callee-saved copy into the loop preheader, shortening its
 * live_length below i's so str allocates to $s2 and i to $s3 (matching the ROM
 * allocno order WITHOUT a 9th callee-saved reg); the explicit xr/k invariant
 * temps order the preheader hoists ahead of that copy, and the fresh w temp
 * schedules the trailing cursor increment before the final sync stores. */
void emit_glyph_string_dl(Gfx** gfxp, s32 y, s32 x, u8* str) {
  Gfx* g;
  Gfx* w;
  Gfx* c = *gfxp;
  s32 i;

  c->words.w0 = 0xE7000000;
  c->words.w1 = 0;
  g = c + 1;
  i = 0;
  if (D_80105118[0] != 0) {
    s32 xr = (x + 0x10) << 2;
    s32 k = 0x400;
    u8* p = str;
    do {
      s32 ret = func_800760CC(*p++);
      i++;
      func_8006A2C0(&g, y << 2, x << 2, (y << 2) + 0x40, xr, 0, 0, ret << 5, k,
                    k);
      y += 0xE;
    } while (D_80105118[i] != 0);
  }
  w = g;
  g++;
  w->words.w0 = 0xE7000000;
  w->words.w1 = 0;
  *gfxp = g;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007624C);
