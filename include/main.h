#ifndef MAIN_H
#define MAIN_H

#include <ultra64.h>

#include "c_language.h"

#define BASE_RDRAM_END 0x80400000

enum FrameStep {
    FRAME_STEP_RUNNING,
    FRAME_STEP_ONE_FRAME,
    FRAME_STEP_PAUSED
};

typedef s8 FrameStep;

extern Boolean8 g_IsDebugEnabled;
extern Boolean g_HasExpansionPak;
extern Boolean g_HasThreeFrameBuffers;
extern s32 g_FramesPerSecond;
extern FrameStep g_FrameStepState;

extern volatile s32 g_FrameCount;

#define BUILD_DATE_LENGTH_MAX 63
extern char g_RomBuildDate[BUILD_DATE_LENGTH_MAX + 1];
#define SAVE_BUILD_DATE_SIZE 40
extern char g_SaveBuildDate[SAVE_BUILD_DATE_SIZE];

void ReportFailedAssertion(const char *expression, const char *fileName, s32 lineNumber);

void AssignCharacterBuffers(void);

#endif
