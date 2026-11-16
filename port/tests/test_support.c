/*
 * The checks of test_support.h.
 */

#include "test_support.h"

#include <math.h>
#include <stdlib.h>

int g_TestFailureCount;

void ReportTestFailure(const char *file, int line, const char *condition) {
    fprintf(stderr, "%s:%d: expected %s\n", file, line, condition);
    g_TestFailureCount++;
}

void ReportStringMismatch(const char *file, int line, const char *actual, const char *expected) {
    fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", file, line, expected, actual);
    g_TestFailureCount++;
}

void ExpectNear(
    const char *file,
    int line,
    const char *expression,
    double actual,
    double expected,
    double tolerance
) {
    if (!(fabs(actual - expected) <= tolerance)) {
        fprintf(
            stderr,
            "%s:%d: expected %s to be %g (within %g), got %g\n",
            file,
            line,
            expression,
            expected,
            tolerance,
            actual
        );
        g_TestFailureCount++;
    }
}

int ReportTestResults(const char *testName) {
    if (g_TestFailureCount != 0) {
        fprintf(stderr, "%s: %d failure(s)\n", testName, g_TestFailureCount);
        return EXIT_FAILURE;
    }

    printf("%s: passed\n", testName);
    return EXIT_SUCCESS;
}
