/*
 * The handlers that the sanitized build's checks call (make SANITIZE=1).
 * Each notes the failure in the record (see debug/sanitizer.h) and returns,
 * so that the game goes on as an unchecked build would, and every failure
 * of a run is found in one run. As LLVM's ubsan_minimal runtime does, it
 * tells places apart by the handler's return address, and so keeps one
 * report for each, however many times it fails.
 *
 * GCC passes each handler a description of the check: its source location
 * and the types involved. The link keeps the descriptions in the ELF and
 * not in the ROM (see tools/shiftable_ld.py), so that the checked code
 * still fits before the main heap, and the handlers keep only their
 * addresses, which tools/sanitize.py looks up. This file is compiled
 * without the checks, which would otherwise call its own handlers.
 */

#include "debug/sanitizer.h"

#include <nusys.h>
#include <ultra64.h>

SanitizerReport g_SanitizerReports[SANITIZER_REPORT_MAX];
u32 g_SanitizerReportCount;
u32 g_SanitizerUnrecordedCount;

// GCC declares these itself, with void pointers for the descriptions and
// the values, and calls them from the checks that fail
void __ubsan_handle_out_of_bounds(void *descriptor, void *index);
void __ubsan_handle_shift_out_of_bounds(void *descriptor, void *shifted, void *amount);
void __ubsan_handle_divrem_overflow(void *descriptor, void *dividend, void *divisor);
void __ubsan_handle_float_cast_overflow(void *descriptor, void *value);
void __ubsan_handle_vla_bound_not_positive(void *descriptor, void *bound);

// Notes a failure of the check called from just before returnAddress
static void NoteFailure(
    SanitizerCheck check,
    void *returnAddress,
    void *descriptor,
    void *operand,
    void *otherOperand
) {
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    u32 reportIndex = 0;
    SanitizerReport *report;

    while (reportIndex < g_SanitizerReportCount &&
        g_SanitizerReports[reportIndex].returnAddress != (u32)returnAddress) {
        reportIndex++;
    }

    if (reportIndex < g_SanitizerReportCount) {
        g_SanitizerReports[reportIndex].failureCount++;
    }
    else if (g_SanitizerReportCount < SANITIZER_REPORT_MAX) {
        report = &g_SanitizerReports[g_SanitizerReportCount];
        report->returnAddress = (u32)returnAddress;
        report->descriptor = (u32)descriptor;
        report->check = check;
        report->failureCount = 1;
        report->firstRetrace = nuScRetraceCounter;
        report->operand = (u32)operand;
        report->otherOperand = (u32)otherOperand;
        g_SanitizerReportCount++;
    }
    else {
        g_SanitizerUnrecordedCount++;
    }

    osSetIntMask(mask);
}

void __ubsan_handle_out_of_bounds(void *descriptor, void *index) {
    NoteFailure(UB_OUT_OF_BOUNDS, __builtin_return_address(0), descriptor, index, NULL);
}

void __ubsan_handle_shift_out_of_bounds(void *descriptor, void *shifted, void *amount) {
    NoteFailure(UB_SHIFT, __builtin_return_address(0), descriptor, shifted, amount);
}

void __ubsan_handle_divrem_overflow(void *descriptor, void *dividend, void *divisor) {
    NoteFailure(UB_DIVISION, __builtin_return_address(0), descriptor, dividend, divisor);
}

void __ubsan_handle_float_cast_overflow(void *descriptor, void *value) {
    NoteFailure(UB_FLOAT_CAST, __builtin_return_address(0), descriptor, value, NULL);
}

void __ubsan_handle_vla_bound_not_positive(void *descriptor, void *bound) {
    NoteFailure(UB_VLA_BOUND, __builtin_return_address(0), descriptor, bound, NULL);
}
