#include "common.h"

#include "ball_simulation.h"
#include "character_slot.h"
#include "libc.h"
#include "reaction.h"
#include "surface.h"

/* LoadCharacter kinds from the kind count on load one of g_OtherCharacterModels
 */
#define OTHER_CHARACTER_MODEL_KIND CHARACTER_MODEL_KIND_COUNT

/* The opening's own copy of g_SurfaceInfo */
extern SurfaceInfo D_8020B564_ovl_05[SURFACE_COUNT];

/* Loads one of the other characters into the slot with its birdie animation, or
 * frees a busy slot first */
s32 func_80206680_ovl_05(u32 slotIndex, s32 character) {
  if (IsCharacterSlotBusy(slotIndex)) {
    FreeCharacterSlot(slotIndex);
    return FALSE;
  }
  LoadCharacter(slotIndex, character, OTHER_CHARACTER_MODEL_KIND);
  SetCharacterAnimation(slotIndex, REACTION_ANIMATION_BIRDIE);
  SetAnimationSpeed(slotIndex, 1.0f);
  return TRUE;
}

/* Like GetSurfaceInfo */
SurfaceInfo* func_802066F0_ovl_05(s32 surface) {
  if ((u32)surface >= SURFACE_COUNT) {
    surface = SURFACE_OUT_OF_BOUNDS;
  }
  return &D_8020B564_ovl_05[surface];
}

/* An unused copy of ResetBallSimulations, which resets g_BallSimulations as
 * that does */
void func_80206718_ovl_05(void) {
  s32 index;

  for (index = 0; index < BALL_SIMULATION_COUNT; index++) {
    g_BallSimulations[index].position[AXIS_X] = 0x800;
    g_BallSimulations[index].position[AXIS_Y] = -1000;
    rand(); /* The original has it */
    g_BallSimulations[index].position[AXIS_Z] = 0x1000;
    g_BallSimulations[index].velocity[AXIS_X] = rand() & 0x3FF;
    g_BallSimulations[index].velocity[AXIS_Y] = -100;
    g_BallSimulations[index].velocity[AXIS_Z] = rand() & 0x3FF;
    g_BallSimulations[index].unk30 = 0;
    g_BallSimulations[index].isRunning = BALL_STATE_FLYING;
  }
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206800_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_802068B4_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_802068EC_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206910_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206968_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206A50_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206AA4_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A474_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A488_ovl_05);
