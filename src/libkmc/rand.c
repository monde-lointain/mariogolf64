/*
 * rand.c -- the C-library pseudo-random generator: rand() and srand().
 *
 * A textbook linear congruential generator. The multiplier/increment pair and
 * the high-bit extraction are the classic ANSI-C reference recurrence; `next`
 * holds the generator state between calls (defined elsewhere in the library).
 */
#include "_kmclib.h"
#include <stdlib.h>

extern long next;

int rand() {
  // Advance the LCG, then return the upper bits (>>16): the high bits of an LCG
  // are far more random than the low bits, and masking to RAND_MAX clamps the
  // result to the promised 0..RAND_MAX range.
  next = (next * 1103515245) + 12345;
  return ((unsigned int)((next + 1) >> 16) & RAND_MAX);
  return 0;
}

/* Reseed the generator; the next rand() restarts the sequence from `seed`. */
void srand(seed) unsigned seed;
{
  next = seed;
}
