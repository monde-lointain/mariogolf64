/*
 * What Unity needs from a C library that the game's lacks
 */

#include <ultra64.h>

#include <string.h>

#include "unity_config.h"

// The first place that needle occurs in haystack, or NULL if it doesn't
char *strstr(const char *haystack, const char *needle) {
    const char *start;

    for (start = haystack;; start++) {
        size_t matched = 0;

        while (needle[matched] != '\0' && start[matched] == needle[matched]) {
            matched++;
        }
        if (needle[matched] == '\0') {
            return (char *)start;
        }
        if (*start == '\0') {
            return NULL;
        }
    }
}
