/*
 * The few checks that the port's tests need, which report each failure
 * where it happened and count them for the test's exit status.
 */

#ifndef PORT_TEST_SUPPORT_H
#define PORT_TEST_SUPPORT_H

#include <stdio.h>
#include <string.h>

extern int g_TestFailureCount;

#define EXPECT(condition) \
    ((condition) ? (void)0 : ReportTestFailure(__FILE__, __LINE__, #condition))

#define EXPECT_STRING(actual, expected) \
    ((strcmp((actual), (expected)) == 0) ? \
            (void)0 : \
            ReportStringMismatch(__FILE__, __LINE__, (actual), (expected)))

#define EXPECT_NEAR(actual, expected, tolerance) \
    ExpectNear(__FILE__, __LINE__, #actual, (double)(actual), (double)(expected), (tolerance))

void ReportTestFailure(const char *file, int line, const char *condition);
void ReportStringMismatch(const char *file, int line, const char *actual, const char *expected);
void ExpectNear(
    const char *file,
    int line,
    const char *expression,
    double actual,
    double expected,
    double tolerance
);

// Prints the outcome and gives main's exit status
int ReportTestResults(const char *testName);

#endif
