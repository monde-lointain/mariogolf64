#ifndef ROUND_RESULTS_H
#define ROUND_RESULTS_H

#include <ultra64.h>

#include "controller.h"
#include "language.h"

void DrawResultsNotice(Gfx **displayListHead);
void DrawRoundResults(Gfx **displayListHead);
void FreeUnlockNotice(void);
void LoadUnlockNotice(s32 unlock);
void OpenRoundResults(s32 request);
Boolean UpdateRoundResults(ControllerState *controller);
Boolean UpdateUnlockNotice(void);

#endif
