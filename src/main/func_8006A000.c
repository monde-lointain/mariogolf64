#include "common.h"

// The main-segment math one-tu (vram 0x8006A000-0x8006A2C0): integer
// vector-magnitude helpers with range-scaling, the PRNG seed get/set, two
// single-precision magnitude helpers, a CRC16, and a div-error reporter.
//
// Kept as ONE translation unit (not split per function group) so its .rodata
// stays contiguous. func_8006A000's two 2^31 cast doubles 16-align this
// section, and KMC as pads its tail to 0x800D1440 where the NEXT TU's 8-aligned
// double sits. Splitting the format strings into their own .o would orphan that
// inter-TU alignment pad, since the asm .rodata sections are force-4-aligned by
// OBJCOPY_ALIGN (Makefile) and cannot self-8-align at a .o boundary.
//
// vector_magnitude_safe / calculate_hypotenuse_safe guard 32-bit overflow by
// right-shifting each component into [-2^29, 2^29) (bounds kept in locals so
// they hoist across the hand-rolled loop) and compute the magnitude in double
// precision (each u32 cast carries its own 2^31 rodata constant).
//
// All the sqrt-using helpers (calculate_*_safe's double sqrt,
// hypotf_2d / calc_vec3_magnitude's single sqrt.s) need the per-file
// -ffast-math override in mk/main.mk to emit the bare sqrt without the
// errno/NaN guard the ROM lacks. calc_vec3_magnitude's 3rd arg z arrives in a
// GPR (o32 passes only the first two float args in $f12/$f14) and is mtc1'd
// back for z*z.

extern u32 rng_seed;
extern f32 D_800C3FB4;

double sqrt(double);
extern f32 sqrtf(f32);

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

void update_rng_seed(void) { rng_seed = rng_seed * 0x5D588B65 + 1; }

f32 hypotf_2d(f32 a, f32 b) { return sqrtf(a * a + b * b); }

f32 calc_vec3_magnitude(f32 x, f32 y, f32 z) {
  return sqrtf(x * x + y * y + z * z);
}

u16 crc16_ccitt(u8* data, u32 len) {
  u32 crc = 0xFFFF;
  u32 idx;

  for (idx = 0; idx < len; idx++) {
    u32 i = 0;
    crc ^= data[idx];
    // Explicit goto loop: gcc 2.7.2 loop.c reverses any structured count-only
    // loop from 0 into a decrement-and-branch (check_dbra_loop), but the ROM
    // keeps the up-count. A goto loop carries no NOTE_INSN_LOOP notes, so
    // loop.c never sees it -> no reversal
    // (docs/hazards.md#top-tested-loop-goto-local-hoist). Both the for and
    // do-while natural forms were tried and both reversed.
  crc_bit:
    if (crc & 1) {
      crc = (crc >> 1) ^ 0x8408;
    } else {
      crc >>= 1;
    }
    if (++i < 8) {
      goto crc_bit;
    }
  }
  crc = ~crc;
  crc &= 0xFFFF;
  osSyncPrintf("CRC = %x\n", crc);
  return crc;
}

s32 report_div_error(void) {
  u32 ra;
  // Log the caller's return address ($ra). __builtin_return_address(0) can't be
  // used: this gcc has no RETURN_ADDR_RTX, so it lowers to a wrong stack-slot
  // load. A register-asm var reads $ra but reorders the prologue below the
  // string load, leaving an unfilled jal delay slot; the volatile asm barrier
  // keeps the prologue first so the addiu fills the delay slot (ROM order).
  __asm__ __volatile__("addu %0, $31, $0" : "=r"(ra));
  osSyncPrintf("kDivError %x\n", ra);
  return (s32)(1.0f / D_800C3FB4);
}
