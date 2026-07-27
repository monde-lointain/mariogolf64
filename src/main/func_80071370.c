#include "common.h"

extern u32 func_8005062C(u16 index, void* out);
extern void* heap3_alloc(u32 need);
extern void func_800506D4(void* dst, void* desc);
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
extern u16* D_800E1C48;
extern u32 D_80104FD0;

/* Allocates + loads the HUD/glyph asset set: 7 tagged assets via
 * func_8005062C size-lookup + heap3_alloc + func_800506D4 load, then two
 * raw-ROM images (D_151CD40 w*h+8, D_151CD50 size-prefixed) via nuPiReadRom. */
void load_hud_glyph_assets(void) {
  u8 spvar[0x20];
  void* headerBuf;
  s32 width;
  s32 height;
  s32 size2;

  D_8012F4E0 = heap3_alloc(func_8005062C(0x4B2, spvar));
  func_800506D4(D_8012F4E0, spvar);
  D_800E1C30 = heap3_alloc(func_8005062C(0x4B3, spvar));
  func_800506D4(D_800E1C30, spvar);
  D_800E1C34 = heap3_alloc(func_8005062C(0x4B5, spvar));
  func_800506D4(D_800E1C34, spvar);
  D_800E1C38 = heap3_alloc(func_8005062C(0x4B6, spvar));
  func_800506D4(D_800E1C38, spvar);
  D_800E1C4C = heap3_alloc(func_8005062C(0x4B0, spvar));
  func_800506D4(D_800E1C4C, spvar);
  D_800E1C3C = heap3_alloc(func_8005062C(0x4B9, spvar));
  func_800506D4(D_800E1C3C, spvar);
  D_800E1C40 = heap3_alloc(func_8005062C(0x4B8, spvar));
  func_800506D4(D_800E1C40, spvar);

  headerBuf = heap3_alloc(8);
  nuPiReadRom((u32)D_151CD40, headerBuf, 8);
  width = (((u8*)headerBuf)[2] << 8) | ((u8*)headerBuf)[3];
  height = (((u8*)headerBuf)[4] << 8) | ((u8*)headerBuf)[5];
  D_800E1C44 = heap3_alloc(size2 = width * height + 8);
  nuPiReadRom((u32)D_151CD40, D_800E1C44, width * height + 8);

  nuPiReadRom((u32)D_151CD50, headerBuf, 8);
  size2 = (((u8*)headerBuf)[0] << 8) | ((u8*)headerBuf)[1];
  heap3_free(&headerBuf);
  D_800E1C48 = heap3_alloc(size2);
  nuPiReadRom((u32)D_151CD50, D_800E1C48, size2);

  D_80104FD0 = (D_800E1C48[1] << 1) + 2;
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
  heap3_free((void**)&D_800E1C48);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071608);

extern s32 D_8012F524;

/* Inits field 0x14 (D_8012F524) of all 12 elements of the 0x2C-stride struct
 * array at D_8012F510 to -1. The variable bound/val + do-while + in-loop
 * byte-offset giv are load-bearing for the v0/v1 allocno order (S236). */
