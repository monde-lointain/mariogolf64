#ifndef COMPUTER_GOLFER_H
#define COMPUTER_GOLFER_H

#include <ultra64.h>

#include "shot/current_shot.h"

#define PLANNED_SHOT_UNFINISHED (-1)

void StartShotPlan(void);
s32 GetPlannedShot(CurrentShot *shot);

#endif
