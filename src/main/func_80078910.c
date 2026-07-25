#include "common.h"

typedef struct {
  f32 unk_00;
  f32 unk_04;
  f32 unk_08;
  s32 unk_0C;
  f32 unk_10;
  s32 unk_14;
  u8 pad_18[0x10];
  f32 unk_28;
  f32 unk_2C;
  f32 unk_30;
  f32 unk_34;
  s16 unk_38;
  s8 unk_3A;
  s8 unk_3B;
  s8 unk_3C;
  u8 pad_3D[0x3];
} Particle; /* size 0x40 */

extern Particle particle_array[40];

extern u32 func_8005062C(u16 index, void* out);
extern void* heap3_alloc(u32 need);
extern void heap3_free(void** payload_ptr);
extern void func_800506D4(void* arg0, void* arg1);
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
extern void* D_800E1DCC;
extern f32 per_view_camera_state;
extern f32 D_801B54F4;
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
  u8 spvar[0x20];

  func_80078910();
  if (D_800C53B8 == 0) {
    D_800C53B8 = 1;
    D_800E1DBC = heap3_alloc(func_8005062C(0x6B5, spvar));
    func_800506D4(D_800E1DBC, spvar);
    D_800E1DB4 = heap3_alloc(func_8005062C(0x6B5, spvar));
    func_800506D4(D_800E1DB4, spvar);
    D_800E1DB8 = heap3_alloc(func_8005062C(0x6B8, spvar));
    func_800506D4(D_800E1DB8, spvar);
    D_800E1DA0 = heap3_alloc(func_8005062C(0x6B0, spvar));
    func_800506D4(D_800E1DA0, spvar);
    D_800E1DA4 = heap3_alloc(func_8005062C(0x8EC, spvar));
    func_800506D4(D_800E1DA4, spvar);
    D_800E1DA8 = heap3_alloc(func_8005062C(0x6B2, spvar));
    func_800506D4(D_800E1DA8, spvar);
    D_800E1DAC = heap3_alloc(func_8005062C(0x6C0, spvar));
    func_800506D4(D_800E1DAC, spvar);
    D_800E1DB0 = heap3_alloc(func_8005062C(0x6C1, spvar));
    func_800506D4(D_800E1DB0, spvar);
    D_800E1DC0 = heap3_alloc(func_8005062C(0x71D, spvar));
    func_800506D4(D_800E1DC0, spvar);
    D_800E1DC4 = heap3_alloc(func_8005062C(0x6C2, spvar));
    func_800506D4(D_800E1DC4, spvar);
    D_800E1DC8 = heap3_alloc(func_8005062C(0x69E, spvar));
    func_800506D4(D_800E1DC8, spvar);
    D_800E1DCC = heap3_alloc(func_8005062C(0x93B, spvar));
    func_800506D4(D_800E1DCC, spvar);
  }
}

typedef struct {
  /* 0x00 */ u32 id;
} TerrainAttrEntry;

extern s32 D_800FBE64;
extern u32 target_z;
extern s32 D_800FBE70;
extern s32 D_800C52B0;
extern s32 D_800FC8B8;
extern TerrainAttrEntry* get_table_entry(u32 idx);
extern s32 func_8005483C(s32 arg0, s32 arg1, f32* out);
extern void func_80216B10(void);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);

void reload_scene_assets(s8* arg0) {
  u8 sp10[0x20];
  s32 i;

  func_8005062C(0x6B5, sp10);
  func_800506D4(D_800E1DB4, sp10);
  func_8005062C(0x6B8, sp10);
  func_800506D4(D_800E1DB8, sp10);
  func_8005062C(0x6B0, sp10);
  func_800506D4(D_800E1DA0, sp10);
  func_8005062C(0x6B2, sp10);
  func_800506D4(D_800E1DA8, sp10);
  func_8005062C(0x6C0, sp10);
  func_800506D4(D_800E1DAC, sp10);

  i = 0;
  do {
    (particle_array + i)->unk_3A = -1;
  } while (++i != 40);

  D_800FC8B8 =
      (s32)(f32)get_interpolated_terrain_height_wrapper(D_800FBE64, target_z);

  if (arg0[0xA3] == 23) {
    D_800C52B0 = 1;
    func_8005062C(0x6B6, sp10);
    func_800506D4(D_800E1DBC, sp10);
  } else {
    switch (get_table_entry(D_800FBE70)->id) {
      case 0:
      case 1:
      case 2:
      case 15:
      case 17:
        D_800C52B0 = 2;
        func_8005062C(0x6B6, sp10);
        func_800506D4(D_800E1DBC, sp10);
        break;
      case 3:
      case 4:
      case 5:
      case 19:
        D_800C52B0 = 4;
        func_8005062C(0x6B8, sp10);
        func_800506D4(D_800E1DBC, sp10);
        break;
      case 6:
      case 18:
        D_800C52B0 = 3;
        func_8005062C(0x6B5, sp10);
        func_800506D4(D_800E1DBC, sp10);
        break;
      case 7:
      case 8:
      case 9:
        D_800C52B0 = 6;
        func_8005062C(0x6B5, sp10);
        func_800506D4(D_800E1DBC, sp10);
        break;
      default:
        D_800C52B0 = 0;
        break;
    }
  }
  func_80216B10();
}

