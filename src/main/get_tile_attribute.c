#include "terrain.h"

extern s32 get_dominant_terrain_attribute(s32, s32, s32);
extern s32 get_interpolated_terrain_height(s32, s32);
extern void load_hole_terrain_assets(s32, s32);
extern TerrainAttrEntry* get_table_entry(u32);
extern s16* get_direct_grid_vertex(s32, s32);

extern s16 D_800BAC0C[];
extern u8 D_80191620[];

/* Negative coarse entries select a nibble-packed 16x16 sub-tile map. */
s32 get_tile_attribute(s32 x, s32 z) {
  s32 col;
  s32 row;
  s32 attribute;
  s32 result;
  s32 byte_offset;

  if (x < 0) {
    x = 0;
  } else if (x >= 256) {
    x = 255;
  }
  if (z < 0) {
    z = 0;
  } else if (z >= 512) {
    z = 511;
  }
  col = (x / 16) * 2;
  row = (z / 16) * 32;
  byte_offset = col + row;
  attribute = *(s16*)((u8*)D_800BAC0C + byte_offset);
  if (attribute == -1) {
    return -1;
  }
  if (attribute >= 0) {
    return attribute;
  }
  if (x & 1) {
    s32 offset = ((x & 15) >> 1) + ((z & 15) << 3);
    offset += (attribute & 0x7FFF) << 7;
    result = D_80191620[offset] & 15;
  } else {
    s32 offset = ((x & 15) >> 1) + ((z & 15) << 3);
    offset += (attribute & 0x7FFF) << 7;
    result = D_80191620[offset] >> 4;
  }
  return result;
}

