#include "common.h"

// Integer vector-magnitude helpers with automatic range-scaling, plus the
// PRNG-seed setter. Both magnitude routines guard against 32-bit overflow by
// right-shifting every component until each lies within [-2^29, 2^29), tracking
// the shift count, then left-shifting the result back. The range test keeps its
// two bounds (OFF/BOUND) in locals so they stay hoisted in registers across the
// hand-rolled loop; the magnitude is computed in double precision and converted
// to u32 (the compiler emits the standard c.le.D/sub.D/trunc.w.D unsigned-cast
// sequence, so each cast carries its own 2^31 rodata constant).

extern u32 rng_seed;

double sqrt(double);

#define RANGE_OFF 0xE0000000
#define RANGE_BOUND 0xBFFFFFFF

u32 vector_magnitude_safe(s32 x, s32 y, s32 z) {
  u32 shift = 0;
  s32 off = RANGE_OFF;
  u32 bound = RANGE_BOUND;

loop:
  if (!(bound < (u32)(x + off))) goto scale;
  if (!(bound < (u32)(y + off))) goto scale;
  if (bound < (u32)(z + off)) goto done;
scale:
  shift++;
  x >>= 1;
  y >>= 1;
  z >>= 1;
  goto loop;
done:
  return (u32)sqrt((f64)x * (f64)x + (f64)y * (f64)y + (f64)z * (f64)z)
         << shift;
}

u32 calculate_hypotenuse_safe(s32 x, s32 y) {
  u32 shift = 0;
  s32 off = RANGE_OFF;
  u32 bound = RANGE_BOUND;

loop:
  if (!(bound < (u32)(x + off))) goto scale;
  if (bound < (u32)(y + off)) goto done;
scale:
  shift++;
  x >>= 1;
  y >>= 1;
  goto loop;
done:
  return (u32)sqrt((f64)x * (f64)x + (f64)y * (f64)y) << shift;
}

void set_rng_seed(u32 seed) { rng_seed = seed; }
