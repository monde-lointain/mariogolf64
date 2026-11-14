#ifndef REACTION_H
#define REACTION_H

#include <ultra64.h>

#include "character.h"
#include "compiler.h"
#include "course_space.h"
#include "geometry.h"

enum ReactionAnimation {
    REACTION_ANIMATION_EAGLE = 12,
    REACTION_ANIMATION_BIRDIE,
    REACTION_ANIMATION_PAR,
    REACTION_ANIMATION_BOGEY
};

typedef s32 ReactionAnimation;
typedef s8 ReactionAnimation8;

s32 AddReactionLight(Gfx **displayListHead, Lights2 *lights);
void StartReaction(u32 slotIndex, CharacterIndex characterIndex, s32 animationId);
void TurnOffReactionLight(void);
NO_RESULT UpdateReaction(u32 slotIndex);
void ClearReactionSpot(void);
NO_RESULT ConsiderReactionSpot(CoursePoint point);
void GetReactionSpot(CoursePoint point);
void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale);

#endif
