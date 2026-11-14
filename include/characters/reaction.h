#ifndef REACTION_H
#define REACTION_H

#include <ultra64.h>

#include "characters/character.h"
#include "course_space.h"
#include "math/geometry.h"
#include "system/c_language.h"
#include "system/compiler.h"

enum ReactionAnimation {
    REACTION_ANIMATION_EAGLE = 12,
    REACTION_ANIMATION_BIRDIE,
    REACTION_ANIMATION_PAR,
    REACTION_ANIMATION_BOGEY
};

typedef s32 ReactionAnimation;
typedef s8 ReactionAnimation8;

Boolean AddReactionLight(Gfx **displayListHead, Lights2 *lights);
void StartReaction(u32 slotIndex, CharacterIndex characterIndex, ReactionAnimation animation);
void TurnOffReactionLight(void);
NO_RESULT UpdateReaction(u32 slotIndex);
void ClearReactionSpot(void);
NO_RESULT ConsiderReactionSpot(CoursePoint point);
void GetReactionSpot(CoursePoint point);
void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale);

#endif
