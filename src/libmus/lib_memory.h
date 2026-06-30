/*
 * lib_memory.h
 *
 * Internal memory primitives for libmus, implemented in lib_memory.c. The
 * audio build runs without libc, so libmus manages its own storage: a thin
 * bump-pointer allocator over a single ALHeap arena (init, allocate, query,
 * and an accessor for the backing heap) plus in-library memset/memmove. Every
 * name is __MusInt*, marking it library-private.
 */
#ifndef _LIBMUS_LIB_MEMORY_H_
#define _LIBMUS_LIB_MEMORY_H_

/*
 * Take ownership of [addr, addr+length) as the audio heap arena: zero it, then
 * bind it to the ALHeap that backs every later __MusIntMemMalloc.
 */
void __MusIntMemInit(void* addr, int length);

/*
 * Return the backing ALHeap, for callers that hand it straight to an SDK
 * routine that allocates from a heap (e.g. the synth driver's alInit).
 */
ALHeap* __MusIntMemGetHeapAddr(void);

/*
 * Bump-allocate `length` bytes from the audio heap. The arena never frees, so
 * each block lives until the library is torn down.
 */
void* __MusIntMemMalloc(int length);

/*
 * Bytes consumed from the arena so far: the bump pointer's distance past the
 * arena base.
 */
int __MusIntMemRemaining(void);

/* memset: write `value` into `length` bytes at `dest`. */
void __MusIntMemSet(void* dest, unsigned char value, int length);

/*
 * memmove: copy `length` bytes from `src` to `dest`, picking the copy direction
 * so overlapping source and destination are copied intact.
 */
void __MusIntMemMove(void* dest, void* src, int length);

#endif
