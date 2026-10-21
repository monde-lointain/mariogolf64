#ifndef DEBUG_FLAGS_H
#define DEBUG_FLAGS_H

#include <ultra64.h>

/* A set of 256 switches for debugging features, numbered from 0 */

#define DEBUG_FLAG_COUNT 256

/* Silences the overlay manager's log */
#define DEBUG_FLAG_QUIET_OVERLAY_LOG 0x4D

s32 IsDebugFlagSet(u32 flag);
void SetDebugFlag(u32 flag);
void ClearDebugFlag(u32 flag);
void ToggleDebugFlag(u32 flag);
void ClearAllDebugFlags(void);

#endif
