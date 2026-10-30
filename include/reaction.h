#ifndef REACTION_H
#define REACTION_H

#include "common.h"

#include <ultra64.h>

#include "character.h"

/*
 * The reaction animations, picked from the strokes against par. The
 * eagle one also follows three under par.
 */
enum ReactionAnimation {
  REACTION_ANIMATION_EAGLE = 12,
  REACTION_ANIMATION_BIRDIE,
  REACTION_ANIMATION_PAR,
  REACTION_ANIMATION_BOGEY
};

s32 AddReactionLight(Gfx** displayListHead, Lights2* lights);
void StartReaction(u32 slotIndex, CharacterId character, s32 animation);
void TurnOffReactionLight(void);
NO_RESULT UpdateReaction(u32 slotIndex);
void ClearReactionSpot(void);
NO_RESULT ConsiderReactionSpot(CoursePoint point);
void GetReactionSpot(CoursePoint point);
void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale);

#endif
