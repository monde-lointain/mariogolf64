/*
 * heapinit.c
 *
 * Sets up an audio DRAM heap. The audio library allocates everything it needs
 * (filters, state blocks, sample buffers) from this heap with alHeapAlloc;
 * allocations are bump-pointer only and are never individually freed.
 */
#include "synthInternals.h"
#include <libaudio.h>

/*
 * Initialize an ALHeap over the caller-supplied [base, base+len) region.
 * The heap is rounded forward to a cache-line boundary so that every
 * subsequent allocation can be kept 16-byte aligned for safe RSP/CPU sharing.
 */
void alHeapInit(ALHeap* hp, u8* base, s32 len) {
  /* Bytes needed to push base up to the next cache-line boundary. */
  s32 extraAlign = (AL_CACHE_ALIGN + 1) - ((s32)base & AL_CACHE_ALIGN);

  /* A full cache line of padding means base was already aligned; skip it. */
  if (extraAlign != AL_CACHE_ALIGN + 1)
    hp->base = base + extraAlign;
  else
    hp->base = base;

  hp->len = len;
  hp->cur = hp->base;
  hp->count = 0;
}
