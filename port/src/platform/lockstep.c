/*
 * Lockstep: how a headless run keeps the game in step with its retraces.
 */

#include "lockstep.h"

#include <SDL3/SDL.h>

#include "port/os_host.h"

// Says that the game is still busy, as it can be for many seconds under a
// sanitizer, so that a long wait isn't taken for a hang
static void WarnOfBusyGame(long retrace, long waitedMs) {
    const long seconds = waitedMs / 1000;

    if (retrace == LOCKSTEP_BOOT) {
        SDL_Log("The game is still booting, after %ld s", seconds);
    }
    else {
        SDL_Log("The game is still busy with retrace %ld, after %ld s", retrace, seconds);
    }
}

bool AwaitGameInLockstep(long retrace, bool (*shouldStop)(void)) {
    long busySlices = 0;

    while (!PcWaitForIdleCpu(LOCKSTEP_SLICE_MS)) {
        busySlices++;
        if (busySlices % LOCKSTEP_SLICES_PER_WARNING == 0) {
            WarnOfBusyGame(retrace, busySlices * LOCKSTEP_SLICE_MS);
        }

        if (shouldStop()) {
            return false;
        }
    }

    return true;
}
