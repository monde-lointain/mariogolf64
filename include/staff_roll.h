#ifndef STAFF_ROLL_H
#define STAFF_ROLL_H

#include <ultra64.h>

/* The staff roll */

void LoadStaffRoll(void);
void UnloadStaffRoll(void);
void UpdateStaffRoll(void);

void DrawStaffRoll(Gfx** displayListHead);

#endif
