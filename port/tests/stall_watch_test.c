/*
 * Tests of the stall watch, which finds where a run in real time stalls:
 * the host reaching a retrace late, or the game going without a new frame.
 */

#include <SDL3/SDL.h>

#include "stall_watch.h"
#include "test_support.h"

// A retrace's period, near enough
#define RETRACE_NS (17 * SDL_NS_PER_MS)
// A start well past 0, so that a gap measured from 0 shows
#define START_NS (1000 * SDL_NS_PER_SECOND)

// Retraces at their period, with a new game frame every other one, are no
// stall, however long they go on
static void TestSteadyRetracesAreNoStall(void) {
    StallWatch watch;
    Stalls stalls;
    int i;

    StartStallWatch(&watch, START_NS, 0);
    for (i = 1; i <= 1000; i++) {
        stalls = WatchRetrace(&watch, START_NS + ((Uint64)i * RETRACE_NS), i / 2);
        EXPECT(stalls.hostNs == 0);
        EXPECT(stalls.gameNs == 0);
    }
}

// A host gap of the threshold is a stall, and one a nanosecond less isn't
static void TestHostStallsAtThreshold(void) {
    StallWatch watch;
    Stalls stalls;

    StartStallWatch(&watch, START_NS, 0);
    stalls = WatchRetrace(&watch, START_NS + STALL_THRESHOLD_NS - 1, 0);
    EXPECT(stalls.hostNs == 0);

    StartStallWatch(&watch, START_NS, 0);
    stalls = WatchRetrace(&watch, START_NS + STALL_THRESHOLD_NS, 0);
    EXPECT(stalls.hostNs == STALL_THRESHOLD_NS);
}

// The host's gap is from the retrace before, not from the start
static void TestHostGapIsFromLastRetrace(void) {
    StallWatch watch;
    Stalls stalls;

    StartStallWatch(&watch, START_NS, 0);
    (void)WatchRetrace(&watch, START_NS + RETRACE_NS, 0);
    stalls = WatchRetrace(&watch, START_NS + RETRACE_NS + STALL_THRESHOLD_NS - 1, 0);
    EXPECT(stalls.hostNs == 0);
    stalls = WatchRetrace(&watch, START_NS + (2 * STALL_THRESHOLD_NS) + RETRACE_NS, 0);
    EXPECT(stalls.hostNs == STALL_THRESHOLD_NS + 1);
}

// A game that holds its frame over steady retraces stalls for the whole
// time from its last frame, reported once, when the next frame comes
static void TestGameStallIsReportedWhenFrameComes(void) {
    StallWatch watch;
    Stalls stalls;
    int i;

    StartStallWatch(&watch, START_NS, 5);
    for (i = 1; i <= 10; i++) {
        stalls = WatchRetrace(&watch, START_NS + ((Uint64)i * RETRACE_NS), 5);
        EXPECT(stalls.hostNs == 0);
        EXPECT(stalls.gameNs == 0);
    }

    stalls = WatchRetrace(&watch, START_NS + (11 * RETRACE_NS), 6);
    EXPECT(stalls.hostNs == 0);
    EXPECT(stalls.gameNs == 11 * RETRACE_NS);
    stalls = WatchRetrace(&watch, START_NS + (12 * RETRACE_NS), 7);
    EXPECT(stalls.gameNs == 0);
}

// The game's gap is from its last frame, and a gap a nanosecond short of
// the threshold isn't a stall
static void TestGameGapIsFromLastFrame(void) {
    StallWatch watch;
    Stalls stalls;

    StartStallWatch(&watch, START_NS, 0);
    (void)WatchRetrace(&watch, START_NS + RETRACE_NS, 1);
    stalls = WatchRetrace(&watch, START_NS + RETRACE_NS + STALL_THRESHOLD_NS - 1, 2);
    EXPECT(stalls.gameNs == 0);
    (void)WatchRetrace(&watch, START_NS + (2 * STALL_THRESHOLD_NS), 2);
    stalls = WatchRetrace(&watch, START_NS + (2 * STALL_THRESHOLD_NS) + RETRACE_NS, 3);
    EXPECT(stalls.gameNs == STALL_THRESHOLD_NS + 1);
}

// A frame count that goes back, as when a new script resets it, is a new
// frame all the same
static void TestFrameGoingBackIsNewFrame(void) {
    StallWatch watch;
    Stalls stalls;

    StartStallWatch(&watch, START_NS, 9);
    (void)WatchRetrace(&watch, START_NS + (STALL_THRESHOLD_NS / 2), 9);
    stalls = WatchRetrace(&watch, START_NS + STALL_THRESHOLD_NS, 0);
    EXPECT(stalls.gameNs == STALL_THRESHOLD_NS);
}

int main(void) {
    TestSteadyRetracesAreNoStall();
    TestHostStallsAtThreshold();
    TestHostGapIsFromLastRetrace();
    TestGameStallIsReportedWhenFrameComes();
    TestGameGapIsFromLastFrame();
    TestFrameGoingBackIsNewFrame();
    return ReportTestResults("stall_watch_test");
}
