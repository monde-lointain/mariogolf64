#ifndef LIBKMC_MEMORY_H
#define LIBKMC_MEMORY_H

/*
 * The KMC C library's memory routines that the game calls. libultra
 * defines its own memcpy, which the game links instead of the library's.
 */

#if !defined(_SIZE_T) && !defined(_SIZE_T_) && !defined(_SIZE_T_DEF)
#define _SIZE_T
#define _SIZE_T_DEF
typedef unsigned int size_t;
#endif

void* memcpy(void* dest, const void* src, size_t size);
void* memset(void* dest, int value, size_t size);

#endif
