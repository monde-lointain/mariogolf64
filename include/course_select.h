#ifndef COURSE_SELECT_H
#define COURSE_SELECT_H

#include <ultra64.h>

#include "language.h"

Boolean ArePlanksOpen(void);
void DrawHoleSelect(Gfx **displayListHead);
void DrawRingShotSelect(Gfx **displayListHead);
void FreeCourseSelect(void);
void LoadCourseSelect(Boolean isHoleSelect);
void SetPlankMotion(s32 motion);
s32 UpdateHoleSelect(s32 pressedButtons, s32 repeatedButtons);
s32 UpdateRingShotSelect(s32 pressedButtons, u32 repeatedButtons);

#endif
