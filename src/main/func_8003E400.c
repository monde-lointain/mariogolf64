#include "common.h"

extern s8 D_800BA9A8;
extern s8 D_800BA9A9;
extern s32 D_800FBE70;
extern s32 D_800B7780;
extern void* D_800DAD00;
extern void* D_800DAD04;
extern void* D_800DAD08;
extern Mtx D_800DAD10[];
extern s32 D_801B6088;
extern f32 D_801B54F4;
extern s32 camera_position_x;
extern s32 camera_position_y;
extern s32 camera_position_z;
extern f32 per_view_camera_state;
extern s8 g_terrain_vtx_xform_mode;
extern void* wind_dl_buffer_addr;

typedef struct {
  /* 0x00 */ u32 id;
} TerrainAttrEntry;

extern TerrainAttrEntry* get_table_entry(u32 idx);
extern double atan2(double y, double x);
extern void convert_and_pack_floats_to_fixed(f32 mf[4][4], Mtx* m);
extern void* heap3_alloc(u32 need);
extern void heap3_free(void** payload_ptr);
extern u32 func_8005062C(u16 index, void* out);
extern void func_800506D4(void* data, void* slot);
extern void mtx_from_rts(f32 mf[4][4], f32 rotate[3], f32 translate[3],
                         f32 scale[3]);

void func_8003E400(void) {
  u8 sp10[0x20];

  D_800DAD04 = heap3_alloc(func_8005062C(0x4CA, sp10));
  func_800506D4(D_800DAD04, sp10);
  D_800DAD00 = heap3_alloc(func_8005062C(0x4C9, sp10));
  func_800506D4(D_800DAD00, sp10);
  D_800DAD08 = heap3_alloc(func_8005062C(0x703, sp10));
  func_800506D4(D_800DAD08, sp10);
  wind_dl_buffer_addr = heap3_alloc(func_8005062C(0x704, sp10));
  func_800506D4(wind_dl_buffer_addr, sp10);
}

void func_8003E4B4(void) {
  heap3_free(&D_800DAD00);
  heap3_free(&D_800DAD04);
  heap3_free(&D_800DAD08);
  heap3_free(&wind_dl_buffer_addr);
}

void build_pin_or_cup_matrix_for_dad10(s32 arg0) {
  s32 arg = arg0;
  s32* camera_x_ptr = &camera_position_x;
  f32 translate[3];
  f32 rotate[3];
  f32 scale[3];
  f32 mf[4][4];
  Mtx* dst;
  f32 k = 0.0009765625f;
  f32 angle;

  angle = atan2((f32)*camera_x_ptr * k - per_view_camera_state,
                (f32)camera_position_z * k - D_801B54F4);

  translate[0] = (f32)*camera_x_ptr * k;
  translate[1] = (f32)camera_position_y * k;
  translate[2] = (f32)camera_position_z * k;
  rotate[0] = 0.0f;
  rotate[1] = angle + 3.14f;
  rotate[2] = 0.0f;
  scale[0] = 0.1f;
  scale[1] = 0.1f;
  scale[2] = 0.1f;
  mtx_from_rts(mf, rotate, translate, scale);
  arg <<= 7;
  dst = (Mtx*)((u8*)D_800DAD10 + (D_800B7780 << 6));
  convert_and_pack_floats_to_fixed(mf, (Mtx*)((u8*)arg + (u32)dst));
}

void func_8003E628(void) { D_800BA9A8 = 1; }

void func_8003E638(void) { D_800BA9A9 = 1; }

s32 func_8003E648(void) {
  if (D_800BA9A8 != 0 || D_800BA9A9 != 0) {
    return 0;
  }

  if ((u32)(D_801B6088 - 6) >= 2) {
    return 1;
  }

  if (g_terrain_vtx_xform_mode != 0) {
    return 1;
  }

  return get_table_entry(D_800FBE70)->id != 0xB;
}