void func_80078D94(void) {
  s32 i = 0;

  do {
    (particle_array + i)->unk_3A = -1;
  } while (++i != 40);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80078DC0);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80078FA8);

void func_80079358(s32 arg0, s32 arg1) {
  s32 sp10[28]; /* reserved stack (unaccessed in this fn) */
  f32 sp80[4];
  s32 i;

  if (func_8005483C(arg0, arg1, sp80) == 0) {
    D_800FC8B8 = (s32)(f32)get_interpolated_terrain_height_wrapper(
        (s32)(sp80[0] * 1024.0f), (s32)(sp80[2] * 1024.0f));

    i = 0;
    do {
      Particle* p = &particle_array[i];

      if (p->unk_3A == -1) {
        f32 z;

        p->unk_00 = sp80[0] + (guRandom() % 3072 - 1536) * (1.0f / 1024.0f);
        p->unk_04 = sp80[1] + (guRandom() % 3072 - 1536) * (1.0f / 1024.0f);
        z = sp80[2] + (guRandom() % 3072 - 1536) * (1.0f / 1024.0f);
        p->unk_0C = 0;
        p->unk_14 = 0;
        p->unk_38 = 0;
        p->unk_3B = 0;
        p->unk_10 = 0.002558593638241291f;
        p->unk_28 = 255.0f;
        p->unk_2C = -4.0f;
        p->unk_30 = 6.0f;
        p->unk_34 = -0.2f;
        p->unk_3A = 8;
        p->unk_08 = z;
        break;
      }
    } while (++i != 40);
  }
}

