#ifndef MAIN_H
#define MAIN_H

#include <ultra64.h>

/* Values of g_FrameStepState, with which the second controller pauses
   the game (L on the control pad) and steps it a frame at a time */
enum FrameStep {
  FRAME_STEP_RUNNING,
  FRAME_STEP_ONE_FRAME,
  FRAME_STEP_PAUSED
};

extern s8 g_IsDebugEnabled;
extern Boolean g_HasExpansionPak;
extern Boolean g_HasThreeFrameBuffers;
extern s32 g_FramesPerSecond;
extern s8 g_FrameStepState;

/* Frames the game has drawn since boot */
extern volatile s32 g_FrameCount;

void ReportFailedAssertion(
    const char* expression, const char* fileName, s32 lineNumber);

#endif
