#ifndef MAIN_H
#define MAIN_H

#include <ultra64.h>

/* Bytes in g_WorkArea, part of which holds the characters' buffers */
#define WORK_AREA_SIZE 0x827E0

/* Values of g_FrameStepState, with which the second controller pauses
   the game (L on the control pad) and steps it a frame at a time */
#define FRAME_STEP_RUNNING 0
#define FRAME_STEP_ONE_FRAME 1
#define FRAME_STEP_PAUSED 2

extern s32 g_HasExpansionPak;
extern s32 g_FramesPerSecond;
extern s8 g_FrameStepState;
extern s8 g_FramePacingMode;
extern u8 g_WorkArea[WORK_AREA_SIZE];

void ReportFailedAssertion(const char *expression, const char *fileName, s32 lineNumber);

#endif
