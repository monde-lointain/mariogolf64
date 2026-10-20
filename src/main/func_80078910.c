#include "common.h"
#include "animation.h"
#include "rom_asset.h"
#include "terrain.h"
#include "session.h"
#include "ball.h"

typedef struct {
  Vec3f position; /* 0x00 */
  Vec3f velocity; /* 0x0C */
  u8 reserved_18[0xC];
  f32 unknown_24; /* 0x24 */
  f32 opacity;
  f32 opacity_step;
  f32 size;
  f32 size_step;
  s16 unknown_38;
  s8 effect_kind; /* -1 marks an unused slot */
  s8 effect_frame;
  s8 matrix_index;
  u8 reserved_3D[0x3];
} Particle; /* size 0x40 */

extern Particle particle_array[40];
extern s32 frame_counter;
extern void mtx_from_rts(f32 matrix[4][4], f32 rotation[3], f32 translation[3],
                         f32 scale[3]);
extern void convert_and_pack_floats_to_fixed(f32 matrix[4][4],
                                             Mtx* fixed_matrix);
extern Mtx D_800FD8F0[];

extern void* heap3_alloc(u32 need);
extern void heap3_free(void** payload_ptr);

extern s32 D_800C53B8;
extern void* D_800E1DBC;
extern void* D_800E1DB4;
extern void* D_800E1DB8;
extern void* D_800E1DA0;
extern void* D_800E1DA4;
extern void* D_800E1DA8;
extern void* D_800E1DAC;
extern void* D_800E1DB0;
extern void* D_800E1DC0;
extern void* D_800E1DC4;
extern void* D_800E1DC8;
extern s32 D_800C5458;
extern s32 D_800C545C;
extern s32 D_800C5460;
extern void* D_800E1DCC;
extern f32 view_camera_eye_x;
extern f32 view_camera_eye_z;
extern f32 D_801B54F8;
extern f32 D_801B5500;
extern s32 D_800FF418;
extern s32 D_800FF41C;
extern char D_800D1A78[];

void func_80078910(void) {
  if (D_800C53B8 != 0) {
    heap3_free(&D_800E1DA0);
    heap3_free(&D_800E1DA4);
    heap3_free(&D_800E1DA8);
    heap3_free(&D_800E1DAC);
    heap3_free(&D_800E1DB0);
    heap3_free(&D_800E1DB4);
    heap3_free(&D_800E1DB8);
    heap3_free(&D_800E1DBC);
    heap3_free(&D_800E1DC0);
    heap3_free(&D_800E1DC4);
    heap3_free(&D_800E1DC8);
    heap3_free(&D_800E1DCC);
    D_800C53B8 = 0;
  }
}

void func_800789C8(void) {
  RomAssetLoad spvar;

  func_80078910();
  if (D_800C53B8 == 0) {
    D_800C53B8 = 1;
    D_800E1DBC = heap3_alloc(get_rom_asset_allocation_size(0x6B5, &spvar));
    load_rom_asset_payload(D_800E1DBC, &spvar);
    D_800E1DB4 = heap3_alloc(get_rom_asset_allocation_size(0x6B5, &spvar));
    load_rom_asset_payload(D_800E1DB4, &spvar);
    D_800E1DB8 = heap3_alloc(get_rom_asset_allocation_size(0x6B8, &spvar));
    load_rom_asset_payload(D_800E1DB8, &spvar);
    D_800E1DA0 = heap3_alloc(get_rom_asset_allocation_size(0x6B0, &spvar));
    load_rom_asset_payload(D_800E1DA0, &spvar);
    D_800E1DA4 = heap3_alloc(get_rom_asset_allocation_size(0x8EC, &spvar));
    load_rom_asset_payload(D_800E1DA4, &spvar);
    D_800E1DA8 = heap3_alloc(get_rom_asset_allocation_size(0x6B2, &spvar));
    load_rom_asset_payload(D_800E1DA8, &spvar);
    D_800E1DAC = heap3_alloc(get_rom_asset_allocation_size(0x6C0, &spvar));
    load_rom_asset_payload(D_800E1DAC, &spvar);
    D_800E1DB0 = heap3_alloc(get_rom_asset_allocation_size(0x6C1, &spvar));
    load_rom_asset_payload(D_800E1DB0, &spvar);
    D_800E1DC0 = heap3_alloc(get_rom_asset_allocation_size(0x71D, &spvar));
    load_rom_asset_payload(D_800E1DC0, &spvar);
    D_800E1DC4 = heap3_alloc(get_rom_asset_allocation_size(0x6C2, &spvar));
    load_rom_asset_payload(D_800E1DC4, &spvar);
    D_800E1DC8 = heap3_alloc(get_rom_asset_allocation_size(0x69E, &spvar));
    load_rom_asset_payload(D_800E1DC8, &spvar);
    D_800E1DCC = heap3_alloc(get_rom_asset_allocation_size(0x93B, &spvar));
    load_rom_asset_payload(D_800E1DCC, &spvar);
  }
}

