/*
 * Tests of the frames that the GPU may still be drawing, which keep the CPU
 * from running more than FRAMES_IN_FLIGHT frames ahead of it. Fences are
 * opaque, so the elements of an array stand in for them.
 */

#include <SDL3/SDL.h>

#include "frames_in_flight.h"
#include "test_support.h"

#define FENCE_COUNT 100

static char f_Fences[FENCE_COUNT];

static SDL_GPUFence *Fence(int index) {
    return (SDL_GPUFence *)&f_Fences[index];
}

// Until FRAMES_IN_FLIGHT frames are in flight, nothing needs a wait
static void TestFillingNeedsNoWait(void) {
    FramesInFlight frames;
    int i;

    SDL_zero(frames);
    for (i = 0; i < FRAMES_IN_FLIGHT; i++) {
        EXPECT(AddFrameInFlight(&frames, Fence(i)) == NULL);
    }
}

// Each frame past that hands back the oldest, in the order submitted, as
// the ring wraps around many times
static void TestFullHandsBackOldest(void) {
    FramesInFlight frames;
    int i;

    SDL_zero(frames);
    for (i = 0; i < FRAMES_IN_FLIGHT; i++) {
        (void)AddFrameInFlight(&frames, Fence(i));
    }

    for (i = FRAMES_IN_FLIGHT; i < FENCE_COUNT; i++) {
        EXPECT(AddFrameInFlight(&frames, Fence(i)) == Fence(i - FRAMES_IN_FLIGHT));
    }
}

// Shutting down takes what is left, oldest first, then nothing, and an
// emptied ring fills again as a new one does
static void TestTakeDrainsOldestFirst(void) {
    FramesInFlight frames;
    int i;

    SDL_zero(frames);
    EXPECT(TakeOldestFrameInFlight(&frames) == NULL);
    for (i = 0; i < FRAMES_IN_FLIGHT + 3; i++) {
        (void)AddFrameInFlight(&frames, Fence(i));
    }

    for (i = 3; i < FRAMES_IN_FLIGHT + 3; i++) {
        EXPECT(TakeOldestFrameInFlight(&frames) == Fence(i));
    }

    EXPECT(TakeOldestFrameInFlight(&frames) == NULL);
    EXPECT(TakeOldestFrameInFlight(&frames) == NULL);
    for (i = 10; i < 10 + FRAMES_IN_FLIGHT; i++) {
        EXPECT(AddFrameInFlight(&frames, Fence(i)) == NULL);
    }

    EXPECT(AddFrameInFlight(&frames, Fence(20)) == Fence(10));
}

// A frame taken early leaves room for one more before a wait
static void TestTakeMakesRoom(void) {
    FramesInFlight frames;
    int i;

    SDL_zero(frames);
    for (i = 0; i < FRAMES_IN_FLIGHT; i++) {
        (void)AddFrameInFlight(&frames, Fence(i));
    }

    EXPECT(TakeOldestFrameInFlight(&frames) == Fence(0));
    EXPECT(AddFrameInFlight(&frames, Fence(FRAMES_IN_FLIGHT)) == NULL);
    EXPECT(AddFrameInFlight(&frames, Fence(FRAMES_IN_FLIGHT + 1)) == Fence(1));
}

// Two frames, so that the CPU records one while the GPU draws the other
static void TestTwoFramesInFlight(void) {
    EXPECT(FRAMES_IN_FLIGHT == 2);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    TestFillingNeedsNoWait();
    TestFullHandsBackOldest();
    TestTakeDrainsOldestFirst();
    TestTakeMakesRoom();
    TestTwoFramesInFlight();
    return ReportTestResults("frames_in_flight_test");
}
