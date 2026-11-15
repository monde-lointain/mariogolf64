/*
 * Unity's settings for the test ROM, whose tests run on the console with
 * the game's code. The game builds without the C library's headers, so
 * Unity takes the sizes of its types from here, and writes its report to
 * memory, where the test driver reads it (see runner.c).
 */

#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#define UNITY_EXCLUDE_STDINT_H
#define UNITY_EXCLUDE_LIMITS_H
#define UNITY_EXCLUDE_MATH_H
#define UNITY_INT_WIDTH 32
#define UNITY_LONG_WIDTH 32
#define UNITY_POINTER_WIDTH 32
#define UNITY_SUPPORT_64
#define UNITY_INCLUDE_DOUBLE

// The fixture's extras replace malloc to find leaks, and the game has none
#define UNITY_FIXTURE_NO_EXTRAS

// The fixture's name filters use strstr, which the game's C library lacks
// (see support.c)
char *strstr(const char *haystack, const char *needle);

// The report gives each test's time on the console, in milliseconds, so
// that slow tests stand out
#define UNITY_INCLUDE_EXEC_TIME
unsigned int TargetTestClockMs(void);
#define UNITY_CLOCK_MS() TargetTestClockMs()

void TargetTestOutputChar(int character);
#define UNITY_OUTPUT_CHAR(character) TargetTestOutputChar(character)
#define UNITY_OUTPUT_CHAR_HEADER_DECLARATION TargetTestOutputChar(int)

#endif