extern s32 D_800FBE64[3];
extern u32 target_z;
extern s32 active_terrain_index;
extern s32 D_800C52B0;
extern s32 D_800FC8B8;

extern void func_80216B10(void);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);

void reload_scene_assets(BallObject *arg0) {
  RomAssetLoad sp10;
  s32 i;

  get_rom_asset_allocation_size(0x6B5, &sp10);
  load_rom_asset_payload(D_800E1DB4, &sp10);
  get_rom_asset_allocation_size(0x6B8, &sp10);
  load_rom_asset_payload(D_800E1DB8, &sp10);
  get_rom_asset_allocation_size(0x6B0, &sp10);
  load_rom_asset_payload(D_800E1DA0, &sp10);
  get_rom_asset_allocation_size(0x6B2, &sp10);
  load_rom_asset_payload(D_800E1DA8, &sp10);
  get_rom_asset_allocation_size(0x6C0, &sp10);
  load_rom_asset_payload(D_800E1DAC, &sp10);

  for (i = 0; i != 40; i++) {
    (particle_array + i)->effect_kind = -1;
  }

  D_800FC8B8 = (s32)(f32)get_interpolated_terrain_height_wrapper(D_800FBE64[0],
                                                                 target_z);

  if (arg0->unkA3 == 23) {
    D_800C52B0 = 1;
    get_rom_asset_allocation_size(0x6B6, &sp10);
    load_rom_asset_payload(D_800E1DBC, &sp10);
  } else {
    switch (get_table_entry(active_terrain_index)->id) {
      case 0:
      case 1:
      case 2:
      case 15:
      case 17:
        D_800C52B0 = 2;
        get_rom_asset_allocation_size(0x6B6, &sp10);
        load_rom_asset_payload(D_800E1DBC, &sp10);
        break;
      case 3:
      case 4:
      case 5:
      case 19:
        D_800C52B0 = 4;
        get_rom_asset_allocation_size(0x6B8, &sp10);
        load_rom_asset_payload(D_800E1DBC, &sp10);
        break;
      case 6:
      case 18:
        D_800C52B0 = 3;
        get_rom_asset_allocation_size(0x6B5, &sp10);
        load_rom_asset_payload(D_800E1DBC, &sp10);
        break;
      case 7:
      case 8:
      case 9:
        D_800C52B0 = 6;
        get_rom_asset_allocation_size(0x6B5, &sp10);
        load_rom_asset_payload(D_800E1DBC, &sp10);
        break;
      default:
        D_800C52B0 = 0;
        break;
    }
  }
  func_80216B10();
}

void func_80078D94(void) {
  s32 i;

  for (i = 0; i != 40; i++) {
    (particle_array + i)->effect_kind = -1;
  }
}

