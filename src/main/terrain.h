#ifndef TERRAIN_H
#define TERRAIN_H

#include "common.h"

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
  /* 0x07 */ u8 unknown_07[3];
  /* 0x0A */ s16 cull_state;
  /* 0x0C */ u16 radius;
  /* 0x0E */ u16 scale_y;
} SceneryCylinder; /* 0x10 */

TerrainAttrEntry* get_table_entry(u32 index);
s32 resolve_club_terrain_mask(s32 mode, TerrainAttrEntry* terrain);

#endif /* TERRAIN_H */
