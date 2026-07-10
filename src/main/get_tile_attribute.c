#include "common.h"

extern s32 get_dominant_terrain_attribute(s32, s32, s32);
extern s32 get_interpolated_terrain_height(s32, s32);
extern void load_hole_terrain_assets(void);
extern void* get_table_entry(u32);
extern s16* get_direct_grid_vertex(s32, s32);
extern s16 g_terrain_grid_verts[];
extern s32 resolve_club_terrain_mask(s32, void*);

typedef struct {
  s16 unk[8];
} GridVertex;

/* get_tile_attribute: minimap-color -> tile-attribute lookup. Clamp x to
 * [0,0xFF], z to [0,0x1FF]; coarse-map D_800BAC0C[tx + tz*16] (s16, tx=x/16,
 * tz=z/16). >=0 returns the attr directly, ==-1 returns -1, and a negative
 * (!= -1) entry indexes a nibble-packed sub-tile map D_80191620[(x&0xF)>>1 +
 * (z&0xF)*8 + (attr&0x7FFF)*128] (odd x -> low nibble, even x -> high nibble).
 * Body fully solved (see git history / this comment) but carried as
 * INCLUDE_ASM: NEAR-MATCH WALL (S216). Everything matches except GCC bakes a
 * spurious -0x10
 * (-8 halfword) in-place addend onto the D_800BAC0C ref (mine lh -0x5404 vs
 * target lh -0x53f4) regardless of index form (1D `[tx+tz*16]`, `[tz*16+tx]`,
 * 2D `[tz][tx]`, explicit byte-offset `(u8*)base + tx*2 + tz*32`, or local
 * `s16 *cmap`), while the index VALUE (tx*2 + tz*32) is identical to target.
 * The reloc is R_MIPS_HI16/LO16 vs D_800BAC0C but the LO16 immediate carries
 * -0x10; a local pointer instead fully materializes base+index (lh 0(reg)),
 * also non-matching. Also: target computes tx<<1 early (branch delay slot) and
 * duplicates the sub-tile block per parity branch; mine schedules differently.
 * Needs a GCC-source dive on the phantom -0x10 addend fold (kin to
 * #flowing-bss-plus-n-address-diff but data-ref addend, not bss). */
INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", get_tile_attribute);

typedef struct {
  s8 r, g, b, unk;
} ColorEntry;

extern ColorEntry D_800BB040[];

