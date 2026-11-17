/*
 * Lockstep: how a headless run keeps the game in step with its retraces,
 * so that what the game computes doesn't depend on how fast the host runs.
 */

#ifndef PLATFORM_LOCKSTEP_H
#define PLATFORM_LOCKSTEP_H

#include <stdbool.h>

// The retrace number that stands for the game's boot, before the first
#define LOCKSTEP_BOOT (-1L)
// How long each wait for the game lasts before checking whether to stop
#define LOCKSTEP_SLICE_MS 1000
// How many slices pass between warnings that the game is still busy
#define LOCKSTEP_SLICES_PER_WARNING 10

/* Waits until the game has finished with the given retrace, or its boot,
   however long it takes, warning every so often that it's still busy.
   Asks shouldStop after each slice of waiting, and gives up if it says so.
   Tells whether the game finished. */
bool AwaitGameInLockstep(long retrace, bool (*shouldStop)(void));

#endif
