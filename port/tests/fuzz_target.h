/*
 * What a fuzz target defines, for libFuzzer or for fuzz_driver.c, and the
 * check of its properties, which aborts so that either reports the input.
 */

#ifndef PORT_TESTS_FUZZ_TARGET_H
#define PORT_TESTS_FUZZ_TARGET_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Runs one input; returns 0, as libFuzzer asks
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static inline void FailFuzzCheck(const char *file, int line, const char *condition) {
    fprintf(stderr, "%s:%d: expected %s\n", file, line, condition);
    abort();
}

#define FUZZ_CHECK(condition) \
    ((condition) ? (void)0 : FailFuzzCheck(__FILE__, __LINE__, #condition))

#endif
