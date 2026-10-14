/*
 * Bundled libmus custom-effect synthesizer. Preset tables remain in main data;
 * the initialization, allocation, and effect-update routines still use
 * assembly.
 */
#include <ultra64.h>

#include "include_asm.h"

extern s32 D_800C77E8[];
extern s32* D_800C7A8C[];

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", CustomInit);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009CF98);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D004);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D274);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D2C4);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D40C);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D6EC);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D774);

/* Select the default custom-effect parameter block. */
void func_8009D7E8(ALSynConfig* config) {
  config->fxType = AL_FX_CUSTOM;
  config->params = D_800C77E8;
}

/* Select a preset; the caller supplies a valid table index. */
void func_8009D800(ALSynConfig* config, s32 index) {
  config->fxType = AL_FX_CUSTOM;
  config->params = D_800C7A8C[index];
}

/* Size the reusable effect's delay-descriptor allocation for every preset. */
s32 func_8009D820(void) {
  s32 index;
  s32 max_sections = 0;

  for (index = 0; D_800C7A8C[index] != NULL; index++) {
    if (D_800C7A8C[index][0] > max_sections) {
      max_sections = D_800C7A8C[index][0];
    }
  }
  return max_sections;
}

/* Size the reusable effect's sample buffer for every preset. */
s32 func_8009D864(void) {
  s32 index;
  s32 max_length = 0;

  for (index = 0; D_800C7A8C[index] != NULL; index++) {
    if (D_800C7A8C[index][1] > max_length) {
      max_length = D_800C7A8C[index][1];
    }
  }
  return max_length;
}

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", ChangeCustomEffect);
