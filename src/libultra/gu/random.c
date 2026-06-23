/*
 * Pseudo-random 32-bit integer generator.
 *
 * A small self-seeding generator that folds the running seed through a multiply
 * and a shift each call. State is kept in a single file-static word.
 */

#include "guint.h"

// Running generator state; the initial value is an arbitrary nonzero seed.
static unsigned int xseed = 174823885;

/* Advance the seed and return the next value. The (x+1) multiply mixes the bits
 * nonlinearly; the >> 2 discards the low bits, which cycle most predictably. */
int guRandom(void) {
  unsigned int x;

  x = (xseed << 2) + 2;
  x *= (x + 1);
  x = x >> 2;
  xseed = x;
  return (x);
}
