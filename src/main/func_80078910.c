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

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", shot_start_rumble_trigger);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", rumble_check_and_trigger);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E234);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E2B0);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E30C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E438);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E664);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E7B8);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007E980);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007EF0C);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007FE44);

INCLUDE_ASM("asm/nonmatchings/main/func_80078910", func_8007FEAC);
