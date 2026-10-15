#include "common.h"

/* Preserve the retail approximation rather than substituting a different pi. */
#define PIN_MARKER_YAW_OFFSET 3.14f
#define PIN_MARKER_SCALE 0.1f

enum {
  PIN_MARKER_NEAR_ASSET_ID = 0x4CA,
  PIN_MARKER_FAR_ASSET_ID = 0x4C9,
  PIN_ASSEMBLY_ASSET_ID = 0x703,
  PIN_WIND_ASSET_ID = 0x704,
  PIN_MARKER_BUFFER_COUNT = 2,
  PIN_HIDDEN_TERRAIN_ID = 11
};

typedef struct {
  u32 start;
  u32 size;
  u32 pos;
  u32 end;
} RomLoadSlot;

extern s8 pin_flag_suppressed;
extern s8 pin_assembly_suppressed;
extern s32 active_terrain_index;
extern s32 render_buffer_index;
extern void* pin_marker_far_texture;
extern void* pin_marker_near_texture;
extern void* pin_assembly_texture;
extern Mtx pin_marker_matrices[][PIN_MARKER_BUFFER_COUNT];
extern s32 scenario_state_id;
extern f32 view_camera_eye_z;
extern s32 camera_position_x;
extern s32 camera_position_y;
extern s32 camera_position_z;
extern f32 view_camera_eye_x;
extern s8 g_terrain_vtx_xform_mode;
extern void* pin_wind_texture;

typedef struct {
  /* 0x00 */ u32 id;
} TerrainAttrEntry;

extern TerrainAttrEntry* get_table_entry(u32 index);
extern double atan2(double y, double x);
extern void convert_and_pack_floats_to_fixed(f32 matrix[4][4],
                                             Mtx* fixed_matrix);
extern void* heap3_alloc(u32 size);
extern void heap3_free(void** payload);
extern u32 get_rom_asset_allocation_size(u16 asset_id, void* load_slots);
extern void load_rom_asset_payload(void* destination, RomLoadSlot* load_slots);
extern void mtx_from_rts(f32 matrix[4][4], f32 rotation[3], f32 translation[3],
                         f32 scale[3]);

void load_pin_render_assets(void) {
  RomLoadSlot load_slots[2];

  pin_marker_near_texture =
      heap3_alloc(get_rom_asset_allocation_size(PIN_MARKER_NEAR_ASSET_ID, load_slots));
  load_rom_asset_payload(pin_marker_near_texture, load_slots);

  pin_marker_far_texture =
      heap3_alloc(get_rom_asset_allocation_size(PIN_MARKER_FAR_ASSET_ID, load_slots));
  load_rom_asset_payload(pin_marker_far_texture, load_slots);

  pin_assembly_texture =
      heap3_alloc(get_rom_asset_allocation_size(PIN_ASSEMBLY_ASSET_ID, load_slots));
  load_rom_asset_payload(pin_assembly_texture, load_slots);

  pin_wind_texture = heap3_alloc(get_rom_asset_allocation_size(PIN_WIND_ASSET_ID, load_slots));
  load_rom_asset_payload(pin_wind_texture, load_slots);
}

void free_pin_render_assets(void) {
  heap3_free(&pin_marker_far_texture);
  heap3_free(&pin_marker_near_texture);
  heap3_free(&pin_assembly_texture);
  heap3_free(&pin_wind_texture);
}

void build_pin_marker_matrix(s32 view_index) {
  /* Keep the address live across atan2 to preserve the retail reload sequence.
   */
  s32* camera_x = &camera_position_x;
  const f32 fixed_to_world = 1.0f / 1024.0f;
  f32 facing_angle =
      atan2((f32)*camera_x * fixed_to_world - view_camera_eye_x,
            (f32)camera_position_z * fixed_to_world - view_camera_eye_z);

  /* GCC 2.7.2 requires these arrays before the first statement. */
  f32 translation[3];
  f32 rotation[3];
  f32 scale[3];
  f32 matrix[4][4];

  translation[0] = (f32)*camera_x * fixed_to_world;
  translation[1] = (f32)camera_position_y * fixed_to_world;
  translation[2] = (f32)camera_position_z * fixed_to_world;

  rotation[0] = 0.0f;
  rotation[1] = facing_angle + PIN_MARKER_YAW_OFFSET;
  rotation[2] = 0.0f;

  scale[0] = PIN_MARKER_SCALE;
  scale[1] = PIN_MARKER_SCALE;
  scale[2] = PIN_MARKER_SCALE;
  mtx_from_rts(matrix, rotation, translation, scale);

  convert_and_pack_floats_to_fixed(
      matrix, &pin_marker_matrices[view_index][render_buffer_index]);
}

/* The renderer consumes each suppression request on its next invocation. */
void suppress_pin_flag_once(void) { pin_flag_suppressed = 1; }

void suppress_pin_assembly_once(void) { pin_assembly_suppressed = 1; }

s32 should_draw_pin_flag(void) {
  if (pin_flag_suppressed != 0 || pin_assembly_suppressed != 0) {
    return 0;
  }

  if (scenario_state_id != 6 && scenario_state_id != 7) {
    return 1;
  }

  if (g_terrain_vtx_xform_mode != 0) {
    return 1;
  }

  return get_table_entry(active_terrain_index)->id != PIN_HIDDEN_TERRAIN_ID;
}