void func_80078DC0(s32 arg0, s32 arg1) {
  f32 translation[3];
  f32 rotation[3];
  f32 scale[3];
  f32 matrix[4][4];
  Vec3f spawn_position;
  s32 i;
  Particle* p;

  if (func_8005483C(arg0, arg1, &spawn_position) == 0) {
    D_800FC8B8 = (s32)(f32)get_interpolated_terrain_height_wrapper(
        (s32)(spawn_position.x * 1024.0f), (s32)(spawn_position.z * 1024.0f));

    for (i = 0; i != 40; i++) {
      p = &particle_array[i];
      if (p->effect_kind == -1) {
        p->position.x = spawn_position.x;
        p->position.y = spawn_position.y - 1.5f;
        p->position.z = spawn_position.z;
        p->velocity.x = 0.0f;
        p->velocity.y = 0.0f;
        p->velocity.z = 0.0f;
        p->opacity = 127.0f;
        p->opacity_step = -8.0f;
        p->size = 0.2f;
        p->size_step = 0.8f;
        p->effect_kind = 0x13;
        p->effect_frame = 0;
        p->matrix_index = (s8)frame_counter;
        translation[0] = p->position.x;
        translation[1] = p->position.y;
        translation[2] = p->position.z;
        rotation[0] = 0.0f;
        rotation[1] = 0.0f;
        rotation[2] = 0.0f;
        scale[0] = p->size;
        scale[1] = p->size;
        frame_counter = (frame_counter + 1) % 40;
        scale[2] = p->size;
        mtx_from_rts(matrix, rotation, translation, scale);
        convert_and_pack_floats_to_fixed(matrix, &D_800FD8F0[p->matrix_index]);
        break;
      }
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80078FA8);

void func_80079358(s32 arg0, s32 arg1) {
  s32 sp10[28]; /* reserved stack (unaccessed in this fn) */
  Vec3f spawn_position;
  s32 i;

  if (func_8005483C(arg0, arg1, &spawn_position) == 0) {
    D_800FC8B8 = (s32)(f32)get_interpolated_terrain_height_wrapper(
        (s32)(spawn_position.x * 1024.0f), (s32)(spawn_position.z * 1024.0f));

    for (i = 0; i != 40; i++) {
      Particle* p = &particle_array[i];

      if (p->effect_kind == -1) {
        f32 z;

        p->position.x = spawn_position.x + (guRandom() % 3072 - 1536) * (1.0f / 1024.0f);
        p->position.y = spawn_position.y + (guRandom() % 3072 - 1536) * (1.0f / 1024.0f);
        z = spawn_position.z + (guRandom() % 3072 - 1536) * (1.0f / 1024.0f);
        p->velocity.x = 0.0f;
        p->velocity.z = 0.0f;
        p->unknown_38 = 0;
        p->effect_frame = 0;
        p->velocity.y = 0.002558593638241291f;
        p->opacity = 255.0f;
        p->opacity_step = -4.0f;
        p->size = 6.0f;
        p->size_step = -0.2f;
        p->effect_kind = 8;
        p->position.z = z;
        break;
      }
    }
  }
}

void func_8007955C(s32* arg0) {
  f32 translation[3];
  f32 rotation[3];
  f32 scale[3];
  f32 matrix[4][4];
  s32 i;
  Particle* p;

  for (i = 0; i != 40; i++) {
    p = &particle_array[i];
    if (p->effect_kind == -1) {
      p->position.x = (f32)arg0[0] * (1.0f / 1024.0f);
      p->position.y = (f32)arg0[1] * (1.0f / 1024.0f);
      p->position.z = (f32)arg0[2] * (1.0f / 1024.0f);
      p->velocity.x = 0.0f;
      p->velocity.y = 0.0f;
      p->velocity.z = 0.0f;
      p->opacity = 95.0f;
      p->opacity_step = -2.0f;
      p->size = 1.0f;
      p->size_step = 0.1f;
      p->effect_kind = 0xC;
      p->effect_frame = 0;
      p->matrix_index = (s8)frame_counter;
      translation[0] = p->position.x;
      translation[1] = p->position.y;
      translation[2] = p->position.z;
      rotation[0] = 0.0f;
      rotation[1] = 0.0f;
      rotation[2] = 0.0f;
      scale[0] = p->size;
      scale[1] = p->size;
      frame_counter = (frame_counter + 1) % 40;
      scale[2] = p->size;
      mtx_from_rts(matrix, rotation, translation, scale);
      convert_and_pack_floats_to_fixed(matrix, &D_800FD8F0[p->matrix_index]);
      return;
    }
  }
}

const f64 D_800D1978 = 0.0490873852123405174;
const f64 D_800D1980 = 0.785398163397448279;
extern u8 D_801B9418[];
extern s32 render_buffer_index;

void func_800796F8(s32* arg0) {
  f32 translation[3];
  f32 scale[3];
  f32 rotation[3];
  f32 matrix[4][4];
  s32 i;
  s32 idx;
  s32 step;
  f32 radius;
  f32 angle;
  f32 sin_part;
  f32 vel_y;
  Particle* p;

  D_800FC8B8 = (s32)(f32)get_interpolated_terrain_height_wrapper(arg0[0], arg0[2]);
  for (i = 0; i < 40; i++) {
    p = &particle_array[i];
    if (p->effect_kind == -1) {
      p->position.x = (f32)arg0[0] * 0.0009765625f;
      p->position.y = (f32)arg0[1] * 0.0009765625f;
      p->position.z = (f32)arg0[2] * 0.0009765625f;
      idx = guRandom() % 32;
      step = idx * 20;
      radius = (f32)(step + 0xC8);
      angle = (f32)((f64)(f32)((f64)idx * D_800D1978) - D_800D1980);
      p->velocity.x = (f32)(cosf(angle) * radius * 0.0009765625f);
      sin_part = sinf(angle) * radius;
      vel_y = (f32)(-2.6367188f - ((f32)step * 0.0009765625f));
      p->unknown_24 = 0.0f;
      p->effect_kind = 5;
      p->opacity_step = 0.0f;
      p->size_step = 0.0f;
      p->opacity = 255.0f;
      p->size = 0.5f;
      p->velocity.y = vel_y;
      p->velocity.z = (f32)(sin_part * 0.0009765625f);
      translation[0] = p->position.x;
      translation[1] = p->position.y;
      translation[2] = p->position.z;
      rotation[0] = p->unknown_24;
      rotation[1] = 0.0f;
      rotation[2] = p->unknown_24;
      scale[0] = p->size;
      scale[1] = p->size;
      scale[2] = p->size;
      mtx_from_rts(matrix, rotation, translation, scale);
      convert_and_pack_floats_to_fixed(matrix, &((Mtx*)D_801B9418)[render_buffer_index]);
      return;
    }
  }
}

void func_80079940(s32* arg0) {
  s32 i;

  for (i = 0; i != 40; i++) {
    Particle* p = &particle_array[i];

    if (p->effect_kind == -1) {
      f32 z;

      p->position.x = arg0[0] * (1.0f / 1024.0f);
      p->position.y = arg0[1] * (1.0f / 1024.0f);
      z = arg0[2] * (1.0f / 1024.0f);
      p->velocity.x = 0.0f;
      p->velocity.y = 0.002558593638241291f;
      p->velocity.z = 0.0f;
      p->unknown_38 = 0;
      p->opacity = 255.0f;
      p->opacity_step = -4.0f;
      p->size = 24.0f;
      p->size_step = -0.2f;
      p->effect_kind = 8;
      p->effect_frame = 0;
      p->position.z = z;
      break;
    }
  }
}

const f64 D_800D1988 = -0.150000000000000022;

void func_80079A08(s32 arg0, s32 arg1) {
  s32 sp10[28]; /* reserved stack (unaccessed in this fn) */
  Vec3f spawn_position;
  s32 i;
  s32 one;
  s32 free_slot;

  if (func_8005483C(arg0, arg1, &spawn_position) == 0) {
    f64 grav;

    i = 0;
    free_slot = -1;
    one = 1;
    grav = D_800D1988;
    for (; i != 40; i++) {
      Particle* p = &particle_array[i];

      if (p->effect_kind == free_slot) {
        f32 w;
        f32 z;

        p->position.x = spawn_position.x;
        p->position.y = spawn_position.y;
        z = spawn_position.z;
        w = (f32)((f64)one * grav);
        p->velocity.x = 0.0f;
        p->velocity.z = 0.0f;
        p->unknown_38 = 0;
        p->opacity = 230.0f;
        p->opacity_step = -2.0f;
        p->size = 2.0f;
        p->size_step = 0.02f;
        p->effect_kind = 14;
        p->effect_frame = 0;
        p->position.z = z;
        p->velocity.y = w;
        break;
      }
    }
  }
}

typedef struct {
  u8 pad00[0x18];
  s32 unk18;
  u8 pad1C[0x4];
  s32 unk20;
  u8 pad24[0x7F];
  s8 terrain_index; /* 0xA3 */
} TerrainSpawnSource;

extern s32 effect_spawn_pos[];
extern s32 D_80104AE8[];
extern s32 blend_terrain_color(s32 x, s32 z, s32 terrain_index);

void spawn_terrain_effect(TerrainSpawnSource* src) {
  s32 color;
  s32 n;
  s32 i;
  Particle* p;
  f32 trans[3];
  f32 rot[3];
  f32 scale[3];
  f32 mf[4][4];

  color = blend_terrain_color(src->unk18, src->unk20, src->terrain_index);
  n = 1;
  switch (get_table_entry(src->terrain_index)->id) {
  case 0:
  case 1:
  case 2:
  case 17:
    for (i = 0; i != 40; i++) {
      p = &particle_array[i];
      if (p->effect_kind == -1) {
        p->position.x = (f32)effect_spawn_pos[0] * 0.0009765625f;
        p->position.y = (f32)effect_spawn_pos[1] * 0.0009765625f;
        p->position.z = (f32)effect_spawn_pos[2] * 0.0009765625f;
        p->velocity.x = 0.0f;
        p->velocity.y = (f32)(-(n * 0x600)) * 0.0001953125f;
        p->velocity.z = 0.0f;
        p->unknown_38 = (s16)color;
        p->opacity = 230.0f;
        p->opacity_step = -2.0f;
        p->size = 1.0f;
        p->size_step = 0.04f;
        p->effect_kind = 0;
        p->effect_frame = 0;
        n++;
        if (n == 4) {
          break;
        }
      }
    }
    break;

  case 6:
  case 7:
  case 8:
  case 9:
  case 18:
    for (i = 0; i != 40; i++) {
      p = &particle_array[i];
      if (p->effect_kind == -1) {
        p->position.x = (f32)effect_spawn_pos[0] * 0.0009765625f;
        p->position.y = (f32)effect_spawn_pos[1] * 0.0009765625f;
        p->position.z = (f32)effect_spawn_pos[2] * 0.0009765625f;
        p->velocity.x = 0.0f;
        p->velocity.y = (f32)(-(n * 0x600)) * 0.00068359375f;
        p->velocity.z = 0.0f;
        p->unknown_38 = (s16)color;
        p->opacity = 230.0f;
        p->opacity_step = -2.0f;
        p->size = 2.0f;
        p->size_step = 0.1f;
        p->effect_kind = 0;
        p->effect_frame = 0;
        n++;
        if (n == 4) {
          break;
        }
      }
    }
    break;

  case 10:
    for (i = 0; i != 40; i++) {
      p = &particle_array[i];
      if (p->effect_kind == -1) {
        p->position.x = (f32)effect_spawn_pos[0] * 0.0009765625f;
        p->position.y = (f32)effect_spawn_pos[1] * 0.0009765625f;
        p->position.z = (f32)D_80104AE8[0] * 0.0009765625f;
        p->velocity.x = 0.0f;
        p->velocity.y = 0.0f;
        p->velocity.z = 0.0f;
        p->opacity = 95.0f;
        p->opacity_step = -2.0f;
        p->size = 1.0f;
        p->size_step = 0.1f;
        p->effect_kind = 12;
        p->effect_frame = 0;
        p->matrix_index = frame_counter;
        frame_counter = (frame_counter + 1) % 40;

        trans[0] = p->position.x;
        trans[1] = p->position.y;
        trans[2] = p->position.z;
        rot[0] = 0.0f;
        rot[1] = 0.0f;
        rot[2] = 0.0f;
        scale[0] = p->size;
        scale[1] = p->size;
        scale[2] = p->size;
        mtx_from_rts(mf, rot, trans, scale);
        convert_and_pack_floats_to_fixed(mf, &D_800FD8F0[p->matrix_index]);
        break;
      }
    }
    break;

  case 3:
  case 4:
  case 5:
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:
  case 16:
  case 19:
    break;
  }
}

extern f64 D_800D19E0;

void func_80079EBC(void) {
  s32 i;
  s32 count;
  Particle* p;
  count = 0;
  for (i = 0; i != 40; i++) {
    p = &particle_array[i];
    if (p->effect_kind == -1) {
      f32 size_step;
      f64 offset;
      p->position.x = (f32)D_800FBE64[0] * (1.0f / 1024.0f);
      p->position.y = ((f32)D_800FBE64[1] - 15360.0f) * (1.0f / 1024.0f);
      p->position.z = (f32)D_800FBE64[2] * (1.0f / 1024.0f);
      p->velocity.x = 0;
      p->velocity.y =
          ((f32)count * 0.3f + 0.1f + (f32)((s32)guRandom() % 10) * 0.01f) *
          -1.5f;
      p->velocity.z = 0;
      p->opacity = 255.0f;
      p->opacity_step = -1.0f;
      p->size = (f32)((s32)guRandom() % 30) * 0.1f + 1.0f;
      size_step = (f32)((s32)guRandom() % 10) * 0.001f;
      offset = D_800D19E0;
      /* Retain the shared pool and keep the byte store of zero ahead of the
       * final double-precision addition. */
      count++;
      do {
        p->effect_frame = 0;
      } while (0);
      p->size_step = size_step + offset;
      p->effect_kind = 0x10;
      if (count == 3) {
        break;
      }
    }
  }
}

/* func_8007A10C + func_8007A40C: GCC NESTED functions defined inside
 * func_8007A6C8 (static chain in $v0 = STATIC_CHAIN_REGNUM, mips.h:1310).
 * Both child bodies match byte-exact in isolation, but a nested fn banks ONLY
 * inside its parent's C body -> this is one indivisible 3-fn carve unit (child
 * A10C < child A40C < parent A6C8, gcc emits children first). Particle
 * unk_0C/unk_14 are f32 in the ROM (swc1); they stay s32 here so the sibling
 * ordinary fns' `= 0` stores stay `sw $zero`. */
INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A10C);

/* Parent frame the nested child reads through the static chain. */
typedef struct {
  u8 pad00[0x70];
  s32 spawned;    /* 0x70 */
  s32 slot;       /* 0x74 */
  s32 **pos_src;  /* 0x78 */
  f32 vel_x;      /* 0x7C */
  f32 vel_y;      /* 0x80 */
  f32 vel_z;      /* 0x84 */
  s32 bend;       /* 0x88 */
  f32 size_scale; /* 0x8C */
} NestFrame;

s32 func_8007A40C(s32 arg0, s32 arg1, s32 arg2) {
  s32 r;
  volatile s32 unused = r;
  NestFrame *frame = (NestFrame *) r;
  Particle *p;
  s32 speed;
  f32 ft;
  f32 size_step;

  frame->spawned = 0;
  frame->slot = 0;
  do {
    p = &particle_array[frame->slot];
    guRandom();
    if (frame->spawned == arg1) {
      break;
    }
    frame->spawned++;
    arg0++;
    if (p->effect_kind == -1) {
      p->position.x = (f32)(*frame->pos_src)[0] * 0.0009765625f;
      p->position.y = (f32)(*frame->pos_src)[1] * 0.0009765625f;
      p->position.z = (f32)(*frame->pos_src)[2] * 0.0009765625f;
      if (frame->spawned * 3 != 0) {
        speed = ((guRandom() % (frame->spawned * 3)) + 5) * 0x600;
      } else {
        speed = 0x1E00;
      }
      ft = (f32)speed;
      p->velocity.x = frame->vel_x * ft * 0.000029296874f;
      p->velocity.y = frame->vel_y * ft * 0.00014648438f;
      p->velocity.z = frame->vel_z * ft * 0.000029296874f;
      p->unknown_38 = (s16)frame->bend;
      if (arg2 == 0) {
        p->opacity = (f32)((guRandom() % 32) + 0xBF);
      } else {
        p->opacity = (f32)((guRandom() % 32) + 0x9F);
      }
      p->opacity_step = -2.0f;
      p->size = (f32)(guRandom() % 32) * 0.2f;
      size_step = (frame->size_scale * 0.01f) + 0.4f;
      p->effect_kind = arg2;
      p->effect_frame = 0;
      p->size_step = size_step;
    }
    frame->slot++;
  } while (frame->slot < 40);
  return arg0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A6C8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007B054);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007B994);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007C5D8);

