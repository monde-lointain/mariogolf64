#ifndef ROUND_RESULTS_H
#define ROUND_RESULTS_H

#include <ultra64.h>

#include "controller.h"

/* The results that slide in when a round ends, and the notices of what
 * the round unlocked */

void DrawResultsNotice(Gfx** displayListHead);
void DrawRoundResults(Gfx** displayListHead);
void FreeUnlockNotice(void);
void LoadUnlockNotice(s32 unlock);
void OpenRoundResults(s32 request);
s32 UpdateRoundResults(ControllerState* controller);
s32 UpdateUnlockNotice(void);

#endif
