#ifndef DEBUG_FLAGS_H
#define DEBUG_FLAGS_H

#include <ultra64.h>

/* A set of 256 switches for debugging features, numbered from 0 */

#define DEBUG_FLAG_COUNT 256

/* Hides the billboards */
#define DEBUG_FLAG_HIDE_BILLBOARDS 0x0E

/* Silences the overlay manager's log */
#define DEBUG_FLAG_QUIET_OVERLAY_LOG 0x4D

/* Lets the second and fourth controllers open and close the inset view */
#define DEBUG_FLAG_INSET_VIEW_BUTTONS 0x2F

/* Shows and edits UpdateGame's sDebugMenuValues in one scene */
#define DEBUG_FLAG_DEBUG_MENU_VALUES 0x57

s32 IsDebugFlagSet(u32 flag);
void SetDebugFlag(u32 flag);
void ClearDebugFlag(u32 flag);
void ToggleDebugFlag(u32 flag);
void ClearAllDebugFlags(void);

#endif
