/*
 * strcmp.c -- compare two NUL-terminated strings.
 *
 * The FAST_SPEED path compares a word (4 bytes) at a time when both strings
 * share the same word alignment, falling back to a byte loop otherwise. The
 * return sign matches the first differing byte, with a zero byte ending the
 * comparison; the word path reconstructs that byte-level result from the two
 * differing words.
 */
#include "_kmclib.h"
#include <memory.h>

int strcmp(s1, s2)
REG4 char* s1;
REG3 char* s2;
{
  REG1 UDWORD c, c1;
  REG2 int d;
#if FAST_SPEED
  // If the two pointers do not share word alignment, a word compare would fault
  // on MIPS, so compare byte by byte for the whole string.
  if (((char*)s1 - (char*)s2) & BUS_ERR_ALLIGN) {
    for (;;) {
      c = *((unsigned char*)s1)++;
      d = c - *((unsigned char*)s2)++;
      if (d || c == 0) return d;
    }
    return 0;
  }

  // Same alignment: step byte by byte up to the first word boundary so the bulk
  // loop can start aligned.
  while ((int)s1 & 3) {
    c = *((unsigned char*)s1)++;
    d = c - *((UBYTE*)s2)++;
    if (d || c == 0) return d;
  }

  // Bulk compare one aligned word at a time.
  for (;;) {
    c = *((UDWORD*)s1)++;
    d = c - (c1 = *((UDWORD*)s2)++);

    // The two words differ. Find the most significant byte position that holds
    // a NUL in c (big-endian, so the high byte is first): everything past the
    // terminator is irrelevant. Mask both words down to the bytes up to that
    // point and compare, so the sign reflects the first differing byte. If no
    // NUL precedes the difference, the raw word difference already has the
    // right sign.
    if (d) {
      if ((c & 0xff000000) == 0) {
        d = 0xff000000;
      } else if ((c & 0xff0000) == 0) {
        d = 0xffff0000;
      } else if ((c & 0xff00) == 0) {
        d = 0xffffff00;
      } else {
        return d;
      }
      return (int)((c & d) - (c1 & d));
    }

    // Words were equal: if any byte of that word is NUL, both strings ended
    // together and are equal (d is 0).
    if ((char)c == 0 || (c & 0xff00) == 0 || (c & 0xff0000) == 0 ||
        (c & 0xff000000) == 0) {
      return d;
    }
  }
#else
  // Portable fallback: byte-at-a-time compare.
  for (;;) {
    c = *((unsigned char*)s1)++;
    d = c - *((unsigned char*)s2)++;
    if (d || c == 0) return d;
  }
#endif
}