/* func_8007955C: particle-spawn + RTS-matrix builder. Fully RE'd near-match
 * carried on a gcc-2.7.2 sched.c schedule-order coin (~85%, < 0.97). See
 * docs/wip/func_8007955C.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007955C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_800796F8);

void func_80079940(s32* arg0) {
  s32 i = 0;

  do {
    Particle* p = &particle_array[i];

    if (p->unk_3A == -1) {
      f32 z;

      p->unk_00 = arg0[0] * (1.0f / 1024.0f);
      p->unk_04 = arg0[1] * (1.0f / 1024.0f);
      z = arg0[2] * (1.0f / 1024.0f);
      p->unk_0C = 0;
      p->unk_10 = 0.002558593638241291f;
      p->unk_14 = 0;
      p->unk_38 = 0;
      p->unk_28 = 255.0f;
      p->unk_2C = -4.0f;
      p->unk_30 = 24.0f;
      p->unk_34 = -0.2f;
      p->unk_3A = 8;
      p->unk_3B = 0;
      p->unk_08 = z;
      break;
    }
  } while (++i != 40);
}

extern f64 D_800D1988;

void func_80079A08(s32 arg0, s32 arg1) {
  s32 sp10[28]; /* reserved stack (unaccessed in this fn) */
  f32 sp80[4];
  s32 i;
  s32 one;
  s32 free_slot;

  if (func_8005483C(arg0, arg1, sp80) == 0) {
    f64 grav;

    i = 0;
    free_slot = -1;
    one = 1;
    grav = D_800D1988;
    do {
      Particle* p = &particle_array[i];

      if (p->unk_3A == free_slot) {
        f32 w;
        f32 z;

        p->unk_00 = sp80[0];
        p->unk_04 = sp80[1];
        z = sp80[2];
        w = (f32)((f64)one * grav);
        p->unk_0C = 0;
        p->unk_14 = 0;
        p->unk_38 = 0;
        p->unk_28 = 230.0f;
        p->unk_2C = -2.0f;
        p->unk_30 = 2.0f;
        p->unk_34 = 0.02f;
        p->unk_3A = 14;
        p->unk_3B = 0;
        p->unk_08 = z;
        p->unk_10 = w;
        break;
      }
    } while (++i != 40);
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", spawn_terrain_effect);

/* func_80079EBC: particle-spawn (3 particles at D_800FBE64 with per-axis
 * jitter). Fully RE'd BYTE-EXACT body (S279 gccB, 148/148 in isolation), but
 * BLOCKED in this partial bank: it needs the TU's shared FP literal pool (0.04
 * at D_800D19E0, alongside 0.2/0.049 + jtbl_800D1990) which is still owned by
 * still-asm siblings in asm/53D10.s. Emitting the 0.04 as a C literal
 * duplicates the pool (+0x10 flowing-rodata shift); referencing it extern
 * avoids the shift but perturbs a source-invariant count++ sched.c coin
 * (CONST_DOUBLE vs MEM cost: with the extern MEM the addiu $s2 hoists one slot
 * ahead of the ldc1). Banks once the pool-owning siblings are also C. See
 * docs/wip/func_80079EBC.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80079EBC);

/* func_8007A10C + func_8007A40C: GCC NESTED functions defined inside
 * func_8007A6C8 (static chain in $v0 = STATIC_CHAIN_REGNUM, mips.h:1310).
 * Both child bodies are fully RE'd BYTE-EXACT in isolation (S279 fan-out;
 * func_8007A40C 175/175, dual-confirmed nested by gcc + binutils subagents),
 * but a nested fn banks ONLY inside its parent's C body -> this is one
 * indivisible 3-fn carve unit (child A10C < child A40C < parent A6C8, gcc
 * emits children first). Struct correction pending at bundle-bank: Particle
 * unk_0C/unk_14 are f32 (ROM swc1), currently s32 (kept s32 so the sibling
 * ordinary fns' `= 0` stores stay `sw $zero`). See
 * docs/wip/func_8007A40C.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A10C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A40C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A6C8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007B054);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007B994);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007C5D8);

typedef struct {
  f32 unk_00;
  u8 pad_04[0x4];
  s32 unk_08;
} ScreenMarker;

extern s32 D_800FC8B8;
extern s32 project_point_to_screen(f32, f32, s32, s32*);

void func_8007CF10(Gfx** pgfx, ScreenMarker* m) {
  Gfx* gfx = *pgfx;
  s32 screen[6];
  s32 z;

  z = project_point_to_screen(m->unk_00, (f32)D_800FC8B8 * (1.0f / 1024.0f),
                              m->unk_08, screen);
  if ((f32)z < -3.0f && (f32)z > -60.0f) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0x3F);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (screen[0] - 1) << 2, (screen[1] - 1) << 2,
                            (screen[0] + 1) << 2, (screen[1] + 1) << 2,
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  }
  *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007D19C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", draw_character_shadow);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007DB08);

extern Mtx D_FD8F0[];
extern Vtx D_800C5338[];

void func_8007DE9C(Gfx** pgfx, Particle* p) {
  Gfx* gfx = *pgfx;

  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, 0xE6, 0xE6, 0xE6, (u32)p->unk_28);
  gSPMatrix(gfx++, &D_FD8F0[p->unk_3C], G_MTX_PUSH);
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
  gDPSetPrimColor(gfx++, 0, 0, 0xE6, 0xE6, 0xE6, (u32)p->unk_28);
  gSPMatrix(gfx++, &D_FD8F0[p->unk_3C], G_MTX_PUSH);
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
extern u32 func_8005062C(u16 index, void* out);
extern void* heap3_alloc(u32 need);
extern void func_800506D4(void* arg0, void* arg1);
extern void func_8007E7B8(void);

void func_8007E234(s32 arg) {
  u8 spvar[0x20];

  if (arg == 2) {
    D_800FF4CC = heap3_alloc(func_8005062C(0x6BC, spvar));
    func_800506D4(D_800FF4CC, spvar);
    D_801EFFA8 = heap3_alloc(0x900);
    D_801B8BB8 = heap3_alloc(0x380);
    func_8007E7B8();
  }
  D_800C5448 = arg;
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
  s32 dx = (s32)(D_801B54F8 - per_view_camera_state);
  s32 dy = (s32)(D_801B5500 - D_801B54F4);
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
    *p = (s32)((f32)*p * scale + per_view_camera_state);
  }
  D_800FF41C = (s32)((f32)D_800FF41C * scale + D_801B54F4);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E438);

typedef struct {
  f32 unk_00; /* world x */
  f32 unk_04; /* world y (terrain height) */
  f32 unk_08; /* world z */
  u8 pad_0C[0xC];
  s8 unk_18;
} TerrainScatterPoint;

extern s32 rand(void);

void randomize_terrain_scatter_point(TerrainScatterPoint* p) {
  p->unk_00 =
      (f32)((rand() % 500 - 250) * 1536) * (1.0f / 1024.0f) + (f32)D_800FF418;
  p->unk_08 =
      (f32)((rand() % 500 - 250) * 1536) * (1.0f / 1024.0f) + (f32)D_800FF41C;
  p->unk_04 = (f32)get_interpolated_terrain_height_wrapper(
                  (s32)(p->unk_00 * 1024.0f), (s32)(p->unk_08 * 1024.0f)) *
              (1.0f / 1024.0f);
  p->unk_18 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E7B8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E980);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007EF0C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007FE44);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007FEAC);
