#ifndef CLUB_SLOTS_H
#define CLUB_SLOTS_H

#include <ultra64.h>

#include "language.h"

void DrawClubSlots(Gfx **displayListHead);
void FreeClubSlots(void);
s32 GetClubSlotsState(void);
void LoadClubSlots(void);
Boolean UpdateClubSlots(void);

#endif
