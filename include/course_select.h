#ifndef COURSE_SELECT_H
#define COURSE_SELECT_H

#include <ultra64.h>

#include "language.h"

enum SelectResult {
    SELECT_IDLE,
    SELECT_MOVED,
    SELECT_BACK
};

typedef s32 SelectResult;

Boolean ArePlanksOpen(void);
void DrawHoleSelect(Gfx **displayListHead);
void DrawRingShotSelect(Gfx **displayListHead);
void FreeCourseSelect(void);
void LoadCourseSelect(Boolean isHoleSelect);
void SetPlankMotion(s32 motion);
SelectResult UpdateHoleSelect(s32 pressedButtons, s32 repeatedButtons);
s32 UpdateRingShotSelect(s32 pressedButtons, u32 repeatedButtons);

#endif
