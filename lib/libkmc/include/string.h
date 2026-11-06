#ifndef LIBKMC_STRING_H
#define LIBKMC_STRING_H

/*
 * The KMC C library's string routines that the game calls, and, as in
 * KMC's own string.h, its memory routines. libultra defines its own
 * strlen, which the game links instead of the library's.
 */

#include "memory.h"

int strcmp(const char* left, const char* right);
size_t strlen(const char* string);

#endif