void func_800718C4(void) {
  s32 i;
  s32 bound;
  s32 val;

  i = 0;
  val = -1;
  bound = 12;
  do {
    s32 off = i * 0x2C;
    *(s32*)((u8*)&D_8012F524 + off) = val;
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

extern s32 D_8012F52C;

/* 2D setter D_8012F52C[idx][j] = val (field 0x1C of the 0x2C struct array at
 * D_8012F510, viewed as s32[][11]). Pre-computing the row byte-offset and the
 * u8* base into separate temps slips j*4 ahead of the pointer-forming add
 * (expr.c binop expands op0 fully first), and the u8* cast forces the full base
 * materialize instead of the %lo fold (S236). */
void func_80071924(s32 val, s32 idx, s32 j) {
  s32 io = idx * 0x2C;
  u8* b = (u8*)&D_8012F52C;
  *(s32*)(b + io + j * 4) = val;
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

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800719A0);

extern void func_80071608(Gfx** dl);
extern void func_80072A08(Gfx** dl, s32 i);
extern void func_80071CE4(Gfx** dl, s32 i);
extern s32 D_8012F518;
extern s32 D_8012F520;
extern u8 D_8012F528;
extern u8 D_8012F529;
extern u8 D_8012F52A;
extern u8 D_8012F52B;

/* Emits a per-entry gDPSetPrimColor (opcode 0xFA) + sub-emitter DL for the
 * 12-element 0x2C-stride table at D_8012F510, skipping entries whose unk_14==-1
 * or unk_10&0x20. dl is post-incremented (dl++) into the ROM's store-giv.
 * Computing the row byte-offset as `i * 0x2C` inside the loop (rather than a
 * separate off biv) is load-bearing: it orders the hoisted-invariant vs
 * biv-init emission so the prologue register init/save block schedules to match
 * (same i*0x2C idiom as func_800718C4). */
void emit_hud_table_prim_dl(Gfx** cursor) {
  Gfx* dl;
  s32 i;
  s32 off;

  dl = *cursor;
  func_80071608(&dl);
  for (i = 0; i != 12; i++) {
    off = i * 0x2C;
    if (*(s32*)((u8*)&D_8012F524 + off) == -1) {
      continue;
    }
    if (*(s32*)((u8*)&D_8012F520 + off) & 0x20) {
      continue;
    }
    gDPSetPrimColor(dl++, 0, 0, *((u8*)&D_8012F528 + off),
                    *((u8*)&D_8012F529 + off), *((u8*)&D_8012F52A + off),
                    *((u8*)&D_8012F52B + off));
    if (*(s32*)((u8*)&D_8012F518 + off) == 0xA) {
      func_80072A08(&dl, i);
    } else {
      func_80071CE4(&dl, i);
    }
  }
  *cursor = dl;
}

extern s32 D_8012F51C;

void func_80071C74(s32 idx) { *(s32*)((u8*)&D_8012F51C + idx * 0x2C) = 0xFF; }

typedef struct {
  u8 pad0[0xC];
  s32 unk_0C;
  s32 unk_10;
  s32 unk_14;
  u8 pad18[0x14];
} Elem2F510; /* stride 0x2C */

extern Elem2F510 D_8012F510[];
extern u16* D_800E1C48;

s32 func_80071C9C(s32 idx) {
  Elem2F510* e = &D_8012F510[idx];
  s32 t = e->unk_14;
  u16* base = D_800E1C48;
  s32 v = e->unk_0C;

  return v >= (s32)base[t * 2 + 3];
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80071CE4);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80072A08);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800734F0);

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
    do {
      str++;
      len++;
    } while (str[0] != 0);
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

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80074968);

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
    do {
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
    } while (c != 0);
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
    do {
      p++;
      count++;
    } while (*p != 0);
  }
  c = *str;
  x -= count * 3;
  if (c != 0) {
    do {
      gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 7) << 2, (y + 8) << 2, 0,
                          (c - 0x27) << 8, 0x300, 1 << 10, 1 << 10);
      str++;
      c = *str;
      x += 6;
    } while (c != 0);
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
    do {
      s32 idx = c - 0x20;
      s32 quot = idx / 31;
      s32 rem = idx % 31;
      gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 8) << 2, (y + 8) << 2, 0,
                          rem << 8, (quot << 8) & 0xff00, 1 << 10, 1 << 10);
      str++;
      c = *str;
      x += 7;
    } while (c != 0);
  }
  gDPPipeSync(gfx++);
  *dl = gfx;
}

extern u8 D_800FF573;

/* Inits byte field (D_800FF573) of all 300 elements of a 0x48-stride array to
 * -1. The s8* store keeps the QImode const at -1 (li a1,-1); the in-loop
 * byte-offset giv under do-while rematerializes %hi each iter (S236). */
void func_8007512C(void) {
  s32 i = 0;
  do {
    s32 off = i * 0x48;
    *(s8*)((u8*)&D_800FF573 + off) = -1;
  } while (++i != 300);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007515C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_800754BC);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_8007580C);

INCLUDE_ASM("asm/nonmatchings/main/func_80071370", func_80075E48);

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
 * schedules the trailing cursor increment before the final sync stores (S272,
 * cracks the S242 terminal biv-swap wall). */
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