typedef struct {
  s8 r, g, b, alpha;
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

s32 ci8_to_rgba5551(s32 index) {
  s32 palette_idx = 0;

  switch (index) {
    case 0:
      palette_idx = 0;
      break;
    case 6:
      palette_idx = 1;
      break;
    case 2:
      palette_idx = 2;
      break;
    case 1:
      palette_idx = 3;
      break;
    case 3:
      palette_idx = 4;
      break;
    case 7:
      palette_idx = 5;
      break;
    case 4:
      palette_idx = 6;
      break;
    case 8:
      palette_idx = 8;
      break;
    case 9:
      palette_idx = 9;
      break;
    case 5:
      palette_idx = 10;
      break;
  }
  /* Member-rooted scalar walks retain the retail separate address roots. */
  palette_idx *= sizeof(ColorEntry);
  {
    s32 r = *(&D_800BB040[0].r + palette_idx);
    s32 g = *(&D_800BB040[0].g + palette_idx);
    s32 b = *(&D_800BB040[0].b + palette_idx);
    return (r << 11) | (g << 6) | (b << 1) | *(&D_800BB040[0].alpha + palette_idx);
  }
}

s32 func_800402F4(s32 color) {
  s32 palette_index = find_closest_palette_index(color);

  switch (palette_index) {
    case 0:
      return 0;
    case 1:
      return 6;
    case 2:
      return 2;
    case 3:
      return 1;
    case 4:
      return 3;
    case 6:
      return 4;
    case 7:
      return 0;
    case 8:
      return 8;
    case 9:
      return 9;
    case 10:
      return 5;
    default:
      return 7;
  }
}

void load_club_offset_pair(TerrainAttrEntry* terrain, s32* state,
                           s32* out) {
  s32 mask = resolve_club_terrain_mask(1, terrain);
  if (state[2] & mask) {
    out[0] = (s8)terrain->traction_drag;
    out[1] = (s8)terrain->traction_grip;
  } else {
    out[0] = (s8)terrain->traction_drag - (s8)terrain->slope_roughness;
    if (out[0] < 5) {
      out[0] = 5;
    }
    out[1] = (s8)terrain->traction_grip - (s8)terrain->slope_roughness;
    if (out[1] < 5) {
      out[1] = 5;
    }
  }
}

const char D_800CA984[12] = "ratio %f,%x";

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", get_ground_attribute);

f32 func_800407D8(s32 arg) { return (f32)arg; }

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", get_dominant_terrain_attribute);

s32 query_terrain_at_position(s32 x, s32 z) {
  return get_dominant_terrain_attribute(x, z, 0);
}

void* func_80040E3C(s32 x, s32 z) {
  return get_table_entry(query_terrain_at_position(x, z));
}

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
  block = (((u32)gx >> 5) + ((u32)gz >> 5) * 8) * sizeof(TerrainTileCache);
  sx = gx & 0x1F;
  sz = gz & 0x1F;
  /* Preserve the retail member-rooted scalar address and 36-halfword stride. */
  base = &g_terrain_tile_cache[0].colors[1][1];
  return *(u16*)((u8*)(base + (sx + sz * 36)) + block) | 1;
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", blend_terrain_color);

s32 func_80041058(s32 x, s32 unused, s32 z) {
  return query_terrain_at_position(x, z) == 5;
}

void average_grid_vertices(s16* a, s16* b, s16* out) {
  GridVertex* av = (GridVertex*)a;
  GridVertex* bv = (GridVertex*)b;
  GridVertex* ov = (GridVertex*)out;
  ov->x = (av->x + bv->x) / 2;
  ov->height = (av->height + bv->height) / 2;
  ov->z = (av->z + bv->z) / 2;
  ov->vertex_flags = 0;
  ov->texture_s = (av->texture_s + bv->texture_s) / 2;
  ov->texture_t = (av->texture_t + bv->texture_t) / 2;
  ov->color_r = (av->color_r + bv->color_r) / 2;
  ov->color_g = (av->color_g + bv->color_g) / 2;
  ov->color_b = (av->color_b + bv->color_b) / 2;
  ov->color_a = (av->color_a + bv->color_a) / 2;
}

void lerp_grid_vertices(s16* a, s16* b, s16* out, s32 t) {
  GridVertex* av = (GridVertex*)a;
  GridVertex* bv = (GridVertex*)b;
  GridVertex* ov = (GridVertex*)out;
  ov->x = (av->x * (16 - t) + bv->x * t) / 16;
  ov->height = (av->height * (16 - t) + bv->height * t) / 16;
  ov->z = (av->z * (16 - t) + bv->z * t) / 16;
  ov->vertex_flags = 0;
  ov->texture_s = (av->texture_s * (16 - t) + bv->texture_s * t) / 16;
  ov->texture_t = (av->texture_t * (16 - t) + bv->texture_t * t) / 16;
  ov->color_r = (av->color_r * (16 - t) + bv->color_r * t) / 16;
  ov->color_g = (av->color_g * (16 - t) + bv->color_g * t) / 16;
  ov->color_b = (av->color_b * (16 - t) + bv->color_b * t) / 16;
  ov->color_a = (av->color_a * (16 - t) + bv->color_a * t) / 16;
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
  GridVertex* vtx = (GridVertex*)get_direct_grid_vertex(col, row);
  vtx->x = col << 8;
  vtx->z = row << 8;
  vtx->height = height & 0xFFFE;
  vtx->vertex_flags = 0;
  vtx->texture_s = (col << 10) + 0x20;
  vtx->texture_t = (row << 10) + 0x20;
  if (row == 0x20) {
    vtx->texture_t = 0x7FFF;
  }
  vtx->color_r = color;
  vtx->color_g = color;
  vtx->color_b = color;
  vtx->color_a = 0xFF;
}

void func_80041464(s32 x, s32 z, s32 value) {
  GridVertex* vtx = (GridVertex*)get_direct_grid_vertex(x, z);
  vtx->height = value & 0xFFFE;
}

void func_80041490(s32 x, s32 z, s32 value) {
  GridVertex* vtx = (GridVertex*)get_direct_grid_vertex(x, z);
  vtx->color_r = value;
  vtx->color_g = value;
  vtx->color_b = value;
}

void set_direct_grid_vertex(s32 col, s32 row, GridVertex* src) {
  GridVertex* dst = (GridVertex*)get_direct_grid_vertex(col, row);
  *dst = *src;
}

extern u8 D_800BA9B0[][8];
extern u8 D_800BA9D0[];
extern u8 D_800BA9B4[];

/* Odd grid coordinates use the subdivided tiles. The far row/column select
 * the preceding tile's edge vertex; even coordinates use the direct grid.
 * Coordinate/address scratch reuse preserves the retail scheduling. */
s16* get_terrain_vertex_pointer(u32 col, u32 row) {
  if ((col | row) & 1) {
    u32 row_hi, row_lo, col_hi, col_lo;
    if (row == 0x40) {
      row_hi = 0xF;
      row_lo = 4;
    } else {
      row_hi = row >> 2;
      row_lo = row & 3;
    }
    if (col == 0x20) {
      col_hi = 7;
      col_lo = 4;
    } else {
      col_hi = col >> 2;
      col_lo = col & 3;
    }
    row_hi <<= 3;
    row_hi += col_hi;
    row_hi *= sizeof(Tile);
    col = (u32)D_800BA9B0;
    col_hi = row_lo * sizeof(D_800BA9B0[0]);
    col_hi += col;
    col_hi += col_lo;
    col_hi = *(u8*)col_hi;
    col = (u32)&D_80185220[0].vertices[0];
    row_hi += col;
    col_hi *= sizeof(GridVertex);
    return (s16*)(row_hi + col_hi);
  }
  return get_direct_grid_vertex(col >> 1, row >> 1);
}

#define PUT_LOD(tile, k)                             \
  {                                                  \
    sel = g_terrain_tile_cache[tile].lod_tile_index; \
    if (sel >= 0) {                                  \
      Tile* tile_record = &D_80185220[sel];          \
      dst = tile_record->vertices + (k);             \
      *dst = *src;                                   \
    }                                                \
  }

s32 set_lod_grid_vertex(s32 col, s32 row, GridVertex* src) {
  s32 tile;
  s32 sel;
  GridVertex* dst;

  if ((col | row) & 1) {
    if (row < 0x40 && col < 0x20) {
      tile = (row / 4) * 8 + col / 4;
      PUT_LOD(tile, D_800BA9B0[row & 3][col & 3]);
    }
    if (col & 1) {
      if ((row & 3) == 0) {
        s32 rt = row / 4;
        if (rt > 0) {
          tile = (rt - 1) * 8 + col / 4;
          PUT_LOD(tile, D_800BA9D0[col & 3]);
        }
      }
    }
    if (row & 1) {
      if ((col & 3) == 0) {
        s32 ct = col / 4;
        if (ct > 0) {
          s32 base = (row / 4) * 8 - 1;
          tile = base + ct;
          PUT_LOD(tile, D_800BA9B4[(row & 3) * 8]);
        }
      }
    }
  } else {
    set_direct_grid_vertex(col / 2, row / 2, src);
  }
}

#undef PUT_LOD

s32 set_lod_tile_attribute(s32 x, s32 z, s32 attr, s32 val) {
  s32 row;
  s32 sel;
  GridVertex* p;

  if ((x | z) & 1) {
    if ((z < 0x40) & (x < 0x20)) {
      row = (z / 4) * 8 + x / 4;
      sel = g_terrain_tile_cache[row].lod_tile_index;
      if (sel >= 0) {
        Tile* t = &D_80185220[sel];
        p = &t->vertices[D_800BA9B0[z & 3][x & 3]];
        p->height = attr;
        p->color_r = p->color_g = p->color_b = val;
      }
    }
    if (x & 1) {
      if ((z & 3) == 0) {
        s32 zt = z / 4;
        if (zt > 0) {
          row = (zt - 1) * 8 + x / 4;
          sel = g_terrain_tile_cache[row].lod_tile_index;
          if (sel >= 0) {
            Tile* t = &D_80185220[sel];
            p = &t->vertices[D_800BA9D0[x & 3]];
            p->height = attr;
            p->color_r = p->color_g = p->color_b = val;
          }
        }
      }
    }
    if (z & 1) {
      if ((x & 3) == 0) {
        s32 xt = x / 4;
        if (xt > 0) {
          s32 base = (z / 4) * 8 - 1;
          row = base + xt;
          sel = g_terrain_tile_cache[row].lod_tile_index;
          if (sel >= 0) {
            Tile* t = &D_80185220[sel];
            p = &t->vertices[D_800BA9B4[(z & 3) * 8]];
            p->height = attr;
            p->color_r = p->color_g = p->color_b = val;
          }
        }
      }
    }
  } else {
    func_80041464(x / 2, z / 2, attr);
  }
}

extern f32 g_terrain_tile_radius[];
extern void func_80041D44(void);

void func_80041AA8(void) {
  s32 tile;
  s32 col;
  s32 row;
  s32 grid_col;
  s32 grid_row;
  TerrainTileCache* cache;

  for (tile = 0; tile < 0x80; tile++) {
    cache = &g_terrain_tile_cache[tile];
    g_terrain_tile_radius[tile] = 1.0f;
    cache->cull_state = 1;
    cache->lod_tile_index = -1;
    cache->unknown_990 = 0;
    for (col = 0; col < 0x20; col++) {
      for (row = 0; row < 0x20; row++) {
        /* Retail fills only the first 1024 halfwords, not the full grid. */
        ((u16*)cache->colors)[col * 0x20 + row] = 0x7C1;
      }
    }
  }
  for (grid_row = 0; grid_row < 0x21; grid_row++) {
    for (grid_col = 0; grid_col < 0x11; grid_col++) {
      init_grid_vertex(grid_col, grid_row, 0, 0x80);
    }
  }
  func_80041D44();
}

void func_80041B98(GridVertex* out, s32 tx, s32 tz) {
  s16* grid[9];
  s32 i;
  s32 row;
  s32 col;

  i = 0;
  for (row = 0; row < 3; row++) {
    for (col = 0; col < 3; col++) {
      grid[i] = get_direct_grid_vertex((tx << 1) + col, (tz << 1) + row);
      i++;
    }
  }

  average_grid_vertices(grid[0], grid[1], (s16*)&out[0]);
  average_grid_vertices(grid[1], grid[2], (s16*)&out[1]);
  average_grid_vertices(grid[0], grid[3], (s16*)&out[2]);
  average_grid_vertices(grid[1], grid[4], (s16*)&out[9]);
  average_grid_vertices(grid[2], grid[5], (s16*)&out[3]);
  average_grid_vertices(grid[3], grid[4], (s16*)&out[11]);
  average_grid_vertices(grid[4], grid[5], (s16*)&out[12]);
  average_grid_vertices(grid[3], grid[6], (s16*)&out[4]);
  average_grid_vertices(grid[4], grid[7], (s16*)&out[14]);
  average_grid_vertices(grid[5], grid[8], (s16*)&out[5]);
  average_grid_vertices(grid[6], grid[7], (s16*)&out[6]);
  average_grid_vertices(grid[7], grid[8], (s16*)&out[7]);
  average_grid_vertices(grid[1], grid[3], (s16*)&out[8]);
  average_grid_vertices(grid[2], grid[4], (s16*)&out[10]);
  average_grid_vertices(grid[4], grid[6], (s16*)&out[13]);
  average_grid_vertices(grid[5], grid[7], (s16*)&out[15]);
}

extern s32 g_terrain_tile_lod_selector[];
extern f32 D_80133698[];
extern f32 D_8013369C[];
extern f32 D_801336A0[];

void func_80041D44(void)
{
  s32 tile = 0;
  s32 row = 0;
  s32 col;
  s32 offset;
  s32 new_var;
  s16 *vertex;
  do
  {
    for (col = 0; col < 8; col++)
    {
      GridVertex *out;
      s32 *selector;
      offset = tile * (sizeof(TerrainTileCache));
      ;
      new_var = offset;
      out = (*(&D_80185220[tile])).vertices;
      *((s32 *) (((u8 *) g_terrain_tile_lod_selector) + offset)) = tile;
      vertex = get_direct_grid_vertex((col * 2) + 1, (row * 2) + 1);
      *((f32 *) (((u8 *) D_80133698) + new_var)) = vertex[0];
      *((f32 *) (((u8 *) D_8013369C) + new_var)) = vertex[1];
      *((f32 *) (((u8 *) D_801336A0) + new_var)) = vertex[2];
      if ((*((s32 *) (((u8 *) g_terrain_tile_lod_selector) + offset))) >= 0)
      {
        func_80041B98(out, col, row);
      }
      tile++;
    }

  }
  while ((++row) < 16);
}

extern s32 D_800BB080;
extern s32 D_800BB084;
extern s32 D_800BB088;
extern u16 D_800FED18;
const char D_800CAA7C[8] = "ROM :";
const char D_800CAA84[8] = "SRAM:";
const char D_800CAA8C[8] = "%x0|";
const char D_800CAA94[4] = "%X";
extern char D_801B5588[];
extern char D_8012F4E8[];
extern void clear_text_grid(void);
extern void check_and_print_grid(char* str, s32 col, s32 row);
extern void set_flag_based_on_param(s32 value);
extern s32 flag_is_set(s32 flag);
extern void flag_set(s32 flag);
extern void flag_clear(s32 flag);

/* Debug flag-grid viewer. Nesting preserves the retail helper's static chain
 * and places its body at 0x80041E8C, immediately before this parent. */
void func_80041EC0(void) {
  char buffer[16];
  s32 row;
  s32 bit;
  s32 column;
  u16 buttons;

  s32 selected_flag(void) {
    return ((D_800BB080 + D_800BB088) << 4) + D_800BB084;
  }

  clear_text_grid();
  check_and_print_grid((char*)D_800CAA7C, 3, 4);
  check_and_print_grid(D_801B5588, 8, 4);
  check_and_print_grid((char*)D_800CAA84, 3, 5);
  check_and_print_grid(D_8012F4E8, 8, 5);
  for (row = 0; row < 8; row++) {
    sprintf(buffer, D_800CAA8C, row + D_800BB080);
    check_and_print_grid(buffer, 3, row + 14);
    for (bit = 0, column = 6; bit < 16; bit++, column += 2) {
      if (row == 0) {
        sprintf(buffer, D_800CAA94, bit);
        check_and_print_grid(buffer, column, 12);
      }
      sprintf(buffer, D_800CAA94, flag_is_set(((row + D_800BB080) << 4) + bit));
      if (row == D_800BB088 && bit == D_800BB084) {
        set_flag_based_on_param(1);
      }
      check_and_print_grid(buffer, column, row + 14);
    }
  }
  buttons = D_800FED18;
  if (buttons & 0x100) {
    D_800BB084++;
  }
  if (buttons & 0x200) {
    D_800BB084--;
  }
  if (D_800BB084 < 0) {
    D_800BB084 = 15;
    D_800BB088--;
  }
  if (D_800BB084 >= 16) {
    D_800BB084 = 0;
    D_800BB088++;
  }
  if (buttons & 0x800) {
    D_800BB088--;
  }
  if (buttons & 0x400) {
    D_800BB088++;
  }
  if (buttons & 0x10) {
    D_800BB080 += 8;
  }
  if (buttons & 0x20) {
    D_800BB080 -= 8;
  }
  while (1) {
    s32 cursor_row = D_800BB088;
    if (cursor_row < 0) {
      D_800BB088 = cursor_row + 1;
      D_800BB080--;
    } else if (cursor_row >= 8) {
      D_800BB088 = cursor_row - 1;
      D_800BB080++;
    } else {
      break;
    }
  }
  if (D_800BB080 > 8) {
    D_800BB080 = 8;
  }
  if (D_800BB080 < 0) {
    D_800BB080 = 0;
  }
  if (D_800FED18 & 0x8000) {
    flag_set(selected_flag());
  }
  if (D_800FED18 & 0x4000) {
    flag_clear(selected_flag());
  }
}

const char D_800CAA98[8] = "FLAGS ";
const char D_800CAAA0[8] = "ABOUT ";
const char D_800CAAA8[8] = " VIEW ";
const char D_800CAAB0[8] = " PLAY ";
const char D_800CAAB8[8] = "HEIGHT";
const char D_800CAAC0[8] = " TREE ";
const char D_800CAAC8[8] = " PIN  ";
const char D_800CAAD0[8] = "DIVINF";
const char D_800CAAD8[8] = "TRIANG";
const char D_800CAAE0[8] = "MENU";
const char D_800CAAE8[8] = "XZ-DIV";
const char D_800CAAF0[8] = " FILE ";
const char D_800CAAF8[8] = "STROKE";
const char D_800CAB00[8] = "S-SHOT";
const char D_800CAB08[8] = "RING  ";
const char D_800CAB10[8] = "WALL  ";
const char D_800CAB18[8] = "      ";

const char* func_80042228(u32 arg0) {
  switch (arg0) {
    case 8:
      return D_800CAA98;
    case 0:
      return D_800CAAA0;
    case 2:
      return D_800CAAA8;
    case 7:
      return D_800CAAB0;
    case 1:
      return D_800CAAB8;
    case 3:
      return D_800CAAC0;
    case 5:
      return D_800CAAC8;
    case 9:
      return D_800CAAD0;
    case 15:
      return D_800CAAD8;
    case 14:
      return D_800CAAE0;
    case 10:
      return D_800CAAE8;
    case 11:
      return D_800CAAF0;
    case 12:
      return D_800CAAF8;
    case 13:
      return D_800CAB00;
    case 16:
      return D_800CAB08;
    case 28:
      return D_800CAB10;
    default:
      return D_800CAB18;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/get_tile_attribute", func_80042318);

extern GridVertex D_8018D228[];
extern u8 D_8018D258[];
extern u8 D_8018D25A[];
extern u8 D_8018D25C[];
extern u8 D_8018D25E[];
extern u8 D_8018D260[];
extern u8 D_8018D262[];

void func_800425C8(void) {
  s32 min_bound = 0x20000000;
  s32 max_bound = -0x20000000;
  s32 bounds[7];
  s32 vertex;
  GridVertex* row;
  s32 row_offset;

  row_offset = 0;
  row = D_8018D228;
  do {
    register GridVertex* point __asm__("$4");

    vertex = 0;
    point = row;
    bounds[2] = min_bound;
    bounds[1] = min_bound;
    bounds[0] = min_bound;
    bounds[6] = max_bound;
    bounds[5] = max_bound;
    bounds[4] = max_bound;
    do {
      s16 x = point->x;
      if (x < bounds[0]) {
        bounds[0] = x;
      }
      x = point->x;
      if (bounds[4] < x) {
        bounds[4] = x;
      }
      x = point->height;
      if (x < bounds[1]) {
        bounds[1] = x;
      }
      x = point->height;
      if (bounds[5] < x) {
        bounds[5] = x;
      }
      x = point->z;
      if (x < bounds[2]) {
        bounds[2] = x;
      }
      x = point->z;
      if (bounds[6] < x) {
        bounds[6] = x;
      }
      point->texture_s = point->x;
      point->texture_t = point->z;
      point++;
      vertex++;
    } while (vertex < 3);
    *(s16*)(D_8018D258 + row_offset) = bounds[0];
    *(s16*)(D_8018D25C + row_offset) = bounds[1];
    *(s16*)(D_8018D260 + row_offset) = bounds[2];
    *(s16*)(D_8018D25A + row_offset) = bounds[4];
    *(s16*)(D_8018D25E + row_offset) = bounds[5];
    *(s16*)(D_8018D262 + row_offset) = bounds[6];
    row_offset += 0x60;
    row += 6;
  } while (row_offset < 0x3C00);
  update_rumble_intensity_table();
}

s32 func_80042720(void) { return (u32)&load_hole_terrain_assets > 0x803FFFFFU; }

extern SceneryCylinder collision_cylinders[];

void mark_scenery_collision_cells(void) {
  s32 mark = 0xF801;
  u16* base = &g_terrain_tile_cache[0].colors[1][1];
  s32 i;

  for (i = 0; i < 128; i++) {
    s32 gx, gz;
    s32 sx, sz;
    s32 block;

    if (collision_cylinders[i].sprite_index < 0) {
      continue;
    }
    gx = collision_cylinders[i].x / 16;
    gz = collision_cylinders[i].z / 16;
    block = (((u32)gx >> 5) + ((u32)gz >> 5) * 8) * sizeof(TerrainTileCache);
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

extern s8 g_terrain_vtx_xform_mode;
extern s8 D_800BB0EC[];
const char D_800CAB9C[16] = "GetHeightInfo";
extern void check_and_print_grid(char* str, s32 col, s32 row);

/* detect_terrain_collision: 4x4-cell quad->2-triangle point location; fills the
 * s16 tri[11] buffer get_interpolated_terrain_height reads. Uses the GNU
 * NESTED-FUNCTION extension: the nested inline fetch()/hit() helpers
 * reference x/z/out as free variables, which is what homes the params + emits
 * the arg pointer the ROM has. `vunused` is a byte-exact dead 0x14 stack slot
 * the ROM reserves.
 * CAVEAT: a nested function makes import.py/pycparser (the permuter)
 * unavailable for the WHOLE get_tile_attribute.c TU. */
s32 detect_terrain_collision(s32 x, s32 z, s16* out) {
  s32 col_base, row_base, col_frac, row_frac;
  s32 ri, ci, col, row;
  s16* vunused;
  s16 *va, *vb, *vc, *vd;

  inline void fetch(s16 * *p, s32 c, s32 r) {
    *p = get_terrain_vertex_pointer(c, r);
  }

  inline s32 hit(s16 * a, s16 * b, s16 * c) {
    if ((b[0] - a[0]) * (z - (a[2] << 10)) -
                (b[2] - a[2]) * (x - (a[0] << 10)) <
            0 ||
        (c[0] - b[0]) * (z - (b[2] << 10)) -
                (c[2] - b[2]) * (x - (b[0] << 10)) <
            0 ||
        (a[0] - c[0]) * (z - (c[2] << 10)) -
                (a[2] - c[2]) * (x - (c[0] << 10)) <
            0) {
      return 0;
    }
    out[2] = a[0];
    out[3] = b[0];
    out[4] = c[0];
    out[5] = a[1];
    out[6] = b[1];
    out[7] = c[1];
    out[8] = a[2];
    out[9] = b[2];
    out[10] = c[2];
    return 1;
  }

  (void)&vunused;

  if (g_terrain_vtx_xform_mode != 0) {
    if ((u32)x > 0xFFFFF) {
      return -1;
    }
    if ((u32)z > 0xFFFFF) {
      return -1;
    }
  } else {
    if ((u32)x > 0x3FFFFF) {
      return -1;
    }
    if ((u32)z > 0x7FFFFF) {
      return -1;
    }
  }

  if (g_terrain_vtx_xform_mode != 0) {
    col_base = x >> 17;
    row_base = z >> 16;
    col_base <<= 2;
    row_base <<= 2;
    col_frac = (x >> 15) & 3;
    row_frac = (z >> 14) & 3;
  } else {
    col_base = x >> 19;
    row_base = z >> 19;
    col_base <<= 2;
    row_base <<= 2;
    col_frac = (x >> 17) & 3;
    row_frac = (z >> 17) & 3;
  }

  for (ri = 0; ri < 4; ri++) {
    row = row_base + ((row_frac + D_800BB0EC[ri]) & 3);
    for (ci = 0; ci < 4; ci++) {
      col = col_base + ((col_frac + D_800BB0EC[ci]) & 3);
      fetch(&va, col, row);
      fetch(&vb, col + 1, row);
      fetch(&vc, col, row + 1);
      if (hit(va, vb, vc)) {
        return 0;
      }
      fetch(&vd, col + 1, row + 1);
      if (hit(vb, vd, vc)) {
        return 0;
      }
    }
  }

  check_and_print_grid((char*)D_800CAB9C, 0xA, 0xB);
  return -1;
}

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

const char D_800CABAC[12] = "NormalError";

double sqrt(double);

s32 func_8004333C(s16* triangle, s32* plane) {
  s32 dy1 = triangle[6];
  s32 dz2 = triangle[10];
  s32 dy2;
  s32 dz1;
  s32 dx1;
  s32 dx2;
  s32 nx;
  s32 ny;
  s32 nz;
  s32 d;
  s32 mag;
  s32 len;

  dy1 -= triangle[5];
  dz2 -= triangle[8];
  dy2 = triangle[7] - triangle[5];
  dz1 = triangle[9] - triangle[8];
  dx1 = triangle[3] - triangle[2];
  dx2 = triangle[4] - triangle[2];
  nx = dy1 * dz2 - dy2 * dz1;
  ny = -dx1 * dz2 + dx2 * dz1;
  nz = -dy1 * dx2 + dy2 * dx1;
  d = -(nx * triangle[2] + ny * triangle[5] + nz * triangle[8]);

  for (;;) {
    mag = nx;
    if (nx < 0) {
      mag = -nx;
    }
    if (mag < 0x4001) {
      mag = ny;
      if (ny < 0) {
        mag = -ny;
      }
      if (mag < 0x4001) {
        mag = nz;
        if (nz < 0) {
          mag = -nz;
        }
        if (mag < 0x4001) {
          break;
        }
      }
    }
    nx /= 256;
    ny /= 256;
    nz /= 256;
    d /= 256;
  }

  len = (s32) sqrt((f64) (nx * nx + ny * ny + nz * nz));
  if (len == 0) {
    check_and_print_grid((char*)D_800CABAC, 3, 0xF);
    plane[0] = 0;
    plane[1] = 0x100;
    plane[2] = 0;
    plane[3] = 0;
    plane[4] = 0x100;
    return -1;
  }
  if (ny < 0) {
    len = -len;
  }
  plane[0] = (nx << 8) / len;
  plane[1] = (ny << 8) / len;
  plane[2] = (nz << 8) / len;
  plane[3] = (d << 8) / len;
  plane[4] = 0x100;
  return 0;
}

extern f32 sqrtf(f32);
extern s8 g_debug_mode_enabled;
const char D_800CABB8[16] = "NormalFError";
/* Preserve the original 8-byte zero tail of this C-owned rodata range. */
const char D_800CABC8[8] = {0};

s32 compute_triangle_plane(s16* triangle, f32* plane) {
  s32 dy1 = triangle[6];
  s32 dz2 = triangle[10];
  s32 dy2;
  s32 dz1;
  s32 dx1;
  s32 dx2;
  f32 nx;
  f32 ny;
  f32 nz;
  f32 length;
  f32 offset;
  f32 reciprocal;

  dy1 -= triangle[5];
  dz2 -= triangle[8];
  dy2 = triangle[7] - triangle[5];
  dz1 = triangle[9] - triangle[8];
  dx1 = triangle[3] - triangle[2];
  dx2 = triangle[4] - triangle[2];
  nx = dy1 * dz2 - dy2 * dz1;
  ny = -dx1 * dz2 + dx2 * dz1;
  nz = -dy1 * dx2 + dy2 * dx1;
  length = sqrtf(nx * nx + ny * ny + nz * nz);
  offset = -(nx * triangle[2] + ny * triangle[5] + nz * triangle[8]);

  if (length == 0.0f) {
    if (g_debug_mode_enabled != 0) {
      check_and_print_grid((char*)D_800CABB8, 3, 15);
    }
    plane[0] = 0.0f;
    plane[1] = 1.0f;
    plane[2] = 0.0f;
    plane[3] = 0.0f;
    plane[4] = 1.0f;
    return -1;
  }
  if (ny < 0.0f) length = -length;
  reciprocal = 1.0f / length;
  plane[0] = nx * reciprocal;
  plane[1] = ny * reciprocal;
  plane[2] = nz * reciprocal;
  plane[3] = offset * reciprocal;
  plane[4] = 1.0f;
  return 0;
}

s32 get_interpolated_terrain_height(s32 x, s32 z) {
  s16 tri[11];
  s32 e1x, e1y, e1z, e2x, e2y, e2z;
  s32 x0s, y0s, z0s, dx, dz;
  s32 y1;
  f32 fnx, fny, fnz;

  if (g_terrain_vtx_xform_mode != 0) {
    if ((u32)x > 0xFFFFF) {
      if (x < 0) {
        x = 0;
      } else {
        x = 0xFFFFF;
      }
    }
    if ((u32)z > 0xFFFFF) {
      if (z < 0) {
        z = 0;
      } else {
        z = 0xFFFFF;
      }
    }
  } else {
    if ((u32)x > 0x3FFFFF) {
      if (x < 0) {
        x = 0;
      } else {
        x = 0x3FFFFF;
      }
    }
    if ((u32)z > 0x7FFFFF) {
      if (z < 0) {
        z = 0;
      } else {
        z = 0x7FFFFF;
      }
    }
  }

  if (detect_terrain_collision(x, z, tri) < 0) {
    return -300;
  }

  y1 = tri[6];
  e2z = tri[10] - tri[8];
  e1y = y1 - tri[5];
  e1z = tri[9] - tri[8];
  e2y = tri[7] - tri[5];
  e1x = tri[3] - tri[2];
  e2x = tri[4] - tri[2];

  y0s = tri[5] << 10;
  x0s = tri[2] << 10;
  z0s = tri[8] << 10;
  dx = x - x0s;
  dz = z - z0s;

  fnx = (f32)(e1y * e2z + e2y * -e1z);
  fny = (f32)(e1x * -e2z + e2x * e1z);
  fnz = (f32)(e1y * -e2x + e2y * e1x);

  if ((s32)fny != 0) {
    return y0s + (s32)((-fnx * (f32)dx - fnz * (f32)dz) / fny);
  }
  return 0;
}

s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z) {
  return get_interpolated_terrain_height(x, z);
}

extern s32 find_collision_triangle(s32 x0, s32 y0, s32 z0, s32 x1, s32 y1,
                                   s32 z1, void* outA, void* outHit);

s32 get_lowest_height_at_position(s32 x, s32 z) {
  s32 a[6];
  f32 hit[0x22];
  s32 height = get_interpolated_terrain_height(x, z);
  if (find_collision_triangle(x, -3072000, z, x, 3072000, z, a, hit)) {
    s32 h = (s32)(hit[0] * 6144000.0f + -3072000.0f);
    if (h < height) {
      height = h;
    }
  }
  return height;
}
