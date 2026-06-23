/*
 * memory.h -- raw memory block operations for the KMC C library.
 *
 * Declares the byte-block copy/compare/fill routines, including the legacy
 * Borland-style aliases (movmem/setmem) and the case-insensitive compare
 * (memicmp) that the standard <string.h> set does not provide.
 */
#ifndef _MEMORY_H
#define _MEMORY_H

// size_t guarded by a shared flag so the several headers that need it agree.
#ifndef _SIZE_T_DEF
#define _SIZE_T_DEF
typedef unsigned size_t;
#endif

void* memccpy(void*, void*, int, size_t);  // copy until a byte equals int, or n
void* memchr(void*, int, size_t);          // locate first byte equal to int
int memcmp(const void*, const void*, size_t);
void* memcpy(void*, const void*, size_t);
int memicmp(void*, void*, size_t);    // compare ignoring letter case
void* memmove(void*, void*, size_t);  // copy allowing overlapping regions
void* memset(void*, int, size_t);

// Legacy Borland/Turbo-C aliases; argument order differs from the mem* set.
void movmem(void*, void*, unsigned);  // block move of n bytes
void setmem(void*, unsigned, int);    // fill n bytes with a value: (dst, n, c)

#endif
