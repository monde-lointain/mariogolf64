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
extern void func_800674B8(void);
extern s32 func_80067730(void);
extern s8 D_801B60B8;
extern u8 D_801B7248[];

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

void update_rumble_intensity_table(void) {
  s32 n;
  u8* row;
  u8* p;
  s32 i;

  func_800674B8();
  n = func_80067730();
  if (n >= 3) {
    n = 3;
  }
  i = 0;
  if (D_801B60B8 != 2) {
    row = D_801B7248;
  loop_outer:
    /* The extra loop scope preserves the inner bounds' allocation priority. */
    do {
      p = row;
      do {
        *p = ((s32)p < n + (s32)row);
        p++;
      } while ((s32)p < (s32)(row + 3));
    } while (0);
    i++;
    row += 0xB8;
    if (i < 4) {
      goto loop_outer;
    }
  }
}

extern s16 D_800C3130[][34];
extern void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan);

void func_80067A60(s32 bank, s32 channel, s32 entry, s32 volume, s32 pan) {
  s32 reverb = -(bank != 3) & 0x14;
  /* Separate call sites preserve the retail argument copies. The base/index
   * split keeps each table address materialized before its row offset. */
  if (volume == -1) {
    s16 (*rows)[34] = D_800C3130;
    s16* row = rows[bank];
    func_80050DA0(row[entry], channel + 4, row[entry + 1], reverb, pan);
  } else {
    s16 (*rows)[34] = D_800C3130;
    s16* row = rows[bank];
    func_80050DA0(row[entry], channel + 4, volume, reverb, pan);
  }
}
