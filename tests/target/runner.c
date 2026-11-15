/*
 * Runs every test group on the console and keeps Unity's report in memory.
 * The test driver, tools/run_target_tests.py, stops the console when it
 * reaches TargetTestsFinished and reads the report from
 * g_TargetTestReport: g_TargetTestReportLength characters, then the count
 * of failures in g_TargetTestFailures.
 */

#include <ultra64.h>

#include "system/c_language.h"
#include "unity_fixture.h"

// Room for the report; what doesn't fit is dropped, and the driver says so
#define REPORT_SIZE 0x10000

char g_TargetTestReport[REPORT_SIZE];
u32 g_TargetTestReportLength;
s32 g_TargetTestFailures = -1;

void RunTargetTests(void);
void TargetTestsFinished(void) __attribute__((noinline));

void TargetTestOutputChar(int character) {
    if (g_TargetTestReportLength < REPORT_SIZE) {
        g_TargetTestReport[g_TargetTestReportLength] = (char)character;
    }
    g_TargetTestReportLength++;
}

// Where the driver stops the console. The barrier keeps GCC from taking
// the call away, though the routine does nothing, and makes every store to
// the report come before it.
void TargetTestsFinished(void) {
    __asm__ volatile("" ::: "memory");
}

static void RunAllGroups(void) {
    RUN_TEST_GROUP(MathUtil);
}

void RunTargetTests(void) {
    // -v names each test as it starts, so a test that hangs or crashes the
    // console is the last that the report names
    const char *arguments[] = {"tests", "-v"};

    g_TargetTestFailures = UnityMain(ARRAY_COUNT(arguments), arguments, RunAllGroups);
    // The driver reads memory, not the CPU's cache
    osWritebackDCacheAll();
    TargetTestsFinished();
    for (;;) {
    }
}
