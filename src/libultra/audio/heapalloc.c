/*
 * heapalloc.c
 *
 * Bump-pointer allocator for the audio DRAM heap. Returns cache-aligned
 * blocks and never reclaims them, so structures shared with the RSP stay
 * 16-byte aligned without per-block headers. Under _DEBUG it threads an
 * extra HeapInfo header onto each block so alHeapCheck can audit the heap.
 */
#include "synthInternals.h"
#include <libaudio.h>
#include <os_internal.h>
#include <ultraerror.h>

/*
 * Allocate num*size bytes from heap hp, rounded up to a cache line. Returns 0
 * when the heap is exhausted. file/line identify the call site for the debug
 * heap-consistency header; they are ignored in the non-debug build.
 */
void* alHeapDBAlloc(u8* file, s32 line, ALHeap* hp, s32 num, s32 size) {
  s32 bytes;
  u8* ptr = 0;

  /* Round the request up so the next allocation also starts cache-aligned. */
  bytes = ((num * size) + AL_CACHE_ALIGN) & ~AL_CACHE_ALIGN;
#ifdef _DEBUG
  hp->count++;
  bytes += sizeof(HeapInfo);
#endif

  /* Hand out the current top of the heap only if the block still fits. */
  if ((hp->cur + bytes) <= (hp->base + hp->len)) {
    ptr = hp->cur;
    hp->cur += bytes;
#ifdef _DEBUG
    /* Stamp the consistency header and record where this block came from. */
    ((HeapInfo*)ptr)->magic = AL_HEAP_MAGIC;
    ((HeapInfo*)ptr)->size = bytes;
    ((HeapInfo*)ptr)->count = hp->count;
    if (file) {
      ((HeapInfo*)ptr)->file = file;
      ((HeapInfo*)ptr)->line = line;
    } else {
      ((HeapInfo*)ptr)->file = (u8*)"unknown";
      ((HeapInfo*)ptr)->line = 0;
    }
    ptr += sizeof(HeapInfo);
#endif
  } else {
#ifdef _DEBUG
    __osError(ERR_ALHEAPNOFREE, 1, size);
#endif
  }
  return ptr;
}
