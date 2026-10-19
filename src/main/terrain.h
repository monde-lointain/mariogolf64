#ifndef TERRAIN_H
#define TERRAIN_H

#include "common.h"
#include "vector.h"

/* Terrain attribute table entry: 0x1C-byte records, 27 entries. */
typedef struct {
  /* 0x00 */ u32 id;
  /* 0x04 */ char* name_ptr;
  /* 0x08 */ u8 traction_drag;
  /* 0x09 */ u8 traction_grip;
  /* 0x0A */ u8 slope_roughness;
  /* 0x0B */ u8 pad_0b;
  /* 0x0C */ s16 bounce_a;
  /* 0x0E */ s16 bounce_b;
  /* 0x10 */ u32 param;
  /* 0x14 */ u8 flag_a;
  /* 0x15 */ u8 flag_b;
  /* 0x16 */ u8 flag_c;
  /* 0x17 */ u8 penalty_class;

  /* 0x18 */ u32 sound_id;
} TerrainAttrEntry; /* 0x1C */

/* Scenery collision cylinder: 0x10-byte records with field aliases. */
typedef struct {
  /* 0x00 */ s16 x;
  /* 0x02 */ s16 terrain_y;
  /* 0x04 */ s16 z;
  /* 0x06 */ s8 sprite_index;
  /* 0x07 */ u8 billboard_vertex_set_index;
  /* 0x08 */ u8 unknown_08[2];
  /* 0x0A */ u16 cull_state;
  /* 0x0C */ u16 radius;
  /* 0x0E */ u16 scale_y;
} SceneryCylinder; /* 0x10 */

/* Direct terrain grid vertex, 0x10-byte stride. */
typedef struct {
  /* 0x00 */ s16 x;
  /* 0x02 */ s16 height;
  /* 0x04 */ s16 z;
  /* 0x06 */ s16 vertex_flags;
  /* 0x08 */ s16 texture_s;
  /* 0x0A */ s16 texture_t;
  /* 0x0C */ u8 color_r;
  /* 0x0D */ u8 color_g;
  /* 0x0E */ u8 color_b;
  /* 0x0F */ u8 color_a;
} GridVertex;

/* LOD terrain tile: 16 grid vertices, 0x100-byte stride. */
typedef struct {
  GridVertex vertices[16];
} Tile;

extern Tile D_80185220[];

enum {
  SCENERY_CYLINDER_COUNT = 0x80,
  SCENERY_SPRITE_COUNT = 46,
  SCENERY_HEAP_SLOT = 1
};

/* Texture metadata and the decoded-texture cache used by scenery billboards. */
typedef struct {
  /* 0x00 */ s32 asset_index;
  /* 0x04 */ s16 storage_mode; /* mode 2 is loaded lazily by the billboard path */
  /* 0x06 */ s16 width;
  /* 0x08 */ s16 height;
  /* 0x0A */ u8 unknown_0A[2];
  /* 0x0C */ void* decoded_texture;
} ScenerySpriteDescriptor; /* 0x10 */

extern ScenerySpriteDescriptor g_scenery_sprites[SCENERY_SPRITE_COUNT];

/* The 34x36 RGBA5551 grid ends at +0x990. D_80132D4A is colors[1][1]. */
typedef struct {
  /* 0x000 */ u16 colors[34][36];
  /* 0x990 */ s16 unknown_990;
  /* 0x992 */ s16 unknown_992;
  /* 0x994 */ s32 cull_state;
  /* 0x998 */ Vec3f position;
  /* 0x9A4 */ s32 lod_tile_index; /* -1 selects the direct grid */
  /* 0x9A8 */ u8 unknown_9A8[0x58];
} TerrainTileCache; /* 0xA00 */

extern TerrainTileCache g_terrain_tile_cache[];
extern s16 g_terrain_grid_verts[];

TerrainAttrEntry* get_table_entry(u32 index);
s32 resolve_club_terrain_mask(s32 mode, TerrainAttrEntry* terrain);

#endif /* TERRAIN_H */
