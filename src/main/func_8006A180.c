#include "common.h"

// Head of the main-segment math one-tu (the [0x45580] tail of the func_8006A000
// group): the LCG seed-advance and a 2D single-precision magnitude helper.
//
// hypotf_2d emits a bare `sqrt.s` opcode. `sqrtf` is a KMC gcc 2.7.2 builtin
// (BUILT_IN_FSQRT) that expands inline to the hardware sqrt.s under -mips3; the
// default profile also emits a c.eq.s/bc1t NaN guard + `jal sqrt` errno
// fallback, which the ROM lacks. -ffast-math (per-file override in mk/main.mk)
// drops the guard, leaving the bare sqrt.s the ROM has (the single-precision
// analog of the func_8006A000.c double-sqrt -ffast-math fix).

extern u32 rng_seed;
extern f32 sqrtf(f32);

void update_rng_seed(void) { rng_seed = rng_seed * 0x5D588B65 + 1; }

f32 hypotf_2d(f32 a, f32 b) { return sqrtf(a * a + b * b); }
