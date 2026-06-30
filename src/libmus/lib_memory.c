/*
 * lib_memory.c
 *
 * libmus's private memory layer. The music driver runs without a C runtime, so
 * it owns one fixed DRAM region (audio_heap) and carves everything it needs
 * (sequences, banks, scratch buffers) from it through the libaudio bump-pointer
 * allocator: allocations grow forward only and are never individually freed.
 * The file also supplies the library's own memset/memmove replacements so that
 * dependency on libc can be dropped entirely.
 */

#include <ultra64.h>

/*
 * The audio backend is selected at build time: the stock libaudio synth, or the
 * SUPPORT_NAUDIO ("n-audio") variant. Either header supplies ALHeap and the
 * alHeap* allocator used below.
 */
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sn_sc.h>
#endif

#include "lib_memory.h"

/*
 * The single heap that backs every libmus allocation. It is defined and placed
 * elsewhere; __MusIntMemInit below lays the allocator over it.
 */
extern ALHeap audio_heap;

/*
 * Adopt the [addr, addr+length) region as the audio heap: clear it first so the
 * driver starts from known-zero memory, then initialize the bump-pointer
 * allocator over it.
 */
void __MusIntMemInit(void* addr, int length) {
  __MusIntMemSet(addr, 0, length);
  alHeapInit(&audio_heap, addr, length);
}

/*
 * Hand out length bytes from the audio heap. Yields 0 once the heap is
 * exhausted; the block can never be returned.
 */
void* __MusIntMemMalloc(int length) {
  return (alHeapAlloc(&audio_heap, 1, length));
}

/*
 * Report how many bytes have been carved from the heap so far: the distance the
 * bump pointer has advanced past the heap base.
 */
int __MusIntMemRemaining(void) { return (audio_heap.cur - audio_heap.base); }

/*
 * Expose the heap itself so callers can pass it straight to libaudio routines
 * that allocate (synthesizer, sequence player, ...).
 */
ALHeap* __MusIntMemGetHeapAddr(void) { return (&audio_heap); }

/* In-library memset: write value into length bytes starting at dest. */
void __MusIntMemSet(void* dest, unsigned char value, int length) {
  unsigned char* a;
  a = dest;

  while (length--) {
    *a++ = value;
  }
}

/*
 * In-library memmove: copy length bytes from src to dest, correct even when the
 * two regions overlap. When src precedes dest, a forward copy would overwrite
 * source bytes before reading them, so walk both pointers down from the end;
 * otherwise a plain forward copy is safe.
 */
void __MusIntMemMove(void* dest, void* src, int length) {
  unsigned char* a;
  unsigned char* b;
  a = dest;
  b = src;

  if (b < a) {
    a += length;
    b += length;
    while (length--) {
      *--a = *--b;
    }
  } else {
    while (length--) {
      *a++ = *b++;
    }
  }
}
