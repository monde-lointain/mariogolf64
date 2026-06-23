/*
 * Standard C string primitives used by the formatted-output code: locate a
 * character, measure a length, and copy a byte run.
 */

#include "PR/ultratypes.h"
#include "string.h"
#ident "$Revision: 1.23 $"

/*
 * Return a pointer to the first occurrence of c in s, or NULL if absent. The
 * terminating NUL counts as part of the string, so strchr(s, 0) finds the end.
 */
char* strchr(const char* s, int c) {
  // Narrow the search target to char so the comparison matches the bytes in s.
  const char ch = c;

  while (*s != ch) {
    if (*s == 0) {
      return NULL;
    }
    s++;
  }
  return (char*)s;
}

/* Return the number of characters in s before its terminating NUL. */
size_t strlen(const char* s) {
  const char* sc = s;

  while (*sc != 0) {
    sc++;
  }

  // The walked distance is the length, NUL excluded.
  return sc - s;
}

/* Copy n bytes from s2 to s1 and return s1. The buffers must not overlap. */
void* memcpy(void* s1, const void* s2, size_t n) {
  char* su1 = (char*)s1;
  const char* su2 = (const char*)s2;

  while (n > 0) {
    *su1 = *su2;
    su1++;
    su2++;
    n--;
  }
  return (void*)s1;
}
