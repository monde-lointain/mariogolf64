/*
 * memset.c -- fill a block of memory with a byte: memset() and setmem().
 *
 * The FAST_SPEED path fills a word (4 bytes) at a time so a long run costs a
 * quarter of the stores, handling the unaligned head and the odd-byte tail
 * separately. setmem() is a legacy alias with the argument order swapped.
 */
#include "_kmclib.h"
#include <memory.h>

void* memset(dest, ch, n)
REG5 void* dest;
int ch;
REG4 size_t n;
{
  REG2 char* d;
#if FAST_SPEED
  REG1 UDWORD c;
  REG3 size_t n1;
  if (n == 0) {
    return dest;
  }

  // Replicate the fill byte across all four bytes of a word so a single word
  // store paints four bytes at once.
  c = (ch & 0xff);
  c |= c << 8;
  c |= c << 16;
  d = dest;

  // Fill the leading bytes one at a time until `d` reaches word alignment;
  // MIPS faults on an unaligned word/halfword store, so the bulk loop below
  // must start aligned.
  if ((int)d & 1) {
    *(d)++ = (BYTE)c;
    --n;
  }
  if (n >= 2) {
    if ((int)d & 2) {
      *((WORD*)d)++ = (WORD)c;
      n -= 2;
    }
  }

  // Bulk fill: one word per iteration for the n/4 aligned words.
  n1 = n >> 2;
  while (n1--) {
    *((DWORD*)d)++ = (DWORD)c;
  }

  // Drain the 0..3 leftover bytes (a halfword then a byte).
  if (n & 2) {
    *((WORD*)d)++ = (WORD)c;
  }
  if (n & 1) {
    *d = (BYTE)c;
  }
#else
  // Portable fallback: store one byte per iteration.
  d = dest;
  while (n--) {
    *(BYTE*)d++ = ch;
  }
#endif
  return dest;
}

/* Legacy spelling with (dest, length, fill) argument order. */
void setmem(dest, n, c) void* dest;
size_t n;
int c;
{
  memset(dest, c, n);
}
