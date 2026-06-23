/*
 * copy.c
 *
 * Byte-copy helper for the audio synthesis library. Used internally to clone
 * small structures such as ADPCM loop state where a generic memcpy is not
 * pulled in.
 */
#include <libaudio.h>

/* Copy len bytes from src to dest, forward and one byte at a time. */
void alCopy(void* src, void* dest, s32 len) {
  s32 i;
  u8* s = (u8*)src;
  u8* d = (u8*)dest;

  for (i = 0; i < len; i++) {
    *d++ = *s++;
  }
}
