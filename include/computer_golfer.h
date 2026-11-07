#ifndef COMPUTER_GOLFER_H
#define COMPUTER_GOLFER_H

#include <ultra64.h>

#include "current_shot.h"

/* GetPlannedShot's result while the plan is unfinished */
#define PLANNED_SHOT_UNFINISHED (-1)

void StartShotPlan(void);
s32 GetPlannedShot(CurrentShot *shot);

#endif
