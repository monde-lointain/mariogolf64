/*
 * lib_memory.h
 *
 * Interface to libmus's audio-heap allocator and byte-copy helpers
 * (see lib_memory.c).
 */
#ifndef _LIBMUS_LIB_MEMORY_H_
#define _LIBMUS_LIB_MEMORY_H_

// Zero a region and initialize the audio heap over it; call once at startup.
void __MusIntMemInit(void* addr, int length);

// Address of the shared audio heap.
ALHeap* __MusIntMemGetHeapAddr(void);

// Allocate length bytes from the audio heap (cache-aligned, never freed).
void* __MusIntMemMalloc(int length);

// Bytes consumed from the heap so far (current offset from the heap base).
int __MusIntMemRemaining(void);

// In-library memset: fill length bytes at dest with value.
void __MusIntMemSet(void* dest, unsigned char value, int length);

// In-library memmove: copy length bytes src -> dest, overlap-safe.
void __MusIntMemMove(void* dest, void* src, int length);
#endif