extern s32 D_800FC8B8;

void func_8007CF10(Gfx** pgfx, Particle* particle) {
  Gfx* gfx = *pgfx;
  ScreenProjection screen;
  s32 z;

  z = project_point_to_screen(particle->position.x,
                              (f32)D_800FC8B8 * (1.0f / 1024.0f),
                              particle->position.z, &screen);
  if ((f32)z < -3.0f && (f32)z > -60.0f) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0x3F);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (screen.x - 1) << 2, (screen.y - 1) << 2,
                            (screen.x + 1) << 2, (screen.y + 1) << 2,
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  }
  *pgfx = gfx;
}

extern u8 D_1B9418[];
extern Vtx D_800C52B8[];
extern s32 render_buffer_index;

void func_8007D19C(Gfx** pgfx, s32 arg1) {
  Gfx* gfx;

  gfx = *pgfx;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
  gDPSetPrimColor(gfx++, 0, 0, 0xEF, 0xEF, 0xEF, 0xFF);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPLoadTextureBlock_4b(gfx++, ((u32)D_800E1DA8 + 8) & ~7, G_IM_FMT_I, 16, 16, 0,
                         G_TX_CLAMP, G_TX_CLAMP, 6, 6, 0, 0);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, (u32)(D_1B9418 + (render_buffer_index << 6)), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, (u32)(arg1 + 0x800000C0), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gSPVertex(gfx++, D_800C52B8, 4, 0);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", draw_character_shadow);

extern Mtx D_106108[];
extern Vtx D_800C52F8[];

void func_8007DB08(Gfx** pgfx) {
  Gfx* gfx = *pgfx;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_800E1DAC + 8) & ~7);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPLoadTextureBlock_4b(gfx++, ((u32)D_800E1DAC + 0x828) & ~7, G_IM_FMT_CI, 32,
                         32, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD,
                         G_TX_NOLOD);
  gSPMatrix(gfx++, D_106108, G_MTX_PUSH);
  gSPVertex(gfx++, D_800C52F8, 4, 0);
  gDPPipeSync(gfx++);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);

  *pgfx = gfx;
}

