#ifndef REACTION_H
#define REACTION_H

#include <ultra64.h>
#include "common.h"

/*
 * The reaction animations, picked from the strokes against par. The
 * eagle one also follows three under par.
 */
#define REACTION_ANIMATION_EAGLE 12
#define REACTION_ANIMATION_BIRDIE 13
#define REACTION_ANIMATION_PAR 14
#define REACTION_ANIMATION_BOGEY 15

s32 AddReactionLight(Gfx **displayListHead, Lights2 *lights);
void StartReaction(u32 slotIndex, s32 character, s32 animation);
void TurnOffReactionLight(void);
s32 UpdateReaction(u32 slotIndex);
void ClearReactionSpot(void);
s32 ConsiderReactionSpot(CoursePoint point);
void GetReactionSpot(CoursePoint point);
void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale);

#endif
