/*
 * lib_memory.c
 *
 * libmus's thin wrapper over the libaudio heap allocator, plus two small
 * byte-copy primitives. Every libmus working buffer is carved from one fixed
 * audio heap (audio_heap) with no individual frees, so allocation is just a
 * forward bump pointer. MemSet/MemMove are in-library replacements for
 * memset/memmove so libmus needs no libc.
 */
#include "libmus_config.h"
#include <ultra64.h>
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#endif
#include "lib_memory.h"

extern ALHeap
    audio_heap;  // the one heap every libmus allocation is carved from

/*
 * Zero the region [addr, addr+length) and initialize the libmus audio heap to
 * span it. Call once at startup, before any MemMalloc.
 */
void __MusIntMemInit(void* addr, int length) {
  __MusIntMemSet(addr, 0, length);
  alHeapInit(&audio_heap, addr, length);
}

/* Allocate length bytes from the audio heap (cache-aligned, never freed). */
void* __MusIntMemMalloc(int length) {
  return (alHeapAlloc(&audio_heap, 1, length));
}

/*
 * Bytes consumed from the heap so far: the bump pointer (cur) offset from the
 * heap base. The body returns bytes USED, not bytes free, despite the
 * "Remaining" name.
 */
int __MusIntMemRemaining(void) { return (audio_heap.cur - audio_heap.base); }

/* Address of the shared audio heap, for callers that drive alHeap* directly. */
ALHeap* __MusIntMemGetHeapAddr(void) { return (&audio_heap); }

/* Fill length bytes at dest with value (in-library memset). */
void __MusIntMemSet(void* dest, unsigned char value, int length) {
  unsigned char* a;
  a = dest;
  while (length--) *a++ = value;
}

/*
 * Copy length bytes from src to dest, safe for overlap (in-library memmove).
 * When src precedes dest the copy runs back-to-front so it never clobbers bytes
 * it has not read yet; otherwise it runs front-to-back.
 */
void __MusIntMemMove(void* dest, void* src, int length) {
  unsigned char *a, *b;
  a = dest;
  b = src;
  if (b < a) {
    a += length;
    b += length;
    while (length--) *--a = *--b;
  } else {
    while (length--) *a++ = *b++;
  }
}
