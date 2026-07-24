#include "common.h"

extern s8 flag;
extern u8 D_800DAF60[];
extern u16 D_800BE6E0[];
extern u16* D_800BFEE0;

void print_string_at_grid(char* str, s32 col, s32 row);

/* Raw two-word DL emit, structurally identical to the gbi gDP* pointer macros
 * (Gfx *_g = pkt; _g->words.w0/w1 = ...). Used verbatim because several of the
 * preamble command words are non-standard (E2001E01, E3001A01|0x30, a LOADBLOCK
 * with lrs=1535/dxt=128) and do not map to a single SDK macro with derivable
 * args; the exact words come straight from the .s. */
#define EMIT(pkt, W0, W1)     \
  {                           \
    Gfx* _g = (pkt);          \
    _g->words.w0 = (u32)(W0); \
    _g->words.w1 = (u32)(W1); \
  }

/* emit_text_glyph_dl_preamble: fixed 26-command render-mode / texture-load DL
 * preamble emitted into a caller-supplied glist. Leaf; gcc-2.7.2 -O2 hoists all
 * 26 slot pointers up front (spilling 6 to the frame) then stores the two words
 * per slot in scheduler order. */
void emit_text_glyph_dl_preamble(Gfx** dlp) {
  Gfx* g = *dlp;

  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE3000A01, 0x00000000);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE200001C, 0x005510C0);
  EMIT(g++, 0xFC11FE23, 0xFFFFF3F9);
  EMIT(g++, 0xD7000002, 0x80008000);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE2001E01, 0x00000000);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE3000C00, 0x00000000);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE3001201, 0x00000000);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE3001A01, 0x00000030);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xE3001001, 0x00000000);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xFD700000, (u32)D_800BE6E0 & ~7);
  EMIT(g++, 0xF5700000, 0x07000000);
  EMIT(g++, 0xE6000000, 0x00000000);
  EMIT(g++, 0xF3000000, 0x075FF080);
  EMIT(g++, 0xE7000000, 0x00000000);
  EMIT(g++, 0xF5602000, 0x00000000);
  EMIT(g++, 0xF2000000, 0x003FC05C);
  EMIT(g++, 0xE7000000, 0x00000000);

  *dlp = g;
}
#undef EMIT

/* func_8004D4B8: string -> glyph-tile blit. CARRIED, ADVANCED (S270): the S182
 * dead-frame + 4-register-perm wall is SOLVED (str[row] indexing reproduces the
 * phantom 8B frame; explicit idx2/base temps + t2=0-first match all 6 regs).
 * Residual = a 2-word scheduler-slot coin (a `move t2,zero` scheduled
 * mid-atomic- c/32-division-BB, permuter-exhausted). Not byte-exact. Near-match
 * + root cause in docs/wip/func_8004D4B8.near-match.c.txt. */
INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D4B8);

void func_8004D5F0(s32 c, s32 x, s32 y);

void func_8004D580(char* str, s32 x, s32 y) {
  print_string_at_grid(str, x, y);
  while (*str != 0) {
    func_8004D5F0(*str, x, y);
    str++;
    x++;
  }
}

/* func_8004D5F0: framebuffer glyph renderer (double-buffered). CARRIED,
 * TERMINAL (S270): register-COALESCING wall retired-with-citation. The ROM
 * keeps 2 non-coalesced copies of c (frame 0x30/s0-s5); my -O2 coalesces (frame
 * 0x28) and is strictly more optimal. global.c:790-823 set_preferences merges
 * them from any faithful C; cse.c CSEs the sign-extend. Not source-reachable,
 * not flag-gated, permuter-plateaued. Do NOT re-open. Verdict in
 * docs/wip/func_8004D5F0.near-match.c.txt. */
INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D5F0);

void clear_text_grid(void) {
  s32 i = 0x4AF;
  u8* p = &D_800DAF60[0x4AF];

  for (; i >= 0; i--) {
    *p-- = 0;
  }
}

extern s32 flag_is_set(s32);

/* render_text_grid: renders the 30x40 D_800DAF60 text grid as a 3-doubleword
 * gSPTextureRectangle per non-space cell (8x8 px cell, glyph = (c&0x7F)-0x20),
 * toggling prim colour white<->red on the c&0x80 attribute bit. Runs after the
 * emit_text_glyph_dl_preamble preamble, gated by !flag_is_set(0x1D). idx/prevhi
 * are declared and zero-inited before the calls so they land in the
 * callee-saved s1/s0 the ROM uses; testing `prevhi` (not `hi`) in the colour
 * branch matches the ROM's reuse of s0. */
void render_text_grid(Gfx** chain) {
  Gfx* dl;
  s32 row;
  s32 col;
  s32 prevhi;
  s32 idx;
  s32 c;
  s32 gi;
  s32 hi;

  idx = 0;
  prevhi = 0;
  emit_text_glyph_dl_preamble(chain);
  if (flag_is_set(0x1D)) {
    return;
  }

  dl = *chain;

  gDPSetPrimColor(dl++, 0, 0, 255, 255, 255, 255);
  gDPPipeSync(dl++);

  for (row = 0; row < 30; row++) {
    for (col = 0; col < 40; col++) {
      c = D_800DAF60[idx];
      if ((u8)c >= 0x21) {
        gi = (c & 0x7F) - 0x20;
        hi = c & 0x80;
        if (hi != prevhi) {
          prevhi = hi;
          gDPPipeSync(dl++);
          if (prevhi != 0) {
            gDPSetPrimColor(dl++, 0, 0, 255, 0, 0, 255);
          } else {
            gDPSetPrimColor(dl++, 0, 0, 255, 255, 255, 255);
          }
          gDPPipeSync(dl++);
        }
        gSPTextureRectangle(dl++, col << 5, row << 5, (col + 1) << 5,
                            (row + 1) << 5, 0, (gi & 0x1F) << 8,
                            (gi << 3) & 0xFF00, 1 << 10, 1 << 10);
      }
      idx++;
    }
  }

  gDPPipeSync(dl++);
  *chain = dl;
}

void set_flag_based_on_param(s32 enable) {
  if (enable) {
    flag = -0x80;
  } else {
    flag = 0;
  }
}
