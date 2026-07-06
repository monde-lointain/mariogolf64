#include "common.h"

extern u8 D_800C3100;
extern s32 collision_triangles;
extern s16 D_8018D228;
extern s16 D_8018D22A;
extern s16 D_8018D22C;
extern s16 D_8018D238;
extern s16 D_8018D23A;
extern s16 D_8018D23C;
extern s16 D_8018D248;
extern s16 D_8018D24A;
extern s16 D_8018D24C;

void update_rumble_intensity_table(void);

void func_800660A0(void) { D_800C3100 = 1; }

INCLUDE_ASM("asm/nonmatchings/main/func_800660A0", draw_course_decal_triangles);

INCLUDE_ASM("asm/nonmatchings/main/func_800660A0", emit_aim_target_ring);

INCLUDE_ASM("asm/nonmatchings/main/func_800660A0", func_800674B8);

void func_800676B0(void) {
  collision_triangles = 1;
  D_8018D228 = 0x800;
  D_8018D22A = -0x100;
  D_8018D22C = 0x1000;
  D_8018D238 = 0x800;
  D_8018D23A = -0x100;
  D_8018D23C = 0xF00;
  D_8018D248 = 0x700;
  D_8018D24A = -0x100;
  D_8018D24C = 0xF00;
  update_rumble_intensity_table();
}

INCLUDE_ASM("asm/nonmatchings/main/func_800660A0", func_80067730);

INCLUDE_ASM("asm/nonmatchings/main/func_800660A0",
            update_rumble_intensity_table);

INCLUDE_ASM("asm/nonmatchings/main/func_800660A0", func_80067A60);
