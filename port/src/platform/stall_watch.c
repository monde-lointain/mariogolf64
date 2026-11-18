/*
 * The stall watch: finds where a run in real time stalls.
 */

#include "stall_watch.h"

#include <SDL3/SDL_assert.h>

void StartStallWatch(StallWatch *watch, Uint64 nowNs, long gameFrame) {
    watch->lastRetraceNs = nowNs;
    watch->lastFrameNs = nowNs;
    watch->lastGameFrame = gameFrame;
}

// The gap from then to now, when it is long enough to be a stall, or 0
static Uint64 MeasureStall(Uint64 thenNs, Uint64 nowNs) {
    Uint64 gap = nowNs - thenNs;

    return gap >= STALL_THRESHOLD_NS ? gap : 0;
}

Stalls WatchRetrace(StallWatch *watch, Uint64 nowNs, long gameFrame) {
    Stalls stalls;

    SDL_assert(nowNs >= watch->lastRetraceNs);
    stalls.hostNs = MeasureStall(watch->lastRetraceNs, nowNs);
    stalls.gameNs = 0;
    watch->lastRetraceNs = nowNs;
    if (gameFrame != watch->lastGameFrame) {
        stalls.gameNs = MeasureStall(watch->lastFrameNs, nowNs);
        watch->lastFrameNs = nowNs;
        watch->lastGameFrame = gameFrame;
    }

    return stalls;
}