extern Vtx D_800C5338[];

void func_8007DE9C(Gfx** pgfx, Particle* p) {
  Gfx* gfx = *pgfx;

  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, 0xE6, 0xE6, 0xE6, (u32)p->opacity);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_800FD8F0[p->matrix_index]),
            G_MTX_PUSH);
  gSPVertex(gfx++, D_800C5338, 4, 0);
  gDPPipeSync(gfx++);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  *pgfx = gfx;
}

extern Vtx D_800C5378[];

void func_8007DFD0(Gfx** pgfx, Particle* p) {
  Gfx* gfx = *pgfx;

  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, 0xE6, 0xE6, 0xE6, (u32)p->opacity);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_800FD8F0[p->matrix_index]),
            G_MTX_PUSH);
  gSPVertex(gfx++, D_800C5378, 4, 0);
  gDPPipeSync(gfx++);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  *pgfx = gfx;
}

typedef struct {
  u8 pad_00[0x27];
  s8 unk_27;
} RumbleController;

extern s8 rumble_disable_flag;
extern u32 controller_handle_array[];
extern s32 nuContRmbCheck(u32 contNo);
extern void nuContRmbStart(u32 contNo, u16 freq, u16 frame);

void shot_start_rumble_trigger(RumbleController* actor) {
  s32 result;

  if (actor->unk_27 < 4) {
    if (rumble_disable_flag == 0) {
      result = nuContRmbCheck(controller_handle_array[actor->unk_27]);
      if (result != 1) {
        if (result < 2) {
          if (result == 0) {
            nuContRmbStart(controller_handle_array[actor->unk_27], 0x100, 0xA);
          }
        }
      }
    }
  }
}

