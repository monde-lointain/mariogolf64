/* S318 carry body for func_8003E004: 196/196 at the ROM's exact -0x140 frame,
 * 88 cmpfn rows. Paste over the INCLUDE_ASM stub in src/main/func_8003DFD0.c,
 * with the externs listed in func_8003E004.near-match.md. */

typedef struct {
  s32 x;
  s32 y;
  s32 z;
} WindVtxScratch;

void func_8003E004(void) {
  WindVtxScratch buf[18];
  u16* anglep = &g_scenery_wind_angle_a;
  s32* base_pos;
  f32 base;
  s32 i;
  s32 k;

  base = (f32)((f64)((f32)*anglep * 9.58738019107841e-05f) + D_800CA8E0);
  base_pos = (s32*)((u8*)anglep - 0x28);

  for (i = 0; i != 9; i++) {
    WindVtxScratch* a = &buf[i * 2];
    WindVtxScratch* b = &buf[i * 2 + 1];
    f64 t;
    f64 c;
    f32 ang_lo;
    f32 ang_hi;

    ang_lo = (f32)((t = (f64)base) - D_800CA8E8);
    a->x = (s32)(sinf(base) * i * 8533.0f + sinf(ang_lo) * (9 - i) * 46.079998f);
    a->z = (s32)(-cosf(base) * i * 8533.0f - cosf(ang_lo) * (9 - i) * 46.079998f);
    ang_hi = (f32)(t = t + D_800CA8F0);
    b->x = (s32)(sinf(base) * i * 8533.0f + sinf(ang_hi) * (9 - i) * 46.079998f);
    b->z = (s32)(-cosf(base) * i * 8533.0f - cosf(ang_hi) * (9 - i) * 46.079998f);
    a->y = get_interpolated_terrain_height_wrapper(base_pos[0] + a->x, base_pos[2] + a->z) - base_pos[1];
    b->y = get_interpolated_terrain_height_wrapper(base_pos[0] + b->x, base_pos[2] + b->z) - base_pos[1];
  }

  for (k = 0; k != 18; k++) {
    D_800BA888[k].v.ob[0] = buf[k].x / 4;
    D_800BA888[k].v.ob[1] = buf[k].y / 4;
    D_800BA888[k].v.ob[2] = buf[k].z / 4;
  }
}


