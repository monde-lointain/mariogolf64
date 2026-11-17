/*
 * Tests of the wait that keeps a headless run in step with the game, with
 * the scheduler's wait for an idle CPU replaced through a link seam.
 */

#include <SDL3/SDL.h>

#include "lockstep.h"
#include "port/os_host.h"
#include "test_support.h"

// How many of the waits for an idle CPU find it still busy, or -1 for all
static int f_BusyWaitCount;
static int f_WaitCount;
static int f_WaitTimeoutMs;
// How many times the stopping check says to go on before it says to stop,
// or -1 for always
static int f_GoOnCount;
static int f_StopCheckCount;
static int f_LogCount;
static char f_LastLog[256];

bool PcWaitForIdleCpu(int timeoutMs) {
    f_WaitTimeoutMs = timeoutMs;
    f_WaitCount++;
    return f_BusyWaitCount >= 0 && f_WaitCount > f_BusyWaitCount;
}

static bool ShouldStop(void) {
    f_StopCheckCount++;
    return f_GoOnCount >= 0 && f_StopCheckCount > f_GoOnCount;
}

static void RecordLog(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    (void)userdata;
    (void)category;
    (void)priority;
    f_LogCount++;
    SDL_strlcpy(f_LastLog, message, sizeof(f_LastLog));
}

// Starts a case where the game stays busy for busyWaitCount waits, and the
// run is stopped after goOnCount checks
static void BeginCase(int busyWaitCount, int goOnCount) {
    f_BusyWaitCount = busyWaitCount;
    f_GoOnCount = goOnCount;
    f_WaitCount = 0;
    f_WaitTimeoutMs = 0;
    f_StopCheckCount = 0;
    f_LogCount = 0;
    f_LastLog[0] = '\0';
}

// A game that is idle at once is waited for once, quietly
static void TestReturnsWhenIdleAtOnce(void) {
    BeginCase(0, -1);
    EXPECT(AwaitGameInLockstep(7, ShouldStop));
    EXPECT(f_WaitCount == 1);
    EXPECT(f_WaitTimeoutMs == LOCKSTEP_SLICE_MS);
    EXPECT(f_LogCount == 0);
}

// However long the game stays busy, the next retrace waits for it: one
// that came on regardless would find the game somewhere else than on the
// recording's host, and the run would go its own way. The slices on each
// side of the first warning, and a wait long enough for two, show when it
// warns.
static void TestWaitsForTheGameWithoutGivingUp(void) {
    BeginCase(LOCKSTEP_SLICES_PER_WARNING - 1, -1);
    EXPECT(AwaitGameInLockstep(7, ShouldStop));
    EXPECT(f_WaitCount == LOCKSTEP_SLICES_PER_WARNING);
    EXPECT(f_LogCount == 0);

    BeginCase(LOCKSTEP_SLICES_PER_WARNING, -1);
    EXPECT(AwaitGameInLockstep(7, ShouldStop));
    EXPECT(f_WaitCount == LOCKSTEP_SLICES_PER_WARNING + 1);
    EXPECT(f_LogCount == 1);

    BeginCase(2 * LOCKSTEP_SLICES_PER_WARNING + 5, -1);
    EXPECT(AwaitGameInLockstep(7, ShouldStop));
    EXPECT(f_WaitCount == 2 * LOCKSTEP_SLICES_PER_WARNING + 6);
    EXPECT(f_WaitTimeoutMs == LOCKSTEP_SLICE_MS);
    EXPECT(f_LogCount == 2);
}

// The warning says which retrace the game is busy with, and for how long
static void TestWarnsOfTheRetraceAndTheTime(void) {
    BeginCase(2 * LOCKSTEP_SLICES_PER_WARNING, -1);
    EXPECT(AwaitGameInLockstep(412, ShouldStop));
    EXPECT_STRING(f_LastLog, "The game is still busy with retrace 412, after 20 s");

    BeginCase(LOCKSTEP_SLICES_PER_WARNING, -1);
    EXPECT(AwaitGameInLockstep(LOCKSTEP_BOOT, ShouldStop));
    EXPECT_STRING(f_LastLog, "The game is still booting, after 10 s");
}

// A run that is told to stop, as by an interrupt from the terminal, stops
// waiting after the slice it's in, and says the game didn't finish
static void TestStopsWhenTold(void) {
    BeginCase(-1, 2);
    EXPECT(!AwaitGameInLockstep(7, ShouldStop));
    EXPECT(f_WaitCount == 3);
    EXPECT(f_StopCheckCount == 3);
}

// The check comes only after a wait that found the game busy
static void TestDoesntAskToStopWhenIdle(void) {
    BeginCase(0, 0);
    EXPECT(AwaitGameInLockstep(7, ShouldStop));
    EXPECT(f_StopCheckCount == 0);
}

int main(void) {
    SDL_SetLogOutputFunction(RecordLog, NULL);
    TestReturnsWhenIdleAtOnce();
    TestWaitsForTheGameWithoutGivingUp();
    TestWarnsOfTheRetraceAndTheTime();
    TestStopsWhenTold();
    TestDoesntAskToStopWhenIdle();
    return ReportTestResults("lockstep_test");
}
