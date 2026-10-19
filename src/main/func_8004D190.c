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

void func_8004D4B8(char* source_text, s32 glyph_count, u16* destination) {
  s32 character_index;
  for (character_index = 0; character_index < glyph_count; character_index++) {
    s32 character_code = source_text[character_index];
    s32 destination_tile_row;
    s32 source_glyph_offset;
    s32 glyph_row;
    s32 glyph_column_offset;
    if (character_code == 0) {
      return;
    }
    if (character_code < 0x20) {
      character_code = 0x20;
    }
    if (character_code > 0x7A) {
      character_code = 0x20;
    }
    character_code -= 0x20;
    glyph_column_offset = (character_code & 0x1F) << 1;
    /* Split the signed division so the row initialization stays at its join. */
    if (character_code < 0) {
      character_code += 31;
    }
    glyph_row = 0;
    source_glyph_offset =
        glyph_column_offset | ((character_code >> 5) << 9);
    destination_tile_row = character_index;
    do {
      s32 destination_word_offset;
      s32 source_word_offset;
      s32 word_index;
      word_index = 0;
      destination_word_offset = destination_tile_row * 2;
      source_word_offset = source_glyph_offset;
      for (; word_index < 2; word_index++) {
        destination[destination_word_offset + word_index] =
            D_800BE6E0[source_word_offset + word_index];
      }
      source_glyph_offset += 0x40;
      glyph_row++;
      destination_tile_row += glyph_count;
    } while (glyph_row < 8);
  }
}

void func_8004D5F0(s32 character_code, s32 column, s32 row);

void func_8004D580(char* text, s32 column, s32 row) {
  print_string_at_grid(text, column, row);
  while (*text != 0) {
    func_8004D5F0(*text, column, row);
    text++;
    column++;
  }
}

/* func_8004D5F0: framebuffer glyph renderer (double-buffered). The ROM keeps 2
 * non-coalesced copies of c (frame 0x30/s0-s5); -O2 coalesces them (frame 0x28),
 * which is strictly more optimal. global.c:790-823 set_preferences merges them
 * from any faithful C, and cse.c CSEs the sign-extend. Not source-reachable and
 * not flag-gated. */
INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D5F0);

void clear_text_grid(void) {
  s32 byte_index = 0x4AF;
  u8* grid_byte = &D_800DAF60[0x4AF];

  for (; byte_index >= 0; byte_index--) {
    *grid_byte-- = 0;
  }
}

extern s32 flag_is_set(s32);

/* render_text_grid: renders the 30x40 D_800DAF60 text grid as a 3-doubleword
 * gSPTextureRectangle per non-space cell (8x8 px cell, glyph = (c&0x7F)-0x20),
 * toggling prim colour white<->red on the c&0x80 attribute bit. Runs after the
 * emit_text_glyph_dl_preamble preamble, gated by !flag_is_set(0x1D). The grid
 * index and previous attribute are initialized before the calls so they land
 * in the callee-saved s1/s0 the ROM uses; testing previous_attribute (not
 * character_attribute) in the colour branch matches the ROM's reuse of s0. */
void render_text_grid(Gfx** chain) {
  Gfx* dl;
  s32 row;
  s32 col;
  s32 previous_attribute;
  s32 grid_index;
  s32 character_code;
  s32 glyph_index;
  s32 character_attribute;

  grid_index = 0;
  previous_attribute = 0;
  emit_text_glyph_dl_preamble(chain);
  if (flag_is_set(0x1D)) {
    return;
  }

  dl = *chain;

  gDPSetPrimColor(dl++, 0, 0, 255, 255, 255, 255);
  gDPPipeSync(dl++);

  for (row = 0; row < 30; row++) {
    for (col = 0; col < 40; col++) {
      character_code = D_800DAF60[grid_index];
      if ((u8)character_code >= 0x21) {
        glyph_index = (character_code & 0x7F) - 0x20;
        character_attribute = character_code & 0x80;
        if (character_attribute != previous_attribute) {
          previous_attribute = character_attribute;
          gDPPipeSync(dl++);
          if (previous_attribute != 0) {
            gDPSetPrimColor(dl++, 0, 0, 255, 0, 0, 255);
          } else {
            gDPSetPrimColor(dl++, 0, 0, 255, 255, 255, 255);
          }
          gDPPipeSync(dl++);
        }
        gSPTextureRectangle(dl++, col << 5, row << 5, (col + 1) << 5,
                            (row + 1) << 5, 0, (glyph_index & 0x1F) << 8,
                            (glyph_index << 3) & 0xFF00, 1 << 10, 1 << 10);
      }
      grid_index++;
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
