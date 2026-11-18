/*
 * The stall watch: finds where a run in real time stalls, so that a
 * player's report of a freeze can say what froze. Either the host reaches a
 * retrace late, as when presenting a frame or the host's other load holds
 * it up, or the game goes without a new frame, as when its own thread is
 * held up while the host's retraces carry on.
 *
 * The game reads its controllers once a frame, every second or third
 * retrace, and the host's retraces come every 17 ms, so a gap of
 * STALL_THRESHOLD_NS, six retraces, is a stall in either.
 */

#ifndef PLATFORM_STALL_WATCH_H
#define PLATFORM_STALL_WATCH_H

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#define STALL_THRESHOLD_NS (100 * SDL_NS_PER_MS)

typedef struct StallWatch {
    Uint64 lastRetraceNs; // When the host last reached a retrace
    Uint64 lastFrameNs; // When the game's frame last changed
    long lastGameFrame;
} StallWatch;

// The stalls that a retrace ends, each 0 when there is none
typedef struct Stalls {
    Uint64 hostNs; // How long the host took to reach this retrace
    Uint64 gameNs; // How long the game took to reach this frame
} Stalls;

// Starts watching from a retrace at nowNs, with the game at gameFrame
void StartStallWatch(StallWatch *watch, Uint64 nowNs, long gameFrame);

// Notes a retrace at nowNs, never before the last one, with the game at
// gameFrame. A game stall is reported once, when its frame comes.
Stalls WatchRetrace(StallWatch *watch, Uint64 nowNs, long gameFrame);

#endif