void rumble_check_and_trigger(RumbleController* actor) {
  s32 result;

  if (actor->unk_27 < 4) {
    if (rumble_disable_flag == 0) {
      result = nuContRmbCheck(controller_handle_array[actor->unk_27]);
      if (result != 1) {
        if (result < 2) {
          if (result == 0) {
            nuContRmbStart(controller_handle_array[actor->unk_27], 0x100, 0xA);
          }
        }
      }
    }
  }
}

extern s32 D_800C5448;
extern void* D_800FF4CC;
extern void* D_801EFFA8;
extern void* D_801B8BB8;

extern void func_8007E7B8(void);

void func_8007E234(s32 mode) {
  RomAssetLoad asset_load;

  if (mode == 2) {
    D_800FF4CC = heap3_alloc(get_rom_asset_allocation_size(0x6BC, &asset_load));
    load_rom_asset_payload(D_800FF4CC, &asset_load);
    D_801EFFA8 = heap3_alloc(0x900);
    D_801B8BB8 = heap3_alloc(0x380);
    func_8007E7B8();
  }
  D_800C5448 = mode;
}

extern s32 D_800C5448;
extern void* D_800FF4CC;
extern void* D_801EFFA8;
extern void* D_801B8BB8;
extern void heap3_free(void** payload_ptr);
extern void update_object_group_by_id(s32 id);

