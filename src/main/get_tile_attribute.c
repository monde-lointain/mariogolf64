#include "common.h"

extern s32 get_dominant_terrain_attribute(s32, s32, s32);
extern s32 get_interpolated_terrain_height(s32, s32);
extern void load_hole_terrain_assets(void);
extern void* get_table_entry(u32);
extern s16* get_direct_grid_vertex(s32, s32);
extern s16 g_terrain_grid_verts[];

typedef struct {
  s16 unk[8];
} GridVertex;

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", get_tile_attribute);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_8004018C);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", ci8_to_rgba5551);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_800402F4);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", load_club_offset_pair);

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

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_8004107C);

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80041160);

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

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_800413C0);

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

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80042738);

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
