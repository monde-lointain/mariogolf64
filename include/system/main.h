/*
 * The state that the main loop keeps for the whole game: the console's
 * memory, the frame count and rate, the debug switches and the build dates.
 */

#ifndef MAIN_H
#define MAIN_H

#include <ultra64.h>

#include "system/c_language.h"

// End of the RDRAM that every console has, without the Expansion Pak
#define BASE_RDRAM_END 0x80400000

// Values of g_FrameStepState, with which the second controller pauses
// the game (L on the control pad) and steps it a frame at a time
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

// Frames the game has drawn since boot
extern volatile s32 g_FrameCount;

// The longest build date that fits g_RomBuildDate with its terminator
#define BUILD_DATE_LENGTH_MAX 63
extern char g_RomBuildDate[BUILD_DATE_LENGTH_MAX + 1];
// The room before the next object; the game only ever empties it
#define SAVE_BUILD_DATE_SIZE 40
extern char g_SaveBuildDate[SAVE_BUILD_DATE_SIZE];

void ReportFailedAssertion(const char *expression, const char *fileName, s32 lineNumber);

void AssignCharacterBuffers(void);

#endif
