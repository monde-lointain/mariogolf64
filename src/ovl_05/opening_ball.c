#include "common.h"

#include "ball_simulation.h"
#include "character_slot.h"
#include "club.h"
#include "debug_text.h"
#include "effects.h"
#include "functions.h"
#include "game_flags.h"
#include "libc.h"
#include "math_util.h"
#include "reaction.h"
#include "surface.h"
#include "variables.h"

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

/*
 * The opening's copies of the ball simulation's routines keep their names and
 * are static, so they don't clash with the originals. GCC 2.7 only warns when
 * a static definition follows the headers' prototypes.
 */

/* The opening's copy of GetSurfaceInfo, which reads its own surface table */
static SurfaceInfo* GetSurfaceInfo(s32 surface) {
  if ((u32)surface >= SURFACE_COUNT) {
    surface = SURFACE_OUT_OF_BOUNDS;
  }
  return &D_8020B564_ovl_05[surface];
}

/* An unused copy of ResetBallSimulations, which resets g_BallSimulations as
 * that does */
static void ResetBallSimulations(void) {
  s32 index;

  for (index = 0; index < BALL_SIMULATION_COUNT; index++) {
    g_BallSimulations[index].position[AXIS_X] = RESET_BALL_X;
    g_BallSimulations[index].position[AXIS_Y] = RESET_BALL_Y;
    rand(); /* The original has it */
    g_BallSimulations[index].position[AXIS_Z] = RESET_BALL_Z;
    g_BallSimulations[index].velocity[AXIS_X] = rand() & RESET_BALL_SPEED_MASK;
    g_BallSimulations[index].velocity[AXIS_Y] = RESET_BALL_VELOCITY_Y;
    g_BallSimulations[index].velocity[AXIS_Z] = rand() & RESET_BALL_SPEED_MASK;
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

/* The opening's copies of D_800B7C10 and of the launch count, and its clubs */
extern Unk6 D_8020B86C_ovl_05[MAX_LIE_RATING + 1][UNKC_ENTRY_COUNT];
extern Club D_8020B9F8_ovl_05[];
extern s32 D_8020BA60_ovl_05;

#ifdef NON_MATCHING
/*
 * A cut-down copy of LaunchBallSimulation. It always takes the club test's
 * entry, picks a lie blend of 0, leaves out the slopes, rain and wind, and
 * prints the launch velocity to the debug output.
 *
 * Nonmatching: GCC schedules the block that sets the spin differently, which
 * moves the unk40 and unk38 stores and swaps two float registers there.
 */
s32 func_8020A488_ovl_05(BallSimulation* simulation, s32 purpose, Unk20* shot) {
  s32 percentRange[2];
  Club* club;
  Unk6* entry;
  s32 groundHeight;
  s32 speed;
  s32 sideSign;
  s32 rumbles;
  f32 pitch;
  f32 yawRadians;
  f32 blend;
  f32 spinBase;
  f32 spinLoss;
  s32 index;
  s8 surface;

  f32 PickLieBlend(void) {
    return 0.0f;
  }

  simulation->purpose = purpose;
  D_8020BA60_ovl_05++;
  sideSign = 1;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    groundHeight = GetGroundHeight(simulation->position[AXIS_X],
                                   simulation->position[AXIS_Z]) -
                   1.536f;
    if (groundHeight < simulation->position[AXIS_Y]) {
      simulation->position[AXIS_Y] = groundHeight;
    }
  }
  simulation->startPosition[AXIS_X] = simulation->unkC[AXIS_X] =
      simulation->position[AXIS_X];
  simulation->startPosition[AXIS_Y] = simulation->unkC[AXIS_Y] =
      simulation->position[AXIS_Y];
  simulation->startPosition[AXIS_Z] = simulation->unkC[AXIS_Z] =
      simulation->position[AXIS_Z];
  simulation->unkA7 = simulation->unkA2 = simulation->surface;
  simulation->unkA1 = 0;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    simulation->playerIndex = shot->unk18;
  } else {
    simulation->playerIndex = 0;
  }
  club = &D_8020B9F8_ovl_05[shot->clubIndex];
  entry = &D_8020B86C_ovl_05[0][UNKC_ENTRY_LIMIT];
  rumbles = D_8020B86C_ovl_05[0][UNKC_ENTRY_LIMIT].unk5;
  if ((club->surfaceMask & CLUB_SURFACE_GREEN) &&
      simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    speed = GetPuttPower(shot->unk8, shot->shotMode);
  } else {
    speed = GetClubPower(club, shot->shotMode) *
            (shot->unk8 + entry->unk0 * 2) / 256;
    if (speed <= 0) {
      speed = 10;
    }
  }
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    SurfaceInfo* info = GetSurfaceInfo(simulation->unkA2);

    blend = PickLieBlend();
    GetSurfacePercentRange(info, club, percentRange);
    speed = (percentRange[0] * blend + percentRange[1] * (1.0f - blend)) *
            speed * 0.01f;
  } else {
    s32 percent = 198; /* The local is there to match */

    speed = speed * percent / 200;
  }
  pitch = (club->pitch + entry->unk2) * 0.017453294f;
  if (pitch >= 0.0f) {
    pitch = 0.0f;
  }
  simulation->unk38 = (((sideSign * entry->unk3 + shot->unk10) * 2 +
                        (club->sideSpin * 2) * sideSign) *
                       speed) *
                      0.00078125f;
  pitch += (f32)club->unk10 * shot->unk14 * 0.00054541545f;
  yawRadians = (u16)shot->unk0 * BINARY_ANGLE_TO_RADIANS;
  spinBase = club->spin * 0.5f;
  simulation->unk34 = ((spinBase + 25.0f) * (shot->unk14 * 0.006f) - spinBase) *
                      speed * 0.0078125f;
  simulation->unk40 = 0.0f;
  simulation->unk3C = yawRadians;
  if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
    spinLoss = simulation->unk34 * 0.2f;
    if (spinLoss < 0.0f) {
      spinLoss = -spinLoss;
    }
    speed = speed - spinLoss;
  }
  if (speed < 0) {
    speed = 0;
  }
  if (speed <= 0) {
    speed = 10;
  }
  simulation->velocity[AXIS_Y] = sinf(pitch) * speed;
  simulation->velocity[AXIS_X] = cosf(yawRadians) * speed;
  simulation->velocity[AXIS_Z] = sinf(yawRadians) * speed;
  osSyncPrintf("SVX%f, SVY%f, SVZ%f\n",
               simulation->velocity[AXIS_X] * 0.00007030687f,
               simulation->velocity[AXIS_Y] * 0.00007030687f,
               simulation->velocity[AXIS_Z] * 0.00007030687f);
  if (club->surfaceMask & CLUB_SURFACE_GREEN) {
    simulation->isRunning = BALL_STATE_ROLLING;
    simulation->velocity[AXIS_Y] += PUTT_VELOCITY_Y_OFFSET;
  } else {
    simulation->isRunning = BALL_STATE_FLYING;
  }
  simulation->unk94 = 0;
  simulation->unk98 = 0;
  simulation->unk9C = -2;
  simulation->unkA0 = 0;
  simulation->unkB0 = 0;
  for (index = 0; index < UNKA8_COUNT; index++) {
    simulation->unkA8[index] = 0;
  }
  simulation->windVelocityX = 0.0f;
  simulation->windVelocityZ = 0.0f;
  if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
    if (rumbles) {
      StartPlayerRumble(&g_Players[simulation->playerIndex]);
    }
    if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
        IsGameFlagSet(GAME_FLAG_SHOW_LAUNCH_VALUES)) {
      char text[128];

      sprintf(text, "ANGLEY = %3.3d/360", (s32)(yawRadians * 57.295776f));
      PrintDebugText(text, 3, 6);
      sprintf(text, "ANGLEX = %3.3d/360", (s32)(pitch * 57.295776f));
      PrintDebugText(text, 3, 7);
      sprintf(text, "Velocity = %fm/s     ", speed * 0.001953125f);
      PrintDebugText(text, 3, 8);
      sprintf(text, "SpinY = %d", simulation->unk38);
      PrintDebugText(text, 3, 9);
      sprintf(text, "SpinX = %d", simulation->unk34);
      PrintDebugText(text, 3, 10);
      sprintf(text, "SpinVel = %fRPM      ", simulation->unk34 * 0.8741959f);
      PrintDebugText(text, 3, 11);
      sprintf(text, "SpinAngle = %3.3d/360",
              (s32)(simulation->unk3C * 57.295776f));
      PrintDebugText(text, 3, 12);
      sprintf(text, "SpinVelX = %fkm/h      ",
              simulation->unk34 * 0.0070306873f);
      PrintDebugText(text, 3, 13);
      sprintf(text, "SpinVelY = %fkm/h      ",
              simulation->unk38 * 0.0070306873f);
      PrintDebugText(text, 3, 14);
    }
  }
  return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A474_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A488_ovl_05);
#endif
