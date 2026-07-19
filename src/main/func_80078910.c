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

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80078BDC);

void func_80078D94(void) {
  s32 i = 0;

  do {
    (particle_array + i)->unk_3A = -1;
  } while (++i != 40);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80078DC0);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80078FA8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80079358);

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

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80079A08);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", spawn_terrain_effect);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80079EBC);

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

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E30C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E438);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E664);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E7B8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E980);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007EF0C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007FE44);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007FEAC);