void func_8007E2B0(void) {
  if (D_800C5448 == 2) {
    heap3_free(&D_800FF4CC);
    heap3_free(&D_801EFFA8);
    heap3_free(&D_801B8BB8);
  }
  update_object_group_by_id(9);
  D_800C5448 = -1;
}

void project_delta_to_radius_150(void) {
  s32 dx = (s32)(D_801B54F8 - view_camera_eye_x);
  s32 dy = (s32)(D_801B5500 - view_camera_eye_z);
  f32 scale = sqrtf((f32)(dx * dx + dy * dy));

  D_800FF418 = dx;
  D_800FF41C = dy;
  if ((f32)(s32)scale != 0.0f) {
    scale = 150.0f / scale;
  } else {
    osSyncPrintf(D_800D1A78);
    scale = 0.0f;
  }
  {
    s32* p = &D_800FF418;
    *p = (s32)((f32)*p * scale + view_camera_eye_x);
  }
  D_800FF41C = (s32)((f32)D_800FF41C * scale + view_camera_eye_z);
}

typedef struct {
  Vec3f position;
  Vec3f target_position;
  s32 unknown_18;
  f32 unknown_1C;
  u8 unknown_20[0xA];
  s16 unknown_2A;
  s8 unknown_2C;
  u8 unknown_2D;
  u8 unknown_2E[2];
} WindParticle; /* 0x30 */

