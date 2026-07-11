#include "common.h"

typedef struct {
  u8 pad_00[0x3A];
  s8 unk_3A;
  u8 pad_3B[0x5];
} Particle; /* size 0x40 */

extern Particle particle_array[40];

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80078910);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_800789C8);

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

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80079940);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80079A08);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", spawn_terrain_effect);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_80079EBC);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A10C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A40C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007A6C8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007B054);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007B994);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007C5D8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007CF10);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007D19C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", draw_character_shadow);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007DB08);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007DE9C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007DFD0);

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
