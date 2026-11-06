#ifndef CLUB_SLOTS_H
#define CLUB_SLOTS_H

#include <ultra64.h>

/* Club Slots, the slot machine that fills each player's bag before play */

void DrawClubSlots(Gfx** displayListHead);
void FreeClubSlots(void);
s32 GetClubSlotsState(void);
void LoadClubSlots(void);
s32 UpdateClubSlots(void);

#endif
