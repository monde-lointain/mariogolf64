/*
 * The record of the sanitized build (make SANITIZE=1), whose code checks
 * itself with GCC's undefined behavior sanitizer. Each check that fails
 * calls a handler in src/debug/sanitizer.c, which notes the failure here,
 * once for each place that fails, for tools/sanitize.py to read.
 */

#ifndef SANITIZER_H
#define SANITIZER_H

#include <ultra64.h>

// What a check found, by the handler that GCC calls for it
enum SanitizerCheck {
    UB_OUT_OF_BOUNDS, // An index past the end of an array
    UB_SHIFT, // A shift by a negative amount or by the width or more
    UB_DIVISION, // A division by zero
    UB_FLOAT_CAST, // A float out of the range of the integer it becomes
    UB_VLA_BOUND, // An array of variable length of no elements or fewer
    UB_CHECK_COUNT
};

typedef u32 SanitizerCheck;

// A place whose check failed. The operands are those of its first failure,
// as GCC passes them to the handler: the operand itself when it fits a
// word, or else where it was.
typedef struct SanitizerReport {
    u32 returnAddress; // Just past the handler's call
    u32 descriptor; // GCC's description of the check, kept in the ELF only
    SanitizerCheck check;
    u32 failureCount;
    u32 firstRetrace; // nuScRetraceCounter at the first failure
    u32 operand; // The index, the value shifted, the dividend, the float or the bound
    u32 otherOperand; // The shift's amount or the divisor, or else 0
} SanitizerReport;

// How many places the record holds; failures at others are only counted
#define SANITIZER_REPORT_MAX 256

extern SanitizerReport g_SanitizerReports[SANITIZER_REPORT_MAX];
extern u32 g_SanitizerReportCount;
extern u32 g_SanitizerUnrecordedCount; // Failures at places past the record's end

#endif