extern s32 rand(void);
extern f32 D_801B54F0;
extern f32 D_801049FC;
extern f32 D_800FE380;
/* Member-sized views retain the separate retail address roots of this step. */
extern f32 D_80104F44[];
extern f32 D_80104F48[];
extern f32 D_80104F4C[];

void func_8007E438(WindParticle* p) {
  f32 t;

  p->position.x =
      (f32)((rand() % 200 - 100) * 1536) * (1.0f / 1024.0f) + (f32)D_800FF418;
  p->position.y = (f32)(-0x1A400 - (rand() % 200) * 1536) * (1.0f / 1024.0f);
  p->position.z =
      (f32)((rand() % 200 - 100) * 1536) * (1.0f / 1024.0f) + (f32)D_800FF41C;
  t = p->position.y / D_80104F48[0];
  p->position.y += D_801B54F0 + 15.0f;
  p->position.x += D_80104F44[0] * t;
  p->position.z += D_80104F4C[0] * t;
  p->unknown_18 = 0;
  p->target_position.z = p->position.z + D_800FE380 * 90.0f;
  p->target_position.y = p->position.y + 90.0f;
  p->target_position.x = p->position.x + D_801049FC * 90.0f;
  p->unknown_2C = -1;
  p->unknown_2D = 0;
  p->unknown_2A = 0;
  p->unknown_1C = 1.0f;
}

typedef struct {
  Vec3f position; /* y is sampled terrain height */
  f32 unknown_0C;
  u8 unknown_10[6];
  s8 unknown_16;
  u8 unknown_17;
  s8 unknown_18;
} TerrainScatterPoint;

void randomize_terrain_scatter_point(TerrainScatterPoint* p) {
  p->position.x =
      (f32)((rand() % 500 - 250) * 1536) * (1.0f / 1024.0f) + (f32)D_800FF418;
  p->position.z =
      (f32)((rand() % 500 - 250) * 1536) * (1.0f / 1024.0f) + (f32)D_800FF41C;
  p->position.y = (f32)get_interpolated_terrain_height_wrapper(
                          (s32)(p->position.x * 1024.0f),
                          (s32)(p->position.z * 1024.0f)) *
                      (1.0f / 1024.0f);
  p->unknown_18 = 0;
}

extern s32 D_80104AC0;
extern f32 cosf(f32 angle);
extern f32 sinf(f32 angle);

void func_8007E7B8(void) {
  s32 wind = hole_environment.wind_magnitude;
  f32 angle;
  f32 strength;
  s32 i;
  D_80104AC0 = 0;
  angle = (u16)hole_environment.wind_angle_bam * 0.000095873802f;
  /* Keep signed truncation in the input scratch before converting to float. */
  if (wind < 0) {
    wind += 0xFFFF;
  }
  strength = (f32)(wind >> 16);
  strength *= 0.1f;
  D_801049FC = cosf(angle) * strength;
  D_800FE380 = sinf(angle) * strength;
  D_80104F48[0] = 60.0f;
  D_80104F44[0] = D_801049FC * 60.0f;
  D_80104F4C[0] = D_800FE380 * 60.0f;
  project_delta_to_radius_150();
  for (i = 0; i != 48; i++) {
    func_8007E438((WindParticle*)D_801EFFA8 + i);
    ((WindParticle*)D_801EFFA8)[i].unknown_2A = 0;
  }
  i = 0;
  strength = 1.0f;
  for (; i != 32; i++) {
    TerrainScatterPoint* point;
    randomize_terrain_scatter_point((TerrainScatterPoint*)D_801B8BB8 + i);
    point = (TerrainScatterPoint*)(i * sizeof(TerrainScatterPoint) +
                                   (u32)D_801B8BB8);
    point->unknown_16 = i % 9 - 1;
    point->unknown_0C = strength;
    ((TerrainScatterPoint*)D_801B8BB8)[i].unknown_17 = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E980);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007EF0C);

void func_8007FE44(s32 value) {
  RomAssetLoad descriptor;

  D_800C5458 = value;
  D_800C545C = 1;
  D_800C5460 = 0;
  get_rom_asset_allocation_size(0x663, &descriptor);
  load_rom_asset_payload(D_800E1DB4, &descriptor);
  get_rom_asset_allocation_size(0x689, &descriptor);
  load_rom_asset_payload(D_800E1DB8, &descriptor);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007FEAC);