s32 find_closest_palette_index(s32 color) {
  s32 best_dist;
  s32 best_idx;
  s32 r, g, b;
  s32 i;

  best_dist = 1000000;
  best_idx = 0;
  r = (color >> 11) & 0x1F;
  g = (color >> 6) & 0x1F;
  b = (color >> 1) & 0x1F;
  for (i = 0; i < 11; i++) {
    s32 dr = r - D_800BB040[i].r;
    s32 dg;
    s32 db;
    s32 sum;
    if (dr < 0) {
      dr = -dr;
    }
    dg = g - D_800BB040[i].g;
    if (dg < 0) {
      dg = -dg;
    }
    db = b - D_800BB040[i].b;
    sum = dr + dg;
    if (db < 0) {
      db = -db;
    }
    sum = sum + db;
    if (sum < best_dist) {
      best_dist = sum;
      best_idx = i;
    }
  }
  return best_idx;
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", ci8_to_rgba5551);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_800402F4);

void load_club_offset_pair(s8* club, s32* state, s32* out) {
  s32 mask = resolve_club_terrain_mask(1, club);
  if (state[2] & mask) {
    out[0] = club[8];
    out[1] = club[9];
  } else {
    out[0] = club[8] - club[0xA];
    if (out[0] < 5) {
      out[0] = 5;
    }
    out[1] = club[9] - club[0xA];
    if (out[1] < 5) {
      out[1] = 5;
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", get_ground_attribute);

f32 func_800407D8(s32 arg) { return (f32)arg; }

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute",
            get_dominant_terrain_attribute);

s32 query_terrain_at_position(s32 x, s32 z) {
  return get_dominant_terrain_attribute(x, z, 0);
}

void* func_80040E3C(s32 x, s32 z) {
  return get_table_entry(query_terrain_at_position(x, z));
}

extern u16 D_80132D4A[];

u16 get_terrain_type(s32 x, s32 z) {
  s32 gx, gz;
  s32 sx, sz;
  s32 block;
  u16* base;

  if ((u32)x > 0x3FFFFF) {
    return 0;
  }
  if ((u32)z > 0x7FFFFF) {
    return 0;
  }
  gx = x / 0x4000;
  gz = z / 0x4000;
  block = (((u32)gx >> 5) + ((u32)gz >> 5) * 8) * 2560;
  sx = gx & 0x1F;
  sz = gz & 0x1F;
  base = D_80132D4A;
  return *(u16*)((u8*)(base + (sx + sz * 36)) + block) | 1;
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", blend_terrain_color);

s32 func_80041058(s32 x, s32 unused, s32 z) {
  return query_terrain_at_position(x, z) == 5;
}

void average_grid_vertices(s16* a, s16* b, s16* out) {
  u8* ab = (u8*)a;
  u8* bb = (u8*)b;
  u8* ob = (u8*)out;
  out[0] = (a[0] + b[0]) / 2;
  out[1] = (a[1] + b[1]) / 2;
  out[2] = (a[2] + b[2]) / 2;
  out[3] = 0;
  out[4] = (a[4] + b[4]) / 2;
  out[5] = (a[5] + b[5]) / 2;
  ob[0xC] = (ab[0xC] + bb[0xC]) / 2;
  ob[0xD] = (ab[0xD] + bb[0xD]) / 2;
  ob[0xE] = (ab[0xE] + bb[0xE]) / 2;
  ob[0xF] = (ab[0xF] + bb[0xF]) / 2;
}

void lerp_grid_vertices(s16* a, s16* b, s16* out, s32 t) {
  u8* ab = (u8*)a;
  u8* bb = (u8*)b;
  u8* ob = (u8*)out;
  out[0] = (a[0] * (16 - t) + b[0] * t) / 16;
  out[1] = (a[1] * (16 - t) + b[1] * t) / 16;
  out[2] = (a[2] * (16 - t) + b[2] * t) / 16;
  out[3] = 0;
  out[4] = (a[4] * (16 - t) + b[4] * t) / 16;
  out[5] = (a[5] * (16 - t) + b[5] * t) / 16;
  ob[0xC] = (ab[0xC] * (16 - t) + bb[0xC] * t) / 16;
  ob[0xD] = (ab[0xD] * (16 - t) + bb[0xD] * t) / 16;
  ob[0xE] = (ab[0xE] * (16 - t) + bb[0xE] * t) / 16;
  ob[0xF] = (ab[0xF] * (16 - t) + bb[0xF] * t) / 16;
}

s16* get_direct_grid_vertex(s32 col, s32 row) {
  s16* result;

  if (((u32)col >= 18) | ((u32)row >= 34)) {
    result = g_terrain_grid_verts;
  } else {
    result = (s16*)((u8*)g_terrain_grid_verts +
                    ((row * 17 + ((u32)col >> 1) + (col & 1) * 9) << 4));
  }
  return result;
}

s16 func_800413A0(s32 x, s32 z) { return get_direct_grid_vertex(x, z)[1]; }

void init_grid_vertex(s32 col, s32 row, s32 height, s32 color) {
  s16* vtx = get_direct_grid_vertex(col, row);
  vtx[0] = col << 8;
  vtx[2] = row << 8;
  vtx[1] = height & 0xFFFE;
  vtx[3] = 0;
  vtx[4] = (col << 10) + 0x20;
  vtx[5] = (row << 10) + 0x20;
  if (row == 0x20) {
    vtx[5] = 0x7FFF;
  }
  ((u8*)vtx)[0xC] = color;
  ((u8*)vtx)[0xD] = color;
  ((u8*)vtx)[0xE] = color;
  ((u8*)vtx)[0xF] = 0xFF;
}

void func_80041464(s32 x, s32 z, s32 value) {
  s16* vtx = get_direct_grid_vertex(x, z);
  vtx[1] = value & 0xFFFE;
}

void func_80041490(s32 x, s32 z, s32 value) {
  u8* vtx = (u8*)get_direct_grid_vertex(x, z);
  vtx[0xC] = value;
  vtx[0xD] = value;
  vtx[0xE] = value;
}

void set_direct_grid_vertex(s32 col, s32 row, GridVertex* src) {
  GridVertex* dst = (GridVertex*)get_direct_grid_vertex(col, row);
  *dst = *src;
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute",
            get_terrain_vertex_pointer);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_800415C4);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80041878);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80041AA8);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80041B98);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80041D44);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80041E8C);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80041EC0);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80042228);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80042318);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_800425C8);

s32 func_80042720(void) { return (u32)&load_hole_terrain_assets > 0x803FFFFFU; }

typedef struct {
  s16 val;
  s8 pad[0xE];
} CylFieldS16;

typedef struct {
  s8 val;
  s8 pad[0xF];
} CylFieldS8;

extern CylFieldS16 collision_cylinders[];
extern CylFieldS16 D_800FBEA4[];
extern CylFieldS8 D_800FBEA6[];

void mark_scenery_collision_cells(void) {
  s32 mark = 0xF801;
  u16* base = D_80132D4A;
  s32 i;

  for (i = 0; i < 128; i++) {
    s32 gx, gz;
    s32 sx, sz;
    s32 block;

    if (D_800FBEA6[i].val < 0) {
      continue;
    }
    gx = collision_cylinders[i].val / 16;
    gz = D_800FBEA4[i].val / 16;
    block = (((u32)gx >> 5) + ((u32)gz >> 5) * 8) * 2560;
    sx = gx & 0x1F;
    sz = gz & 0x1F;
    *(u16*)((u8*)(base + (sx + sz * 36)) + block) = mark;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_800427E8);

void* func_80042DF4(void* base, s32 a1, s32 a2, s32 flags) {
  void* result;
  if (flags & 0x80) {
    result = get_direct_grid_vertex((a1 << 1) + ((flags >> 2) & 3),
                                    (a2 << 1) + (flags & 3));
  } else {
    result = (u8*)base + (flags << 4);
  }
  return result;
}

s32 func_80042E40(s32 ax, s32 ay, s32 bx, s32 by, s32 cx, s32 cy) {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute",
            detect_terrain_collision);

s32 func_800432E4(s16* a, s16* b) {
  s32 i;

  for (i = 0; i < 3; i++) {
    if (a[i + 2] != b[i + 2]) {
      return 1;
    }
    if (a[i + 5] != b[i + 5]) {
      return 1;
    }
    if (a[i + 8] != b[i + 8]) {
      return 1;
    }
  }
  return 0;
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_8004333C);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", compute_triangle_plane);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute",
            get_interpolated_terrain_height);

s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z) {
  return get_interpolated_terrain_height(x, z);
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute",
            get_lowest_height_at_position);
