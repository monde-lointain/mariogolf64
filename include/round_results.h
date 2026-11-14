#ifndef ROUND_RESULTS_H
#define ROUND_RESULTS_H

#include <ultra64.h>

#include "controller.h"
#include "system/c_language.h"

enum UnlockNotice {
    UNLOCK_NOTICE_DONKEY_KONG,
    UNLOCK_NOTICE_MAPLE,
    UNLOCK_NOTICE_METAL_MARIO,
    UNLOCK_NOTICE_COURSES
};

typedef s32 UnlockNotice;

enum RoundResultsRequest {
    CLOSE_ROUND_RESULTS = -1,
    OPEN_ROUND_RESULTS
};

typedef s32 RoundResultsRequest;

void DrawResultsNotice(Gfx **displayListHead);
void DrawRoundResults(Gfx **displayListHead);
void FreeUnlockNotice(void);
void LoadUnlockNotice(UnlockNotice unlock);
void OpenRoundResults(RoundResultsRequest request);
Boolean UpdateRoundResults(ControllerState *controller);
Boolean UpdateUnlockNotice(void);

#endif
