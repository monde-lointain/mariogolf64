#ifndef GAME_FLAGS_H
#define GAME_FLAGS_H

#include <ultra64.h>

/*
 * A set of 256 switches, numbered from 0, which boot clears. Game code
 * sets and clears some of them as it runs, such as 0x07 and UNK1E. A flag
 * editor in the code at 1B4A0 sets or clears any of them; the flags that
 * nothing else sets, such as HIDE_BILLBOARDS, are debugging features.
 */

#define GAME_FLAG_COUNT 256

/* Hides the billboards */
#define GAME_FLAG_HIDE_BILLBOARDS 0x0E

/* Silences the overlay manager's log */
#define GAME_FLAG_QUIET_OVERLAY_LOG 0x4D

/* Lets the second and fourth controllers open and close the inset view */
#define GAME_FLAG_INSET_VIEW_BUTTONS 0x2F

/* Shows and edits UpdateGame's sDebugMenuValues in one scene */
#define GAME_FLAG_DEBUG_MENU_VALUES 0x57

/* Makes every character selectable */
#define GAME_FLAG_ALL_CHARACTERS 0x54

/* Set when a saved game is continued, among other places */
#define GAME_FLAG_UNK1E 0x1E

/* Logs the ID of each encoded asset that is opened */
#define GAME_FLAG_LOG_ENCODED_ASSETS 0x49

/* Cuts every round to two holes */
#define GAME_FLAG_TWO_HOLE_ROUNDS 0x3D

s32 IsGameFlagSet(u32 flag);
void SetGameFlag(u32 flag);
void ClearGameFlag(u32 flag);
void ToggleGameFlag(u32 flag);
void ClearAllGameFlags(void);

#endif
